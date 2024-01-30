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
	
	core::scoring::ScoreFunctionOP sfxn = core::scoring::get_score_function();
	core::Real score = sfxn->score( *mypose );
	std::cout << "The score is: " << score << std::endl;

	core::Real temperature = 1;
	protocols::moves::MonteCarloOP mc ( new protocols::moves::MonteCarlo( *mypose, *sfxn, temperature ) );
	
	protocols::moves::PyMOLObserverOP the_observer = protocols::moves::AddPyMOLObserver( *mypose, true, 0 );

	core::Size N = mypose->size();
	for (int i = 0; i < 10; i++) {
		core::Real uniform_random_number = numeric::random::uniform();
		core::Size randres = static_cast< core::Size > (uniform_random_number * N + 1);
		core::Real pert1 = numeric::random::gaussian();
		core::Real pert2 = numeric::random::gaussian();
		core::Real orig_phi = mypose->phi( randres );
		core::Real orig_psi = mypose->psi( randres );
		mypose->set_phi( randres, orig_phi + pert1 );
		mypose->set_psi( randres, orig_psi + pert2 );
		mc->boltzmann( *mypose );
		the_observer->pymol().apply( *mypose );
	}
	
	std::cout << "After MonteCarlo, the score is: " << sfxn->score( *mypose ) << std::endl;
	// std::cout << N << " | " << randres << " | " << pert1 << " | " << pert2 << " | " << orig_phi << " | " << orig_psi << " | " << mypose->phi( randres ) << " | " << mypose->psi( randres ) << std::endl;


}
// randomseed