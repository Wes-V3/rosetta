// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

#include<iostream>

#include<devel/init.hh> 
#include<utility/vector1.hh>
#include<basic/options/option.hh>
#include<basic/options/keys/in.OptionKeys.gen.hh>

#include<core/pose/Pose.fwd.hh>
#include<utility/pointer/owning_ptr.hh>
#include<core/pose/Pose.hh>
#include<core/import_pose/import_pose.hh> //pose_from_file

#include<core/scoring/ScoreFunctionFactory.hh> //get_score_funtion
#include<core/scoring/ScoreFunction.fwd.hh>
#include<core/scoring/ScoreFunction.hh>

#include<numeric/random/random.hh>

#include<protocols/moves/MonteCarlo.hh>
#include<protocols/moves/MonteCarlo.fwd.hh>

#include<protocols/moves/PyMOLMover.hh>

#include<core/pack/task/PackerTask.fwd.hh>
#include<core/pack/task/PackerTask.hh>
#include<core/pack/task/TaskFactory.hh>
#include<core/pack/pack_rotamers.hh>

#include<core/kinematics/MoveMap.hh>
#include<core/optimization/MinimizerOptions.hh>
#include<core/optimization/AtomTreeMinimizer.hh>

// Self defined FoldTreeFromSS
#include<protocols/bootcamp/fold_tree_from_ss.hh>

// correctly_add_cutpoint_variants() 
#include<core/pose/variant_util.hh>

int main( int argc, char ** argv ) {
	devel::init( argc, argv );
	utility::vector1< std::string > filenames = basic::options::option[ basic::options::OptionKeys::in::file::s ].value();
	if ( filenames.size() > 0 ) {
		std::cout << "You entered: " << filenames[1] << " as the PDB file to be read" << std::endl;
	} else {
		std::cout << "You didn't provide a PDB file with the -in::file::s option" << std::endl;
		return 1;
	}
	
	core::pose::PoseOP mypose = core::import_pose::pose_from_file( filenames[1] );
	core::pose::correctly_add_cutpoint_variants(*mypose); // add cutpoint variants
	
	core::scoring::ScoreFunctionOP sfxn = core::scoring::get_score_function();
	
	// Enable a new score term linear_chainbreak and set the weight to be 1	
	sfxn->set_method_weights("linear_chainbreak", 1);

	core::Real score = sfxn->score( *mypose );
	std::cout << "The score is: " << score << std::endl;

	core::Real temperature = 1;
	protocols::moves::MonteCarloOP mc ( new protocols::moves::MonteCarlo( *mypose, *sfxn, temperature ) );
	
	protocols::moves::PyMOLObserverOP the_observer = protocols::moves::AddPyMOLObserver( *mypose, true, 0 );

	core::Size N = mypose->size();

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
	mypose->fold_tree(protocols::bootcamp::fold_tree_from_ss(*mypose));

	for (int i = 0; i < 100; i++) {
		// Perturbation
		core::Real uniform_random_number = numeric::random::uniform();
		core::Size randres = static_cast< core::Size > (uniform_random_number * N + 1);
		core::Real pert1 = numeric::random::gaussian();
		core::Real pert2 = numeric::random::gaussian();
		core::Real orig_phi = mypose->phi( randres );
		core::Real orig_psi = mypose->psi( randres );
		mypose->set_phi( randres, orig_phi + pert1 );
		mypose->set_psi( randres, orig_psi + pert2 );

		// Packing and minimization
		core::pack::task::PackerTaskOP repack_task = core::pack::task::TaskFactory::create_packer_task( *mypose );
		repack_task->restrict_to_repacking();
		core::pack::pack_rotamers( *mypose, *sfxn, repack_task );
		
		// Invoke minimization
		copy_pose = *mypose; // To avoid sending pose to PyMol over and over again which cause long runtime
		atm.run( copy_pose, mm, *sfxn, min_opts );
		*mypose = copy_pose;

		if (mc->boltzmann( *mypose )) {
			acceptance_count++;
		};
		total_score = total_score + mc->last_accepted_score();

		the_observer->pymol().apply( *mypose );
	}
	
	std::cout << "Acceptance rate: " << acceptance_count/100 << " | Average score: " << total_score/100 << std::endl;

	std::cout << "After MonteCarlo, the score is: " << sfxn->score( *mypose ) << std::endl;
	// std::cout << N << " | " << randres << " | " << pert1 << " | " << pert2 << " | " << orig_phi << " | " << orig_psi << " | " << mypose->phi( randres ) << " | " << mypose->psi( randres ) << std::endl;

	
}
// randomseed