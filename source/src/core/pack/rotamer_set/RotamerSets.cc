// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/pack/RotamerSet/RotamerSets.cc
/// @brief  RotamerSets class implementation
/// @author Andrew Leaver-Fay (leaverfa@email.unc.edu)
/// @modified Vikram K. Mulligan (vmulligan@flatironinstitue.org) to allow multi-threaded interaction graph setup.

// Unit Headers
#include <core/pack/rotamer_set/RotamerSets.hh>
#include <core/pack/rotamer_set/RotamerLinks.hh>

// Package Headers
#include <core/pack/rotamer_set/RotamerSet.hh>
#include <core/pack/rotamer_set/RotamerSetOperation.hh>
#include <core/pack/rotamer_set/symmetry/SymmetricRotamerSet_.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/pack/rotamer_set/RotamerSetFactory.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/interaction_graph/InteractionGraphBase.hh>
#include <core/pack/interaction_graph/PrecomputedPairEnergiesInteractionGraph.hh>
#include <core/pack/interaction_graph/OnTheFlyInteractionGraph.hh>

#include <core/conformation/Residue.hh>
#include <core/chemical/AA.hh>
#include <core/chemical/VariantType.hh>
#include <utility/graph/Graph.hh>
#include <core/pose/Pose.hh>
#include <core/pose/variant_util.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionInfo.hh>
#include <core/scoring/LREnergyContainer.hh>
#include <core/scoring/methods/LongRangeTwoBodyEnergy.hh>

#include <core/io/pdb/pdb_writer.hh>
#include <core/pose/symmetry/util.hh>

#include <ObjexxFCL/format.hh>
#include <basic/Tracer.hh>

// C++
#include <fstream>

// ObjexxFCL headers
#include <ObjexxFCL/FArray2D.hh>

#include <utility/vector1.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/packing.OptionKeys.gen.hh>

//Headers for the Rosetta thread manager
#include <basic/thread_manager/RosettaThreadManager.hh>
#include <basic/thread_manager/RosettaThreadAssignmentInfo.hh>


static basic::Tracer TR( "core.pack.rotamer_set.RotamerSets", basic::t_info );


using namespace ObjexxFCL;


#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#endif // SERIALIZATION

namespace core {
namespace pack {
namespace rotamer_set {

RotamerSets::RotamerSets() = default;
RotamerSets::~RotamerSets() = default;

void
RotamerSets::dump_pdb( pose::Pose const & pose, std::string const & filename ) const
{
	// model 0 -- just the non-moving residues
	// model N -- Nth rotamer from each set
	using ObjexxFCL::format::I;

	// open file
	std::ofstream out( filename.c_str() );

	// write model 0
	Size model(0), atom_counter(0);

	out << "MODEL" << I(9,model) << '\n';
	for ( Size i=1; i<= pose.size(); ++i ) {
		if ( task_->pack_residue(i) ) continue;
		io::pdb::dump_pdb_residue( pose.residue(i), atom_counter, out );
	}
	out << "ENDMDL\n";

	while ( true ) {
		bool found_a_rotamer( false );
		++model;

		for ( Size ii=1; ii<= nmoltenres_; ++ii ) {
			//Size const resid( moltenres_2_resid[ ii ] );
			RotamerSetOP rotset( set_of_rotamer_sets_[ ii ] );
			if ( rotset->num_rotamers() >= model ) {
				if ( !found_a_rotamer ) {
					found_a_rotamer = true;
					out << "MODEL" << I(9,model) << '\n';
				}
				io::pdb::dump_pdb_residue( *(rotset->rotamer( model )), atom_counter, out );
			}
		}
		if ( found_a_rotamer ) {
			out << "ENDMDL\n";
		} else {
			break;
		}
	}
	out.close();
}

void
RotamerSets::set_task( task::PackerTaskCOP task)
{
	task_ = task;
	nmoltenres_ = task_->num_to_be_packed();
	total_residue_ = task_->total_residue();

	resid_2_moltenres_.resize( total_residue_ );
	moltenres_2_resid_.resize( nmoltenres_ );
	set_of_rotamer_sets_.resize( nmoltenres_ );
	nrotamer_offsets_.resize( nmoltenres_ );
	nrotamers_for_moltenres_.resize( nmoltenres_ );

	uint count_moltenres = 0;
	for ( uint ii = 1; ii <= total_residue_; ++ii ) {
		if ( task_->pack_residue( ii ) ) {
			++count_moltenres;
			resid_2_moltenres_[ ii ] = count_moltenres;
			moltenres_2_resid_[ count_moltenres ] = ii;
		} else {
			resid_2_moltenres_[ ii ] = 0; //sentinal value; Andrew, replace this magic number!
		}
	}
	debug_assert( count_moltenres == nmoltenres_ );

}

void
RotamerSets::build_rotamers(
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn,
	utility::graph::GraphCOP packer_neighbor_graph
)
{
	for ( uint ii = 1; ii <= nmoltenres_; ++ii ) {
		uint ii_resid = moltenres_2_resid_[ ii ];
		RotamerSetOP rotset( RotamerSetFactory::create_rotamer_set( pose ) );
		rotset->set_resid( ii_resid );
		rotset->build_rotamers( pose, sfxn, *task_, packer_neighbor_graph );
		set_of_rotamer_sets_[ ii ] = rotset;
	}


	Size asym_length = 0;
	// make sure we have a symmetric RotamerSet_ if we have a symmetric pose
	if ( core::pose::symmetry::is_symmetric( pose ) ) {
		if ( set_of_rotamer_sets_.size() > 0 ) {
			runtime_assert ( dynamic_cast< core::pack::rotamer_set::symmetry::SymmetricRotamerSet_ const * >( set_of_rotamer_sets_[ 1 ].get() ) );
		}
		// also extract the asymmetric unit length for rotamer link operation
		core::conformation::symmetry::SymmetryInfoCOP symm_info = core::pose::symmetry::symmetry_info(pose);
		asym_length = symm_info->num_independent_residues();
	} else {
		asym_length = pose.size();
	}


	// now build any additional rotamers that are dependent on placement of other rotamers
	for ( uint ii = 1; ii <= nmoltenres_; ++ii ) {
		set_of_rotamer_sets_[ ii ]->build_dependent_rotamers( *this, pose, sfxn, *task_, packer_neighbor_graph );
	}

	update_offset_data();

	if ( task_->rotamer_links_exist() ) {
		//check all the linked positions

		TR << "RotamerLinks detected!" << std::endl;

		bool quasiflag = false;
		//this quasisymmetry flag turns on a lot of bypasses below
		if ( basic::options::option[ basic::options::OptionKeys::packing::quasisymmetry]() == true ) {
			quasiflag = true;
			TR << "NOTICE: QUASISYMMETRIC PACKING IS TURNED ON in RotamerSets. (quasiflag = " << quasiflag  << ")" << std::endl;
		}

		if ( quasiflag == false ) {
			//adding code to define a template residue in link-residues that is processed after layer-design etc. does pruning. This works independent to where linkres is placed in the task-operator list
			for ( Size ii = 1; ii <= nmoltenres(); ++ii ) {
				Size resid = moltenres_2_resid(ii);
				if ( task_->rotamer_links()->get_template(resid) != resid ) {
					RotamerSetCOP bufferset = rotamer_set_for_residue(task_->rotamer_links()->get_template(resid));
					RotamerSetOP rotset( RotamerSetFactory::create_rotamer_set( pose ) );
					rotset->set_resid( resid );
					for ( auto const & itr : *bufferset ) {
						conformation::ResidueOP cloneRes( new conformation::Residue(*itr->clone()) );
						copy_residue_conenctions_and_variants(pose,cloneRes,resid, asym_length);
						rotset->add_rotamer(*cloneRes);

					}
					set_of_rotamer_sets_[ resid_2_moltenres_[ resid ] ] = rotset;
				}
			} //end addition template residue code
		} //if quasiflag == false

		utility::vector1<bool> visited(asym_length,false);
		int expected_rot_count = 0;

		for ( uint ii = 1; ii <= nmoltenres_; ++ii ) { //loop through each residue
			TR.Debug << "visiting residue " << moltenres_2_resid_[ii] << std::endl;
			utility::vector1<int> copies = task_->rotamer_links()->get_equiv(moltenres_2_resid_[ii]);

			if ( visited[ moltenres_2_resid_[ii] ] ) {
				TR.Debug << "residue " << moltenres_2_resid_[ii] << " already visited, skipping" << std::endl;
				continue;
			}

			int smallest_res = 0;
			int num_rot = 1000000;

			//Added to handle cases where no equivalent residue has been set. This shouldn't
			//happen except in special cases (my scenario: add rotamer links and then relax
			//with coordinate constraints, which adds virtual residues that have no equivalents
			//set
			if ( copies.size() == 0 ) {
				TR.Warning << "residue " << moltenres_2_resid_[ii] << " has no equivalent residues set!" << std::endl;
				smallest_res = moltenres_2_resid_[ii];
			}

			//turn on quasisymmetry stuff, don't take smallest rotamer set
			if ( quasiflag == true ) {
				num_rot = 0; //start num_rot at 0
				for ( uint jj = 1; jj <= copies.size(); ++jj ) { //loop through each copy
					visited[ copies[jj] ] = true; //marks this copy-group as visited, so a future round will not re-analyze
					for ( uint rr = 1; rr <= set_of_rotamer_sets_[ resid_2_moltenres_[ copies[jj] ] ]->num_rotamers(); ++rr ) { //loop through each rotamer in this copy
						if ( jj == 1 ) {
							TR.Debug << "skipping copies[1]" << std::endl;
							continue;
						}
						TR.Debug << "adding rotamer # " << rr << " from copies[" << jj << "] into copies[1]" << std::endl;
						conformation::ResidueOP cloneRes( new conformation::Residue(*(set_of_rotamer_sets_[ resid_2_moltenres_[ copies[jj] ] ]->rotamer(rr)->clone())) );
						set_of_rotamer_sets_[ resid_2_moltenres_[ copies[1] ] ]->add_rotamer( *cloneRes );
					}
					num_rot += set_of_rotamer_sets_[ resid_2_moltenres_[ copies[jj] ] ]->num_rotamers(); //add the current copy's # of rotamers into num_rot
				}
				smallest_res = copies[1]; //set "smallest_res" (one to be copied) to copies[1]
			} else { //original behavior
				for ( uint jj = 1; jj <= copies.size(); ++jj ) {
					visited[ copies[jj] ] = true;
					int buffer;
					buffer = set_of_rotamer_sets_[ resid_2_moltenres_[ copies[jj] ] ]->num_rotamers();
					if ( buffer <= num_rot ) {
						num_rot = buffer;
						smallest_res = copies[jj];
					}
				}
			}
			expected_rot_count += num_rot;

			//relace rotset with the smallest set AND fix connectivity
			RotamerSetCOP bufferset = rotamer_set_for_moltenresidue( resid_2_moltenres_[ smallest_res ]);

			for ( uint jj = 1; jj <= copies.size(); ++jj ) { //each copy

				if ( ( copies[jj] == smallest_res ) && ( quasiflag == false ) ) {
					//no need to overwrite itself
					continue;
				}

				RotamerSetOP smallset( RotamerSetFactory::create_rotamer_set( pose ) ) ;

				for ( auto const & itr : *bufferset ) { //go through each rotamer

					conformation::ResidueOP cloneRes( new conformation::Residue(*itr->clone()) );

					cloneRes->seqpos(copies[jj]); //sets sequence position to current copy

					//correct for connections if the smallset is from first or last
					//residues.  These positions don't have a complete connect record.

					if ( itr->seqpos() == 1 && copies[jj] != 1  ) {
						if ( cloneRes->has_variant_type( chemical::LOWER_TERMINUS_VARIANT ) ) {
							cloneRes = core::pose::remove_variant_type_from_residue( *cloneRes, chemical::LOWER_TERMINUS_VARIANT, pose);
						}
						cloneRes->residue_connection_partner(1, copies[jj]-1, 2);
						cloneRes->residue_connection_partner(2, copies[jj]+1, 1);
					} else if ( itr->seqpos() == 1 && copies[jj] == 1  ) {
						cloneRes->copy_residue_connections( pose.residue(copies[jj]));
					} else if ( itr->seqpos() == asym_length && copies[jj] != (int) asym_length ) {
						if ( cloneRes->has_variant_type( chemical::UPPER_TERMINUS_VARIANT ) ) {
							cloneRes = core::pose::remove_variant_type_from_residue( *cloneRes, chemical::UPPER_TERMINUS_VARIANT, pose);
						}
						cloneRes->residue_connection_partner(1, copies[jj]-1, 2);
						cloneRes->residue_connection_partner(2, copies[jj]+1, 1);
					} else if ( itr->seqpos() == asym_length && copies[jj] == (int) asym_length ) {
						cloneRes->copy_residue_connections( pose.residue(copies[jj]));
					} else {
						cloneRes->copy_residue_connections( pose.residue(copies[jj]));
					}

					//debug connectivity
					TR.Debug << "rotamer #: " << itr << " ";
					TR.Debug << "resconn1: " << cloneRes->connected_residue_at_resconn( 1 ) << " ";
					TR.Debug << cloneRes->residue_connection_conn_id(1);
					TR.Debug << " resconn2: " << cloneRes->connected_residue_at_resconn( 2 ) << " ";
					TR.Debug << cloneRes->residue_connection_conn_id(2);
					TR.Debug << " seqpos: " << cloneRes->seqpos() << " for " << copies[jj] << " cloned from " << itr->seqpos() << std::endl;

					using namespace core::chemical;

					//if pose is cyclic, it'll have cutpoint variants; in those cases,
					//leave alone
					if ( copies[jj]==1 && !pose.residue(copies[jj]).has_variant_type(CUTPOINT_UPPER) ) {
						cloneRes = core::pose::add_variant_type_to_residue( *cloneRes, chemical::LOWER_TERMINUS_VARIANT, pose);
						//std::cout << cloneRes->name()  << " of variant type lower? " << cloneRes->has_variant_type("LOWER_TERMINUS") << std::endl;
					}
					if ( copies[jj]== (int) asym_length && !pose.residue(copies[jj]).has_variant_type(CUTPOINT_LOWER) ) {
						cloneRes = core::pose::add_variant_type_to_residue( *cloneRes, chemical::UPPER_TERMINUS_VARIANT, pose);
						//std::cout << cloneRes->name() << " of variant type upper? " << cloneRes->has_variant_type("UPPER_TERMINUS") <<  std::endl;
					}

					cloneRes->place( pose.residue(copies[jj]), pose.conformation());

					smallset->add_rotamer(*cloneRes);

					TR.Debug << "smallset has " << smallset->num_rotamers() << std::endl;
				}
				smallset->set_resid(copies[jj]);
				set_of_rotamer_sets_[ resid_2_moltenres_[ copies[jj] ]] = smallset;
				TR.Debug << "replacing rotset at " << copies[jj] << " with smallest set from " << smallest_res << std::endl;
			}
		}
		TR << "expected rotamer count is " << expected_rot_count << std::endl;
	} //end RotamerLinks detected
	update_offset_data();

	// Allow the RotamerSetsOperations to modify this object
	for ( auto
			rotsetsop_iter = task_->rotamer_sets_operation_begin(),
			rotsetsop_end = task_->rotamer_sets_operation_end();
			rotsetsop_iter != rotsetsop_end; ++rotsetsop_iter ) {

		(*rotsetsop_iter)->alter_rotamer_sets( pose, sfxn, *task_, packer_neighbor_graph, *this );
		debug_assert( offset_data_up_to_date() );
	}
}

//fd add explicit rotamers at a position.  Deletes current rotamers at this position!
void
RotamerSets::set_explicit_rotamers(
	core::Size moltenresid,
	RotamerSetOP rotamers
) {
	set_of_rotamer_sets_[ moltenresid ] = rotamers;
}

void RotamerSets::copy_residue_conenctions_and_variants(pose::Pose const & pose, conformation::ResidueOP cloneRes, Size seqpos, Size asym_length){
	using namespace core::chemical;
	//initial setup & remove all variants
	cloneRes->clear_residue_connections();
	cloneRes->copy_residue_connections( pose.residue(seqpos));
	core::pose::remove_variant_type_from_residue( *cloneRes, chemical::LOWER_TERMINUS_VARIANT, pose);
	core::pose::remove_variant_type_from_residue( *cloneRes, chemical::UPPER_TERMINUS_VARIANT, pose);
	//standard cases

	if ( seqpos==1 ) {
		core::pose::add_variant_type_to_residue( *cloneRes, chemical::LOWER_TERMINUS_VARIANT, pose);
	}
	if ( seqpos == asym_length ) {
		core::pose::add_variant_type_to_residue( *cloneRes, chemical::UPPER_TERMINUS_VARIANT, pose);
	}
	//other cases
	if ( pose.residue(seqpos).has_variant_type(CUTPOINT_UPPER) ) {
		core::pose::add_variant_type_to_residue( *cloneRes, chemical::CUTPOINT_UPPER, pose);
	}
	cloneRes->place( pose.residue(seqpos), pose.conformation());
}

void
RotamerSets::update_offset_data()
{
	// count rotamers
	nrotamers_ = 0;
	for ( uint ii = 1; ii <= nmoltenres_; ++ii ) {
		nrotamers_for_moltenres_[ ii ] = set_of_rotamer_sets_[ii]->num_rotamers();
		nrotamers_ += set_of_rotamer_sets_[ii]->num_rotamers();
		if ( ii > 1 ) { nrotamer_offsets_[ ii ] = nrotamer_offsets_[ii - 1] + set_of_rotamer_sets_[ii - 1]->num_rotamers(); }
		else { nrotamer_offsets_[ ii ] = 0; }
	}

	moltenres_for_rotamer_.resize( nrotamers_ );
	uint count_rots_for_moltenres = 1;
	uint count_moltenres = 1;
	for ( uint ii = 1; ii <= nrotamers_; ++ii ) {
		moltenres_for_rotamer_[ ii ] = count_moltenres;
		if ( count_rots_for_moltenres == nrotamers_for_moltenres_[ count_moltenres ] ) {
			count_rots_for_moltenres = 1;
			++count_moltenres;
		} else {
			++count_rots_for_moltenres;
		}
	}
}

/// @brief Used as a debug assert to ensure that RotamerSetsOperations call update_offset_data()
/// @details Without this check, rotamer sets operations could potentially introduce very-hard-to-debug
///          glitches if they don't call update_offset_data().
bool
RotamerSets::offset_data_up_to_date() {

	// Everything else is derived from this. Checking this alone should be enough to verify that
	//  nothing has changed.
	utility::vector1< uint > old_nrotamers_for_moltenres = nrotamers_for_moltenres_;
	update_offset_data();

	return old_nrotamers_for_moltenres == nrotamers_for_moltenres_;
}

/// @brief Construct a vector of calculations for the one-body energies in the interaction graph, for
/// subsequent multi-threaded evaluation.
/// @details Each individual calculation is for all one-body energies at a packable position.  This isn't as granular
/// as possible, but more finely-grained parallelism would be harder to implement (and anyways the one-body energies
/// are much less expensive than the two-body energies).
/// @author Vikram K. Mulligan (vmulligan@flatironinstitue.org).
utility::vector1< basic::thread_manager::RosettaThreadFunction >
RotamerSets::construct_one_body_energy_work_vector(
	core::pose::Pose const & pose,
	core::scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	interaction_graph::InteractionGraphBaseOP ig,
	basic::thread_manager::RosettaThreadAssignmentInfo const & thread_assignment_info
) const {
	TR.Debug << "Constructing work vector for multi-threaded evaluation of onebody energies." << std::endl;
	utility::vector1< basic::thread_manager::RosettaThreadFunction > work_vector;
	work_vector.reserve( nmoltenres_ );

	//Loop through all packable positions:
	for ( core::Size i(1); i<=nmoltenres_; ++i ) {
		//Ensure that the interaction graph can store an energy for each rotamer at position i:
		set_of_rotamer_sets_[i]->update_rotamer_offsets(); //Call this now, in a single-threaded context, so that we don't do a thread-unsafe operation later.
		//ig->set_num_states_for_node( i, set_of_rotamer_sets_[i]->num_rotamers() );

		work_vector.push_back(
			std::bind(
			&interaction_graph::InteractionGraphBase::set_onebody_energies_multithreaded, ig.get(),
			i, set_of_rotamer_sets_[i], std::cref(pose), std::cref(scfxn), std::cref(*task_), packer_neighbor_graph, std::cref(thread_assignment_info)
			)
		);
	}
	debug_assert( nmoltenres_ == work_vector.size() ); //Should be true.
	return work_vector;
}

/// @brief Append to a vector of calculations that already contains one-body energy calculations additonal work units
/// for two-body energy calculations, for subsequent multi-threaded evaluation.
/// @details Each individual calculation is for the interaction of all possible rotamer pairs at two positions.  Again,
/// not as granular as conceivably possible, but easier to set up.
/// @note The work_vector vector is extended by this operation.
/// @author Vikram K. Mulligan (vmulligan@flatironinstitue.org).
void
RotamerSets::append_two_body_energy_computations_to_work_vector(
	core::pose::Pose const & pose,
	core::scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	interaction_graph::PrecomputedPairEnergiesInteractionGraphOP pig,
	utility::vector1< basic::thread_manager::RosettaThreadFunction > & work_vector,
	basic::thread_manager::RosettaThreadAssignmentInfo const & thread_assignment_info
) const {
	using namespace interaction_graph;
	using namespace scoring;

	TR.Debug << "Constructing work vector for multi-threaded evaluation of pairwise interaction energies." << std::endl;

	// Loop through all packable positions:
	for ( core::Size ii(1); ii <= nmoltenres_; ++ii ) {
		// The residue index for the nth packable postion:
		core::Size const ii_resid = moltenres_2_resid_[ ii ];

		// Iterate over packer neighbours:
		for ( utility::graph::Graph::EdgeListConstIter
				uli  = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_begin(),
				ulie = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_end();
				uli != ulie; ++uli ) {
			uint const jj_resid = (*uli)->get_second_node_ind();
			uint const jj = resid_2_moltenres_[ jj_resid ]; //pretend we're iterating over jj >= ii
			if ( jj == 0 ) continue; // Magic number -- zero has special meaning here.  (It's the signal that jj_resid is not "molten".)

			pig->add_edge( ii, jj );

			//NOTE: The behaviour of std::bind is to pass EVERYTHING by copy UNLESS std::cref or std::ref is used.  So in
			//the following, all of the std::pairs are passed by copy.
			work_vector.push_back(
				std::bind(
				&interaction_graph::PrecomputedPairEnergiesInteractionGraph::set_twobody_energies_multithreaded, pig.get(),
				std::make_pair( ii, jj ), std::make_pair( set_of_rotamer_sets_[ii], set_of_rotamer_sets_[jj] ), std::cref(pose), std::cref(scfxn),
				std::cref(thread_assignment_info), false, nullptr, std::pair< core::Size, core::Size >(0, 0), false, false
				)
			);
		}
	}

	// Iterate across the long range energy functions and use the iterators generated
	// by the LRnergy container object
	for ( auto
			lr_iter = scfxn.long_range_energies_begin(),
			lr_end  = scfxn.long_range_energies_end();
			lr_iter != lr_end; ++lr_iter ) {
		LREnergyContainerCOP lrec = pose.energies().long_range_container( (*lr_iter)->long_range_type() );
		if ( !lrec || lrec->empty() ) continue; // only score non-emtpy energies.
		// Potentially O(N^2) operation...

		// Loop over all packable residues:
		for ( core::Size ii(1); ii <= nmoltenres_; ++ii ) {

			// Get the residue index.
			core::Size const ii_resid( moltenres_2_resid_[ ii ] );

			//Loop over all residue neighbours:
			for ( ResidueNeighborConstIteratorOP
					rni = lrec->const_upper_neighbor_iterator_begin( ii_resid ),
					rniend = lrec->const_upper_neighbor_iterator_end( ii_resid );
					(*rni) != (*rniend); ++(*rni) ) {

				// Index of the other residue:
				core::Size const jj_resid( rni->upper_neighbor_id() );
				core::Size const jj( resid_2_moltenres_[ jj_resid ] ); //pretend we're iterating over jj >= ii
				if ( jj == 0 ) continue; // Andrew, remove this magic number! (It's the signal that jj_resid is not "molten".)

				core::Size const iiprime( ii < jj ? ii : jj );
				core::Size const jjprime( ii < jj ? jj : ii );

				// Ensure that the edge exists.
				if ( ! pig->get_edge_exists( iiprime, jjprime ) ) {
					pig->add_edge( iiprime, jjprime );
				}

				//NOTE: The behaviour of std::bind is to pass EVERYTHING by copy UNLESS std::cref or std::ref is used.  So in
				//the following, all of the std::pairs are passed by copy.
				work_vector.push_back(
					std::bind(
					&interaction_graph::PrecomputedPairEnergiesInteractionGraph::add_longrange_twobody_energies_multithreaded, pig.get(),
					std::make_pair( iiprime, jjprime), std::make_pair( set_of_rotamer_sets_[iiprime], set_of_rotamer_sets_[jjprime] ), std::cref(pose), *lr_iter, std::cref(scfxn),
					std::cref(thread_assignment_info), false, nullptr, std::pair< core::Size, core::Size >(0, 0), false, false
					)
				);
			}
		}
	}
}

/// @brief Precompute all rotamer pair and rotamer one-body energies, populating
/// the given interaction graph.
/// @note In the multithreaded case, this function requests that work be done in threads.  It
/// takes as input the number of threads to request.
void
RotamerSets::compute_energies(
	pose::Pose const & pose,
	scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	interaction_graph::InteractionGraphBaseOP ig,
	core::Size const threads_to_request
) {
	using namespace interaction_graph;
	using namespace scoring;

	ig->initialize( *this );

	//In the multithreaded case, we create a vector of computations to do.
	basic::thread_manager::RosettaThreadAssignmentInfo thread_assignment_info( basic::thread_manager::RosettaThreadRequestOriginatingLevel::CORE_PACK );
	utility::vector1< basic::thread_manager::RosettaThreadFunction > work_vector;
	work_vector = construct_one_body_energy_work_vector( pose, scfxn, packer_neighbor_graph, ig, thread_assignment_info );

	PrecomputedPairEnergiesInteractionGraphOP pig =
		utility::pointer::dynamic_pointer_cast< core::pack::interaction_graph::PrecomputedPairEnergiesInteractionGraph > ( ig );
	if ( pig ) {
		//We append to the vector of computations to do.
		append_two_body_energy_computations_to_work_vector( pose, scfxn, packer_neighbor_graph, pig, work_vector, thread_assignment_info );
		basic::thread_manager::RosettaThreadManager::get_instance()->do_work_vector_in_threads( work_vector, threads_to_request, thread_assignment_info );

		//In a single thread, finalize the edges (not threadsafe):
		pig->declare_all_edge_energies_final();
	} else {
		/// is this an on the fly graph?
		OnTheFlyInteractionGraphOP otfig = utility::pointer::dynamic_pointer_cast< core::pack::interaction_graph::OnTheFlyInteractionGraph > ( ig );
		if ( otfig ) {
			if ( (threads_to_request == 0 || threads_to_request > 1) && basic::thread_manager::RosettaThreadManager::total_threads() > 1 ) {
				TR.Warning << "Cannot pre-compute an on-the-fly interaction graph in threads.  Only precomputing single-body energies in threads." << std::endl;
			}
			basic::thread_manager::RosettaThreadManager::get_instance()->do_work_vector_in_threads( work_vector, threads_to_request, thread_assignment_info );
			prepare_otf_graph( pose, scfxn, packer_neighbor_graph, otfig );
			compute_proline_correction_energies_for_otf_graph( pose, scfxn, packer_neighbor_graph, otfig);
		} else {
			utility_exit_with_message("Unknown interaction graph type encountered in RotamerSets::compute_energies()");
		}
	}

#ifdef MULTI_THREADED
	TR << "Completed interaction graph pre-calculation in " << thread_assignment_info.get_assigned_total_thread_count() << " available threads (" << (thread_assignment_info.get_requested_thread_count() == 0 ? std::string( "all available" ) : std::to_string( thread_assignment_info.get_requested_thread_count() ) ) << " had been requested)." << std::endl;
#endif

}

void
RotamerSets::prepare_otf_graph(
	pose::Pose const & pose,
	scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	interaction_graph::OnTheFlyInteractionGraphOP otfig
)
{
	using namespace conformation;
	using namespace interaction_graph;
	using namespace scoring;

	FArray2D_bool sparse_conn_info( otfig->get_num_aatypes(),  otfig->get_num_aatypes(), false );

	/// Mark all-amino-acid nodes as worthy of distinguishing between bb & sc
	for ( Size ii = 1; ii <= nmoltenres_; ++ii ) {
		RotamerSetCOP ii_rotset = set_of_rotamer_sets_[ ii ];
		bool all_canonical_aas( true );
		for ( Size jj = 1, jje = ii_rotset->get_n_residue_types(); jj <= jje; ++jj ) {
			ResidueCOP jj_rotamer = ii_rotset->rotamer( ii_rotset->get_residue_type_begin( jj ) );
			if ( jj_rotamer->aa() > chemical::num_canonical_aas ) {
				all_canonical_aas = false;
			}
		}
		if ( all_canonical_aas ) {
			otfig->distinguish_backbone_and_sidechain_for_node( ii, true );
		}
	}

	for ( Size ii = 1; ii <= nmoltenres_; ++ii ) {
		//tt << "pairenergies for ii: " << ii << '\n';
		uint const ii_resid = moltenres_2_resid_[ ii ];
		for ( utility::graph::Graph::EdgeListConstIter
				uli  = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_begin(),
				ulie = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_end();
				uli != ulie; ++uli ) {
			uint const jj_resid = (*uli)->get_second_node_ind();
			uint const jj = resid_2_moltenres_[ jj_resid ]; //pretend we're iterating over jj >= ii
			if ( jj == 0 ) continue; // Andrew, remove this magic number!

			bool any_neighbors = false;

			if ( scfxn.any_lr_residue_pair_energy(pose, ii, jj) ) {
				otfig->note_long_range_interactions_exist_for_edge( ii, jj );
				// if there are any lr interactions between these two residues, assume, all
				// residue type pairs interact
				any_neighbors = true;
				sparse_conn_info = true;
			} else {

				// determine which groups of rotamers are within their interaction distances
				RotamerSetCOP ii_rotset = set_of_rotamer_sets_[ ii ];
				RotamerSetCOP jj_rotset = set_of_rotamer_sets_[ jj ];

				sparse_conn_info = false;
				Distance const sfxn_reach = scfxn.max_atomic_interaction_cutoff();
				for ( Size kk = 1, kk_end = ii_rotset->get_n_residue_groups(); kk <= kk_end; ++kk ) {
					Size kk_example_rotamer_index = ii_rotset->get_residue_group_begin( kk );
					conformation::Residue const & kk_rotamer( *ii_rotset->rotamer( kk_example_rotamer_index ) );

					Distance const kk_reach = kk_rotamer.nbr_radius();
					Vector const kk_nbratom( kk_rotamer.xyz( kk_rotamer.nbr_atom() ) );

					for ( Size ll = 1, ll_end = jj_rotset->get_n_residue_groups(); ll <= ll_end; ++ll ) {
						Size ll_example_rotamer_index = jj_rotset->get_residue_group_begin( ll );
						conformation::Residue const & ll_rotamer( *jj_rotset->rotamer( ll_example_rotamer_index ) );

						Distance const ll_reach( ll_rotamer.nbr_radius() );
						Vector const ll_nbratom( ll_rotamer.xyz( ll_rotamer.nbr_atom() ) );

						DistanceSquared const nbrdist2 = kk_nbratom.distance_squared( ll_nbratom );
						Distance const intxn_cutoff = sfxn_reach + kk_reach + ll_reach;

						if ( nbrdist2 <= intxn_cutoff * intxn_cutoff ) {
							any_neighbors = true;
							sparse_conn_info( ll, kk ) = true; // these two are close enough to interact, according to the score function.
						}
					}
				}
			}

			if ( any_neighbors ) {
				otfig->add_edge( ii, jj );
				otfig->note_short_range_interactions_exist_for_edge( ii, jj );
				otfig->set_sparse_aa_info_for_edge( ii, jj, sparse_conn_info );
			}
		}
	}

	sparse_conn_info = true;
	// Iterate across the long range energy functions and use the iterators generated
	// by the LRnergy container object
	for ( auto
			lr_iter = scfxn.long_range_energies_begin(),
			lr_end  = scfxn.long_range_energies_end();
			lr_iter != lr_end; ++lr_iter ) {
		LREnergyContainerCOP lrec = pose.energies().long_range_container( (*lr_iter)->long_range_type() );
		if ( !lrec || lrec->empty() ) continue; // only score non-empty energies.
		// Potentially O(N^2) operation...

		for ( uint ii = 1; ii <= nmoltenres_; ++ ii ) {
			uint const ii_resid = moltenres_2_resid_[ ii ];

			for ( ResidueNeighborConstIteratorOP
					rni = lrec->const_upper_neighbor_iterator_begin( ii_resid ),
					rniend = lrec->const_upper_neighbor_iterator_end( ii_resid );
					(*rni) != (*rniend); ++(*rni) ) {
				Size const jj_resid = rni->upper_neighbor_id();

				uint const jj = resid_2_moltenres_[ jj_resid ]; //pretend we're iterating over jj >= ii
				if ( jj == 0 ) continue; // Andrew, remove this magic number! (it's the signal that jj_resid is not "molten")

				//RotamerSetCOP ii_rotset = set_of_rotamer_sets_[ ii ];
				//RotamerSetCOP jj_rotset = set_of_rotamer_sets_[ jj ];

				//std::cout << "preprare_oft_graph -- long range iterator = " << ii_resid << " " << jj_resid << std::endl;

				/// figure out how to notify the lrec that it should only consider these two residues neighbors
				/// for the sake of packing...
				if ( ! otfig->get_edge_exists( ii, jj ) ) {
					otfig->add_edge( ii, jj );
				}
				otfig->note_long_range_interactions_exist_for_edge( ii, jj );
				otfig->set_sparse_aa_info_for_edge( ii, jj, sparse_conn_info );

			}
		}
	}

}

/// @details
/// 0. Find example proline and glycine residues where possible.
/// 1. Iterate across all edges in the graph
///    2. Calculate proline-correction terms between neighbors
void
RotamerSets::compute_proline_correction_energies_for_otf_graph(
	pose::Pose const & pose,
	scoring::ScoreFunction const & scfxn,
	utility::graph::GraphCOP packer_neighbor_graph,
	interaction_graph::OnTheFlyInteractionGraphOP otfig
)
{
	using namespace conformation;

	utility::vector1< Size > example_gly_rotamers( nmoltenres_, 0 );
	utility::vector1< Size > example_pro_rotamers( nmoltenres_, 0 );
	utility::vector1< Size > example_reg_rotamers( nmoltenres_, 0 );

	/// 0. Find example proline and glycine residues where possible.
	for ( Size ii = 1; ii <= nmoltenres_; ++ii ) {
		RotamerSetCOP ii_rotset = set_of_rotamer_sets_[ ii ];
		Size potential_gly_replacement( 0 );
		for ( Size jj = 1, jje = ii_rotset->get_n_residue_types(); jj <= jje; ++jj ) {
			ResidueCOP jj_rotamer = ii_rotset->rotamer( ii_rotset->get_residue_type_begin( jj ) );
			if ( jj_rotamer->aa() == chemical::aa_gly ) {
				example_gly_rotamers[ ii ] = ii_rotset->get_residue_type_begin( jj );
			} else if ( jj_rotamer->aa() == chemical::aa_pro ) {
				example_pro_rotamers[ ii ] = ii_rotset->get_residue_type_begin( jj );
			} else if ( jj_rotamer->is_protein() ) {
				potential_gly_replacement = ii_rotset->get_residue_type_begin( jj );
				example_reg_rotamers[ ii ] = ii_rotset->get_residue_type_begin( jj );
			}
		}
		/// failed to find a glycine backbone for this residue, any other
		/// backbone will do, so long as its a protein backbone.
		if ( example_gly_rotamers[ ii ] == 0 ) {
			example_gly_rotamers[ ii ] = potential_gly_replacement;
		}
	}

	/// 1. Iterate across all edges in the graph
	for ( Size ii = 1; ii <= nmoltenres_; ++ ii ) {
		Size const ii_resid = moltenres_2_resid_[ ii ];
		if ( ! otfig->distinguish_backbone_and_sidechain_for_node( ii ) ) continue;
		for ( utility::graph::Graph::EdgeListConstIter
				uli  = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_begin(),
				ulie = packer_neighbor_graph->get_node( ii_resid )->const_upper_edge_list_end();
				uli != ulie; ++uli ) {
			Size const jj_resid = (*uli)->get_second_node_ind();
			Size const jj = resid_2_moltenres_[ jj_resid ]; //pretend we're iterating over jj >= ii
			if ( jj == 0 ) continue;
			if ( ! otfig->distinguish_backbone_and_sidechain_for_node( jj ) ) continue;
			/// 2. Calculate proline-correction terms between neighbors

			RotamerSetCOP ii_rotset = set_of_rotamer_sets_[ ii ];
			RotamerSetCOP jj_rotset = set_of_rotamer_sets_[ jj ];

			for ( Size kk = 1; kk <= ii_rotset->num_rotamers(); ++kk ) {
				core::PackerEnergy bb_bbregE( 0 ), bb_bbproE( 0 ), bb_bbglyE( 0 );
				core::PackerEnergy sc_regbb_energy( 0 ), sc_probb_energy( 0 ), sc_glybb_energy( 0 );

				//calc sc_npbb_energy;
				if ( example_reg_rotamers[ jj ] != 0 ) {
					bb_bbregE       = get_bb_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_reg_rotamers[ jj ] ) );
					sc_regbb_energy = get_sc_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_reg_rotamers[ jj ] ));
				}
				if ( example_gly_rotamers[ jj ] != 0 ) {
					bb_bbglyE       = get_bb_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_gly_rotamers[ jj ] )  );
					sc_glybb_energy = get_sc_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_gly_rotamers[ jj ] ) );
				}
				//calc sc_probb_energy
				if ( example_pro_rotamers[ jj ] != 0 ) {
					bb_bbproE       = get_bb_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_pro_rotamers[ jj ] )  );
					sc_probb_energy = get_sc_bbE( pose, scfxn, *ii_rotset->rotamer( kk ), *jj_rotset->rotamer( example_pro_rotamers[ jj ] ) );
				}
				otfig->add_to_one_body_energy_for_node_state( ii, kk, sc_regbb_energy +  0.5 * bb_bbregE  );
				otfig->set_ProCorrection_values_for_edge( ii, jj, ii, kk,
					bb_bbregE, bb_bbproE, sc_regbb_energy, sc_probb_energy );
				otfig->set_GlyCorrection_values_for_edge( ii, jj, ii, kk,
					bb_bbregE, bb_bbglyE, sc_regbb_energy, sc_glybb_energy );
			}

			for ( Size kk = 1; kk <= jj_rotset->num_rotamers(); ++kk ) {
				core::PackerEnergy bb_bbregE( 0 ), bb_bbproE( 0 ), bb_bbglyE( 0 );
				core::PackerEnergy sc_regbb_energy( 0 ), sc_probb_energy( 0 ), sc_glybb_energy( 0 );
				//calc sc_npbb_energy;
				if ( example_reg_rotamers[ ii ] != 0 ) {
					bb_bbregE       = get_bb_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_reg_rotamers[ ii ] ) );
					sc_regbb_energy = get_sc_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_reg_rotamers[ ii ] ));
				}
				if ( example_gly_rotamers[ ii ] != 0 ) {
					bb_bbglyE       = get_bb_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_gly_rotamers[ ii ] ) );
					sc_glybb_energy = get_sc_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_gly_rotamers[ ii ] ) );
				}
				//calc sc_probb_energy
				if ( example_pro_rotamers[ ii ] != 0 ) {
					bb_bbproE       = get_bb_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_pro_rotamers[ ii ] ) );
					sc_probb_energy = get_sc_bbE( pose, scfxn, *jj_rotset->rotamer( kk ), *ii_rotset->rotamer( example_pro_rotamers[ ii ] ) );
				}
				otfig->add_to_one_body_energy_for_node_state( jj, kk, sc_regbb_energy + 0.5 * bb_bbregE );

				otfig->set_ProCorrection_values_for_edge( ii, jj, jj, kk,
					bb_bbregE, bb_bbproE, sc_regbb_energy, sc_probb_energy );
				otfig->set_GlyCorrection_values_for_edge( ii, jj, jj, kk,
					bb_bbregE, bb_bbglyE, sc_regbb_energy, sc_glybb_energy );
			}
		}
	}
}


uint RotamerSets::nrotamers() const { return nrotamers_;}
uint RotamerSets::nrotamers_for_moltenres( uint mresid ) const
{
	return rotamer_set_for_moltenresidue( mresid )->num_rotamers();
}

uint RotamerSets::nmoltenres() const { return nmoltenres_;}

uint RotamerSets::total_residue() const { return total_residue_;}

uint
RotamerSets::moltenres_2_resid( uint mresid ) const { return moltenres_2_resid_[ mresid ]; }

uint
RotamerSets::resid_2_moltenres( uint resid ) const { return resid_2_moltenres_[ resid ]; }

uint
RotamerSets::moltenres_for_rotamer( uint rotid ) const { return moltenres_for_rotamer_[ rotid ]; }

uint
RotamerSets::res_for_rotamer( uint rotid ) const { return moltenres_2_resid( moltenres_for_rotamer( rotid ) ); }

core::conformation::ResidueCOP
RotamerSets::rotamer( uint rotid ) const
{
	return rotamer_set_for_residue( res_for_rotamer( rotid ) )->rotamer( rotid_on_moltenresidue( rotid ) );
}

core::conformation::ResidueCOP
RotamerSets::rotamer_for_moltenres( uint moltenres_id, uint rotamerid ) const
{
	return rotamer_set_for_moltenresidue( moltenres_id )->rotamer( rotamerid );
}


uint
RotamerSets::nrotamer_offset_for_moltenres( uint mresid ) const { return nrotamer_offsets_[ mresid ]; }

/// @brief rotamers_to_delete must be of size nrotmaers -- each position
/// in the array that's "true" is removed from the set of rotamers
void
RotamerSets::drop_rotamers( utility::vector1< bool > const & rotamers_to_delete ) {
	debug_assert( rotamers_to_delete.size() == nrotamers() );

	//For every residue, copy over the slice of booleans into their own vector and pass the logic down to the RotamerSet
	for ( uint i = 1; i <= nmoltenres(); ++i ) {
		utility::vector1< bool > rot_to_delete_mres( nrotamers_for_moltenres(i), false );
		uint const offset = nrotamer_offset_for_moltenres( i );
		//Slow but safe. This isn't performance critical but it is correctness critical
		for ( uint r = 1; r <= rot_to_delete_mres.size(); ++r ) {
			rot_to_delete_mres[ r ] = rotamers_to_delete[ offset + r ];
		}
		rotamer_set_for_moltenresidue( i )->drop_rotamers( rot_to_delete_mres );
	}

	update_offset_data();
	debug_assert( offset_data_up_to_date() );
}


/// @brief Does this RotamerSets object store a rotamer set for a residue at position resid
/// in the pose?
/// @details Rotamer sets for non-packable residues aren't generated, but could conceivably
/// be queried by protocols or energy functions.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
bool
RotamerSets::has_rotamer_set_for_residue(
	uint const resid
) const {
	return static_cast<bool>( resid_2_moltenres_[resid] ); //resid_2_moltenres_ maps to 0 for non-packable positions.
}

RotamerSetCOP
RotamerSets::rotamer_set_for_residue( uint resid ) const { return set_of_rotamer_sets_[ resid_2_moltenres( resid ) ]; }

RotamerSetOP
RotamerSets::rotamer_set_for_residue( uint resid )  { return set_of_rotamer_sets_[ resid_2_moltenres( resid ) ]; }

RotamerSetCOP
RotamerSets::rotamer_set_for_moltenresidue( uint moltenresid ) const { return set_of_rotamer_sets_[ moltenresid ]; }


RotamerSetOP
RotamerSets::rotamer_set_for_moltenresidue( uint moltenresid ) { return set_of_rotamer_sets_[ moltenresid ]; }

/// convert rotid in full rotamer enumeration into rotamer id on its source residue
uint
RotamerSets::rotid_on_moltenresidue( uint rotid ) const
{
	return rotid - nrotamer_offsets_[ moltenres_for_rotamer_[ rotid ] ];
}

/// convert moltenres rotid to id in full rotamer enumeration
uint
RotamerSets::moltenres_rotid_2_rotid( uint moltenres, uint moltenresrotid ) const
{
	return moltenresrotid + nrotamer_offsets_[ moltenres ];
}

/// access to packer_task_
task::PackerTaskCOP
RotamerSets::task() const
{
	return task_;
}

void
RotamerSets::prepare_sets_for_packing(
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn)
{
	update_offset_data();
	for ( Size ii = 1; ii <= nmoltenres(); ++ii ) {
		sfxn.prepare_rotamers_for_packing( pose, *set_of_rotamer_sets_[ ii ] );
	}
}

core::PackerEnergy
RotamerSets::get_bb_bbE(
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn,
	conformation::Residue const & res1,
	conformation::Residue const & res2
)
{
	scoring::EnergyMap emap;
	sfxn.eval_ci_2b_bb_bb( res1, res2, pose, emap );
	sfxn.eval_cd_2b_bb_bb( res1, res2, pose, emap );
	return static_cast< core::PackerEnergy > ( sfxn.weights().dot( emap ) );
}

core::PackerEnergy
RotamerSets::get_sc_bbE(
	pose::Pose const & pose,
	scoring::ScoreFunction const & sfxn,
	conformation::Residue const & res1,
	conformation::Residue const & res2
)
{
	scoring::EnergyMap emap;
	sfxn.eval_ci_2b_bb_sc( res2, res1, pose, emap );
	sfxn.eval_cd_2b_bb_sc( res2, res1, pose, emap );
	return static_cast< core::PackerEnergy > ( sfxn.weights().dot( emap ) );
}


void
RotamerSets::show( std::ostream & out ) const {
	out << "RotamerSets with " << nmoltenres_ << " molten residues for " << total_residue_ << " total residues and " << nrotamers_ << " rotamers." << std::endl;
	for ( core::Size ii(1); ii <= set_of_rotamer_sets_.size(); ++ii ) {
		out << ii << ": " << *(set_of_rotamer_sets_[ii]) << std::endl;
	}
}


} // namespace rotamer_set
} // namespace pack
} // namespace core


#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::pack::rotamer_set::RotamerSets::save( Archive & arc ) const {
	arc( cereal::base_class< core::pack::rotamer_set::FixbbRotamerSets >( this ) );
	arc( CEREAL_NVP( nmoltenres_ ) ); // uint
	arc( CEREAL_NVP( total_residue_ ) ); // uint
	arc( CEREAL_NVP( nrotamers_ ) ); // uint
	arc( CEREAL_NVP( set_of_rotamer_sets_ ) ); // RotamerSetVector
	arc( CEREAL_NVP( resid_2_moltenres_ ) ); // utility::vector1<uint>
	arc( CEREAL_NVP( moltenres_2_resid_ ) ); // utility::vector1<uint>
	arc( CEREAL_NVP( nrotamer_offsets_ ) ); // utility::vector1<uint>
	arc( CEREAL_NVP( moltenres_for_rotamer_ ) ); // utility::vector1<uint>
	arc( CEREAL_NVP( nrotamers_for_moltenres_ ) ); // utility::vector1<uint>
	arc( CEREAL_NVP( task_ ) ); // PackerTaskCOP
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::pack::rotamer_set::RotamerSets::load( Archive & arc ) {
	arc( cereal::base_class< core::pack::rotamer_set::FixbbRotamerSets >( this ) );
	arc( nmoltenres_ ); // uint
	arc( total_residue_ ); // uint
	arc( nrotamers_ ); // uint
	arc( set_of_rotamer_sets_ ); // RotamerSetVector
	arc( resid_2_moltenres_ ); // utility::vector1<uint>
	arc( moltenres_2_resid_ ); // utility::vector1<uint>
	arc( nrotamer_offsets_ ); // utility::vector1<uint>
	arc( moltenres_for_rotamer_ ); // utility::vector1<uint>
	arc( nrotamers_for_moltenres_ ); // utility::vector1<uint>
	std::shared_ptr< core::pack::task::PackerTask > local_task;
	arc( local_task ); // PackerTaskCOP
	task_ = local_task; // copy the non-const pointer(s) into the const pointer(s)
}

SAVE_AND_LOAD_SERIALIZABLE( core::pack::rotamer_set::RotamerSets );
CEREAL_REGISTER_TYPE( core::pack::rotamer_set::RotamerSets )

CEREAL_REGISTER_DYNAMIC_INIT( core_pack_rotamer_set_RotamerSets )
#endif // SERIALIZATION
