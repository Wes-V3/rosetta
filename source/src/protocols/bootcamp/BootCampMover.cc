// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/bootcamp/BootCampMover.cc
/// @brief Create a new mover subclass
/// @author sen (swei25@jhu.edu)

// Unit headers
#include <protocols/bootcamp/BootCampMover.hh>
#include <protocols/bootcamp/BootCampMoverCreator.hh>

// Core headers
#include <core/pose/Pose.hh>

// Basic/Utility headers
#include <basic/Tracer.hh>
#include <utility/tag/Tag.hh>
#include <utility/pointer/memory.hh>

// XSD Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

// Citation Manager
#include <utility/vector1.hh>
#include <basic/citation_manager/UnpublishedModuleInfo.hh>

// Monte Carlo
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/MonteCarlo.fwd.hh>

// Pose
#include <core/pose/Pose.hh>

// Score Function
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunction.hh>

// random
#include<numeric/random/random.hh>

// Packer
#include<core/pack/task/PackerTask.fwd.hh>
#include<core/pack/task/PackerTask.hh>
#include<core/pack/task/TaskFactory.hh>
#include<core/pack/pack_rotamers.hh>

// Movemap
#include<core/kinematics/MoveMap.hh>

// Minimizer
#include<core/optimization/MinimizerOptions.hh>
#include<core/optimization/AtomTreeMinimizer.hh>

// Self defined FoldTreeFromSS
#include<protocols/bootcamp/fold_tree_from_ss.hh>

// correctly_add_cutpoint_variants() 
#include<core/pose/variant_util.hh>

// JobDistributor
#include <protocols/jd2/JobDistributor.hh>

static basic::Tracer TR( "protocols.bootcamp.BootCampMover" );

namespace protocols {
namespace bootcamp {

	/////////////////////
	/// Constructors  ///
	/////////////////////

/// @brief Default constructor
BootCampMover::BootCampMover():
	protocols::moves::Mover( BootCampMover::mover_name() )
{

}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destructor (important for properly forward-declaring smart-pointer members)
BootCampMover::~BootCampMover(){}

////////////////////////////////////////////////////////////////////////////////
	/// Mover Methods ///
	/////////////////////

/// @brief Apply the mover
void
BootCampMover::apply( core::pose::Pose& pose ){
	
	core::pose::correctly_add_cutpoint_variants(pose); // add cutpoint variants
	
	// core::scoring::ScoreFunctionOP sfxn = core::scoring::get_score_function();
	
	// Enable a new score term linear_chainbreak and set the weight to be 1	
	// core::scoring::ScoreType new_linear_chainbreak = core::scoring::ScoreType::linear_chainbreak;
	// utility::vector1<core::Real> weight = {1.0};
	// sfxn->set_method_weights( new_linear_chainbreak, weight );

	core::Real score = sfxn_->score( pose );
	std::cout << "The score is: " << score << std::endl;

	core::Real temperature = 1;
	protocols::moves::MonteCarloOP mc ( new protocols::moves::MonteCarlo( pose, *sfxn_, temperature ) );
	
	// protocols::moves::PyMOLObserverOP the_observer = protocols::moves::AddPyMOLObserver( *mypose, true, 0 );

	core::Size N = pose.size();

	// Define MoveMap
	core::kinematics::MoveMap mm;
	mm.set_bb(true); // backbone dihedral angle degrees of freedom
	mm.set_chi(true); // sidechain dihedral angle degrees of freedom
	
	// Define MinimizerOptions
	core::optimization::MinimizerOptions min_opts( "lbfgs_armijo_atol", 0.01, true );

	// Define AtomTreeMinimizer
	core::optimization::AtomTreeMinimizer atm;

	// Declare copy_pose
	core::pose::Pose copy_pose;

	// Calculate and print out the acceptance rate
	core::Real acceptance_count = 0;
	// Calculate and print out the average score
	core::Real total_score = 0;

	// Create FoldTree
	pose.fold_tree(protocols::bootcamp::fold_tree_from_ss(pose));

	for (core::Size i = 0; i < num_iterations_; i++) {
		// Perturbation
		core::Real uniform_random_number = numeric::random::uniform();
		core::Size randres = static_cast< core::Size > (uniform_random_number * N + 1);
		core::Real pert1 = numeric::random::gaussian();
		core::Real pert2 = numeric::random::gaussian();
		core::Real orig_phi = pose.phi( randres );
		core::Real orig_psi = pose.psi( randres );
		pose.set_phi( randres, orig_phi + pert1 );
		pose.set_psi( randres, orig_psi + pert2 );

		// Packing and minimization
		core::pack::task::PackerTaskOP repack_task = core::pack::task::TaskFactory::create_packer_task( pose );
		repack_task->restrict_to_repacking();
		core::pack::pack_rotamers( pose, *sfxn_, repack_task );
		
		// Invoke minimization
		copy_pose = pose; // To avoid sending pose to PyMol over and over again which cause long runtime
		atm.run( copy_pose, mm, *sfxn_, min_opts );
		pose = copy_pose;

		if (mc->boltzmann( pose )) {
			acceptance_count++;
		};
		total_score = total_score + mc->last_accepted_score();
		
	}
	
	std::cout << "Acceptance rate: " << acceptance_count/num_iterations_ << " | Average score: " << total_score/num_iterations_ << std::endl;

	std::cout << "After MonteCarlo, the score is: " << sfxn_->score( pose ) << std::endl;
	// std::cout << N << " | " << randres << " | " << pert1 << " | " << pert2 << " | " << orig_phi << " | " << orig_psi << " | " << mypose->phi( randres ) << " | " << mypose->psi( randres ) << std::endl;
	
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Show the contents of the Mover
void
BootCampMover::show(std::ostream & output) const
{
	protocols::moves::Mover::show(output);
}

////////////////////////////////////////////////////////////////////////////////
	/// Rosetta Scripts Support ///
	///////////////////////////////

/// @brief parse XML tag (to use this Mover in Rosetta Scripts)
void
BootCampMover::parse_my_tag(
	utility::tag::TagCOP ,
	basic::datacache::DataMap&
) {

}
void BootCampMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{

	using namespace utility::tag;
	AttributeList attlist;

	//here you should write code to describe the XML Schema for the class.  If it has only attributes, simply fill the probided AttributeList.

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "Create a new mover subclass", attlist );
}


////////////////////////////////////////////////////////////////////////////////
/// @brief required in the context of the parser/scripting scheme
protocols::moves::MoverOP
BootCampMover::fresh_instance() const
{
	return utility::pointer::make_shared< BootCampMover >();
}

/// @brief required in the context of the parser/scripting scheme
protocols::moves::MoverOP
BootCampMover::clone() const
{
	return utility::pointer::make_shared< BootCampMover >( *this );
}

std::string BootCampMover::get_name() const {
	return mover_name();
}

std::string BootCampMover::mover_name() {
	return "BootCampMover";
}



/////////////// Creator ///////////////

protocols::moves::MoverOP
BootCampMoverCreator::create_mover() const
{
	return utility::pointer::make_shared< BootCampMover >();
}

std::string
BootCampMoverCreator::keyname() const
{
	return BootCampMover::mover_name();
}

void BootCampMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	BootCampMover::provide_xml_schema( xsd );
}


/// @brief getter and setter function to access the private data
void 
BootCampMover::set_scorefunction(core::scoring::ScoreFunctionOP sfxn) { //add & to modify in place
	sfxn_ = sfxn;
};

void 
BootCampMover::set_iternum(core::Size iters) {
	num_iterations_ = iters;
};

core::scoring::ScoreFunctionOP 
BootCampMover::get_scorefunction() {
	return sfxn_;
};

core::Size 
BootCampMover::get_iternum() {
	return num_iterations_;
};


/// @brief This mover is unpublished.  It returns sen as its author.
void
BootCampMover::provide_citation_info(basic::citation_manager::CitationCollectionList & citations ) const {
	citations.add(
		utility::pointer::make_shared< basic::citation_manager::UnpublishedModuleInfo >(
		"BootCampMover", basic::citation_manager::CitedModuleType::Mover,
		"sen",
		"TODO: institution",
		"swei25@jhu.edu",
		"Wrote the BootCampMover."
		)
	);
}


////////////////////////////////////////////////////////////////////////////////
	/// private methods ///
	///////////////////////


std::ostream &
operator<<( std::ostream & os, BootCampMover const & mover )
{
	mover.show(os);
	return os;
}


} //bootcamp
} //protocols
