// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/netcharge_energy/NetChargeEnergy.cc
/// @brief An EnergyMethod that penalizes deviation from a desired net charge.
/// @details This energy method is inherently not pairwise decomposible.  However, it is intended for very rapid calculation,
/// and has been designed to plug into Alex Ford's modifications to the packer that permit it to work with non-pairwise scoring
/// terms.  It has also been modified to permit sub-regions of a pose to be scored.
/// @author Vikram K. Mulligan (vmullig@uw.edu).

// Unit headers
#include <core/energy_methods/NetChargeEnergy.hh>
#include <core/energy_methods/NetChargeEnergyCreator.hh>
#include <core/scoring/netcharge_energy/NetChargeEnergySetup.hh>

// Package headers
#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/scoring/EnergyMap.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/conformation/Residue.hh>
#include <core/chemical/ResidueType.hh>
#include <core/pose/Pose.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/netcharge_energy/NetChargeConstraint.hh>
#include <core/select/residue_selector/ResidueSelector.hh>

// Options system

// File I/O

// Other Headers
#include <basic/Tracer.hh>
#include <basic/citation_manager/CitationCollection.hh>
#include <basic/citation_manager/CitationManager.hh>
#include <utility/vector1.hh>
#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace energy_methods {

static basic::Tracer TR( "core.energy_methods.NetChargeEnergy" );

/// @brief This must return a fresh instance of the NetChargeEnergy class, never an instance already in use.
///
core::scoring::methods::EnergyMethodOP
NetChargeEnergyCreator::create_energy_method( core::scoring::methods::EnergyMethodOptions const &options ) const
{
	return utility::pointer::make_shared< NetChargeEnergy >( options );
}

/// @brief Defines the score types that this energy method calculates.
///
core::scoring::ScoreTypes
NetChargeEnergyCreator::score_types_for_method() const
{
	using namespace core::scoring;
	ScoreTypes sts;
	sts.push_back( netcharge );
	return sts;
}

/// @brief Options constructor.
///
NetChargeEnergy::NetChargeEnergy ( core::scoring::methods::EnergyMethodOptions const &options ) :
	parent1( utility::pointer::make_shared< NetChargeEnergyCreator >() ),
	parent2( ),
	setup_helpers_(),
	setup_helpers_for_packing_(),
	setup_helper_masks_for_packing_()
{
	//The following reads from disk the first time only, and caches the data in memory:
	setup_helpers_ = core::scoring::ScoringManager::get_instance()->get_cloned_netcharge_setup_helpers( options );
	if ( TR.Debug.visible() ) report();
}

/// @brief Copy constructor.
///
NetChargeEnergy::NetChargeEnergy( NetChargeEnergy const &src ) :
	parent1( utility::pointer::make_shared< NetChargeEnergyCreator >() ),
	parent2( src ),
	setup_helpers_(), //CLONE the helper data below; don't copy them.
	setup_helpers_for_packing_(), //CLONE these below, too -- don't copy them.
	setup_helper_masks_for_packing_(  src.setup_helper_masks_for_packing_ )
{
	for ( core::Size i=1, imax=src.setup_helpers_.size(); i<=imax; ++i ) {
		setup_helpers_.push_back( src.setup_helpers_[i]->clone() );
	}
	for ( core::Size i=1, imax=src.setup_helpers_for_packing_.size(); i<=imax; ++i ) {
		setup_helpers_for_packing_.push_back( src.setup_helpers_for_packing_[i]->clone() );
	}
}

/// @brief Default destructor.
///
NetChargeEnergy::~NetChargeEnergy() = default;

/// @brief Clone: create a copy of this object, and return an owning pointer
/// to the copy.
core::scoring::methods::EnergyMethodOP NetChargeEnergy::clone() const {
	return utility::pointer::make_shared< NetChargeEnergy >(*this);
}

/// @brief NetChargeEnergy is context-independent and thus indicates that no context graphs need to be maintained by
/// class Energies.
void NetChargeEnergy::indicate_required_context_graphs( utility::vector1< bool > & /*context_graphs_required*/ ) const
{
	//Do nothing.
	return;
}

/// @brief NetChargeEnergy is version 1.0 right now.
///
core::Size NetChargeEnergy::version() const
{
	return 1; // Initial versioning
}

/// @brief Actually calculate the total energy
/// @details Called by the scoring machinery.
void NetChargeEnergy::finalize_total_energy( core::pose::Pose & pose, core::scoring::ScoreFunction const &, core::scoring::EnergyMap & totals ) const
{
	//Number of residues:
	core::Size const nres( pose.size() );

	//Vector of residue owning pointers:
	utility::vector1< core::conformation::ResidueCOP > resvector;
	resvector.reserve(nres);

	//Populate the vector with const owning pointers to the residues:
	for ( core::Size ir=1; ir<=nres; ++ir ) {
		resvector.push_back( pose.residue(ir).get_self_ptr() );
	}

	//Get the core::scoring::netcharge_energy::NetChargeEnergySetup objects from the pose and append them to the setup_helpers_ list, making a new setup_helpers list:
	utility::vector1< core::scoring::netcharge_energy::NetChargeEnergySetupCOP > setup_helpers;
	utility::vector1< core::select::residue_selector::ResidueSubset > masks;
	get_helpers_from_pose( pose, setup_helpers, masks ); //Pulls core::scoring::netcharge_energy::NetChargeEnergySetupCOPs from pose; generates masks from ResidueSelectors simultaneously.
	runtime_assert( masks.size() == setup_helpers.size() ); //Should be guaranteed to be true.

	totals[ core::scoring::netcharge ] += calculate_energy( resvector, setup_helpers, masks ); //Using the vector of residue owning pointers, calculate the repeat energy (unweighted) and set the netcharge to this value.

	return;
}

/// @brief Calculate the total energy given a vector of const owning pointers to residues.
/// @details Called directly by the ResidueArrayAnnealingEvaluator during packer runs.  Requires
/// that set_up_residuearrayannealablenergy_for_packing() be called first.
core::Real
NetChargeEnergy::calculate_energy(
	utility::vector1< core::conformation::ResidueCOP > const &resvect,
	utility::vector1< core::Size > const &,
	core::Size const /*substitution_position = 0*/
) const {
	//for(core::Size i=1, imax=resvect.size(); i<=imax; ++i) { TR << i << ":\t" << resvect[i]->name() << "\t" << resvect[i]->seqpos() << std::endl; } //DELETE ME
	return calculate_energy( resvect, setup_helpers_for_packing_, setup_helper_masks_for_packing_);
}


/// @brief Calculate the total energy given a vector of const owning pointers to residues.
/// @details Called by finalize_total_energy() and during packer runs.
core::Real
NetChargeEnergy::calculate_energy(
	utility::vector1< core::conformation::ResidueCOP > const &resvect,
	utility::vector1< core::scoring::netcharge_energy::NetChargeEnergySetupCOP > const &setup_helpers,
	utility::vector1< core::select::residue_selector::ResidueSubset > const &masks
) const
{
	runtime_assert( setup_helpers.size() == masks.size() ); //Should always be true.

	core::Real outer_accumulator(0.0);

	for ( core::Size ihelper=1, ihelpermax=setup_helpers.size(); ihelper<=ihelpermax; ++ihelper ) { //loop through all setup_helper objects

		// Const owning pointer to the setup helper:
		core::scoring::netcharge_energy::NetChargeEnergySetupCOP helper( setup_helpers[ihelper] );

		// Number of residues:
		core::Size const nres( resvect.size() );

		// Loop through all residues and count charges in each masked region.
		signed long int netcharge_observed(0);
		for ( core::Size ir=1; ir<=nres; ++ir ) {
			if ( !masks[ihelper][ir] ) continue; //Skip residues that are masked.
			core::chemical::ResidueType const & curtype( resvect[ir]->type() );
			if ( curtype.net_formal_charge() != 0 ) netcharge_observed += curtype.net_formal_charge();
			else { //Assume single charge for POSITIVE_CHARGE or NEGATIVE_CHARGE properties, if no net formal charge is specified.
				if ( curtype.has_property( core::chemical::POSITIVE_CHARGE ) ) ++netcharge_observed;
				if ( curtype.has_property( core::chemical::NEGATIVE_CHARGE ) ) --netcharge_observed;
			}
		}

		outer_accumulator += helper->penalty( netcharge_observed );

	} //End loop through helper objects

	return outer_accumulator;
}


/// @brief Get a summary of all loaded data.
///
void NetChargeEnergy::report() const {
	if ( !TR.Debug.visible() ) return; //Do nothing if I don't have a tracer.

	TR.Debug << std::endl << "Summary of data loaded by NetChargeEnergy object:" << std::endl;

	for ( core::Size i=1, imax=setup_helper_count(); i<=imax; ++i ) {
		TR.Debug << "NetChargeEnergySetup #" << i << ":" << std::endl;
		TR.Debug << setup_helper_cop(i)->report();
	}

	TR.Debug << std::endl;

	TR.Debug.flush();

	return;
}

/// @brief Cache data from the pose in this EnergyMethod in anticipation of scoring.
///
void
NetChargeEnergy::set_up_residuearrayannealableenergy_for_packing (
	core::pose::Pose &pose,
	core::pack::rotamer_set::RotamerSets const &/*rotamersets*/,
	core::scoring::ScoreFunction const &/*sfxn*/
) {
	if ( TR.Debug.visible() ) TR.Debug << "Setting up the NetChargeEnergy for packing." << std::endl;

	get_helpers_from_pose( pose, setup_helpers_for_packing_, setup_helper_masks_for_packing_ );

	runtime_assert( setup_helper_masks_for_packing_.size() == setup_helpers_for_packing_.size() ); //Should be guaranteed to be true.

	if ( TR.Debug.visible() ) {
		for ( core::Size i=1, imax=setup_helper_masks_for_packing_.size(); i<=imax; ++i ) {
			TR.Debug << "Setup helper " << i << ":" << std::endl;
			TR.Debug << "Mask: ";
			for ( core::Size j=1, jmax=setup_helper_masks_for_packing_[i].size(); j<=jmax; ++j ) {
				TR.Debug << (setup_helper_masks_for_packing_[i][j] ? "1" : "0");
			}
			TR.Debug << std::endl;
			TR.Debug << setup_helpers_for_packing_[i]->report();
		}
		TR.Debug.flush();
	}
	return;
}

//////////////////////////////CITATION MANAGER FUNCTIONS/////////////////////////////////

/// @brief Provide the citation.
void
NetChargeEnergy::provide_citation_info(basic::citation_manager::CitationCollectionList & citations ) const {
	using namespace basic::citation_manager;
	CitationCollectionOP citation(
		utility::pointer::make_shared< CitationCollection >(
		"NetChargeEnergy",
		CitedModuleType::EnergyMethod
		)
	);
	citation->add_citation( CitationManager::get_instance()->get_citation_by_doi("10.1073/pnas.2012800118") );
	citations.add( citation );
}

/// @brief Given a pose, pull out the core::scoring::netcharge_energy::NetChargeEnergySetup objects stored in SequenceConstraints in the pose and
/// append them to the setup_helpers_ vector, returning a new vector.  This also generates a vector of masks simultaneously.
/// @param [in] pose The pose from which the core::scoring::netcharge_energy::NetChargeEnergySetupCOPs will be extracted.
/// @param [out] setup_helpers The output vector of core::scoring::netcharge_energy::NetChargeEnergySetupCOPs that is the concatenation of those stored in setup_helpers_ and those from the pose.
/// @param [out] masks The output vector of ResidueSubsets, which will be equal in size to the helpers vector.
/// @details The output vectors are first cleared by this operation.
void
NetChargeEnergy::get_helpers_from_pose(
	core::pose::Pose const &pose,
	utility::vector1< core::scoring::netcharge_energy::NetChargeEnergySetupCOP > &setup_helpers,
	utility::vector1< core::select::residue_selector::ResidueSubset > &masks
) const {
	setup_helpers.clear();
	masks.clear();
	if ( setup_helpers_.size() > 0 ) {
		setup_helpers = setup_helpers_; //Copy the setup_helpers_ list.
		masks.resize( setup_helpers_.size(), core::select::residue_selector::ResidueSubset( pose.size(), true ) ); //All of the helpers in the setup_helpers_ list should be applied globally.
	}

	core::Size const n_sequence_constraints( pose.constraint_set()->n_sequence_constraints() );
	if ( n_sequence_constraints > 0 ) {
		for ( core::Size i=1; i<=n_sequence_constraints; ++i ) {
			core::scoring::netcharge_energy::NetChargeConstraintCOP cur_cst( utility::pointer::dynamic_pointer_cast<core::scoring::netcharge_energy::NetChargeConstraint const>( pose.constraint_set()->sequence_constraint(i) ) );
			if ( !cur_cst ) continue; //Continue if this isn't an core::scoring::netcharge_energy::NetChargeConstraint.
			setup_helpers.push_back( cur_cst->netcharge_energy_setup() ); //Append the core::scoring::netcharge_energy::NetChargeEnergySetup object stored in the current sequence constraint to the list to be used.
			core::select::residue_selector::ResidueSelectorCOP selector( cur_cst->selector() ); //Get the ResidueSelector in the current sequence constraint object, if there is one.  (May be NULL).
			if ( selector ) { //If we have a ResidueSelector, generate a mask from the pose and store it in the masks list.
				masks.push_back( selector->apply( pose ) );
			} else { //If not, add an all-true mask
				masks.push_back( core::select::residue_selector::ResidueSubset( pose.size(), true ) );
			}
		}
	}

	runtime_assert( setup_helpers.size() == masks.size() ); //Should be guaranteed true.

	return;
}


} // scoring
} // core
