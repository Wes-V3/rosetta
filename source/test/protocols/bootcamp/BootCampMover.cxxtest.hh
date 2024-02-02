// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file  protocols/bootcamp/BootCampMover.cxxtest.hh
/// @brief  test BootCampMover setter and getter for number of iterations and score function
/// @author sen (swei25@jhu.edu)


// Test headers
#include <test/UMoverTest.hh>
#include <test/UTracer.hh>
#include <cxxtest/TestSuite.h>
#include <test/util/pose_funcs.hh>
#include <test/core/init_util.hh>

// Project Headers


// Core Headers
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>

// Utility, etc Headers
#include <basic/Tracer.hh>

// BootCampMover
#include <protocols/bootcamp/BootCampMover.hh>
#include <protocols/bootcamp/BootCampMover.fwd.hh>
#include <protocols/bootcamp/BootCampMoverCreator.hh>

// ScoreFunction
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

// Tag
#include <utility/tag/Tag.hh>
#include <utility/tag/Tag.fwd.hh>

// DataMap
#include <basic/datacache/DataMap.hh>

static basic::Tracer TR("BootCampMover");


class BootCampMover : public CxxTest::TestSuite {
	//Define Variables
private:
	protocols::bootcamp::BootCampMoverOP bootcamp_mover;
	core::Size iters;
	core::scoring::ScoreFunctionOP sfxn;
	utility::tag::TagCOP tag;
	basic::datacache::DataMap data;

public:

	void setUp() {
		core_init();
		bootcamp_mover = utility::pointer::make_shared< protocols::bootcamp::BootCampMover >() ;
		iters = 3;
	 	sfxn = core::scoring::get_score_function();
		bootcamp_mover->set_iternum(iters);
		bootcamp_mover->set_scorefunction(sfxn);
	}

	void tearDown() {

	}



	void test_set_iternums() {
		TS_TRACE("Test setting number of iterations");
		core::Size new_iters = 10;
		bootcamp_mover->set_iternum(new_iters);
		TS_ASSERT_EQUALS(bootcamp_mover->get_iternum(), new_iters)
	}

	void test_get_iternums() {
		TS_TRACE("Test getting number of iterations");
		TS_ASSERT_EQUALS(bootcamp_mover->get_iternum(), iters);
	}

	void test_set_sfxn() {
		TS_TRACE("Test setting score funtion");
		// Enable a new score term linear_chainbreak and set the weight to be 1	
		core::scoring::ScoreType new_linear_chainbreak = core::scoring::ScoreType::linear_chainbreak;
		utility::vector1<core::Real> weight = {1.0};
		sfxn->set_method_weights( new_linear_chainbreak, weight );
		bootcamp_mover->set_scorefunction(sfxn);
		TS_ASSERT(bootcamp_mover->get_scorefunction() == sfxn);
	}

	void test_get_sfxn() {
		TS_TRACE("Test getting score funtion");
		TS_ASSERT(bootcamp_mover->get_scorefunction() == sfxn);
	}

	void test_mover_factory() {
		protocols::bootcamp::BootCampMoverCreator bcmc;
		protocols::moves::MoverOP base_mover_op = bcmc.create_mover();
		protocols::bootcamp::BootCampMoverOP bcm_op = protocols::bootcamp::BootCampMoverOP( utility::pointer::dynamic_pointer_cast< protocols::bootcamp::BootCampMover > ( base_mover_op ) );
		TS_ASSERT( bcm_op != 0 );
	}

	void test_parse_my_tag() {
		TS_TRACE("Test parsing my tag");

		/// @brief Generate a tagptr from a string
		/// For parse_my_tag tests, only do the relevant tag, not the full <ROSETTASCRIPTS> ... </ROSETTASCRIPTS> wrapped tag.
		std::string xml_file = "<BootCampMover niterations=\"1025\"/>";
		core::Size niterations = 1025;

		std::stringstream instream( xml_file );
		utility::tag::TagCOP iter_tag( utility::tag::Tag::create( instream ) );
		
		bootcamp_mover->parse_my_tag(iter_tag, data);
		TS_ASSERT_EQUALS(bootcamp_mover->get_iternum(), niterations);
	}

	void test_parse_score_function() {
		std::string xml_file = "<BootCampMover scorefxn=\"testing123\"/>";

		std::stringstream instream( xml_file );
		utility::tag::TagCOP sfxn_tag( utility::tag::Tag::create( instream ) );

		core::scoring::ScoreFunctionOP sfxn2 = core::scoring::ScoreFunctionOP( new core::scoring::ScoreFunction );
		data.add( "scorefxns" , "testing123", sfxn2 );

		bootcamp_mover->parse_my_tag(sfxn_tag, data);

		TS_ASSERT( sfxn2 == bootcamp_mover->get_scorefunction() );
	}
};
