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

// JobDistributor
#include <protocols/jd2/JobDistributor.hh>

// BootCampMover
#include <protocols/bootcamp/BootCampMover.hh>
#include <protocols/bootcamp/BootCampMover.fwd.hh>

int main( int argc, char ** argv ) {

	devel::init(argc, argv);

	protocols::bootcamp::BootCampMoverOP bootcamp_mover( new protocols::bootcamp::BootCampMover() );

	protocols::jd2::JobDistributor::get_instance()->go(bootcamp_mover);

}
// randomseed