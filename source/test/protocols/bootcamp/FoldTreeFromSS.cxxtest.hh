// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file  protocols/bootcamp/FoldTreeFromSS.cxxtest.hh
/// @brief  Tests for the FoldTree from secondary structure
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

// FoldTree
#include <utility>
#include <core/kinematics/FoldTree.hh>
#include <core/scoring/dssp/Dssp.hh>

// Edge
#include <core/kinematics/Edge.hh>

// fold_tree_from_ss
#include <protocols/bootcamp/fold_tree_from_ss.hh> 

static basic::Tracer TR("FoldTreeFromSS");

class FoldTreeFromSS : public CxxTest::TestSuite {
	//Define Variables

private:
	std::string SS1 = "   EEEEE   HHHHHHHH  EEEEE   IGNOR EEEEEE   HHHHHHHHHHH  EEEEE  HHHH   ";
	std::string SS2 = "HHHHHHH   HHHHHHHHHHHH      HHHHHHHHHHHHEEEEEEEEEEHHHHHHH EEEEHHH ";
	std::string SS3 = "EEEEEEEEE EEEEEEEE EEEEEEEEE H EEEEE H H H EEEEEEEE";
	utility::vector1< std::pair< core::Size, core::Size > > SS1_span = { {4, 8}, {12, 19}, {22, 26}, {36, 41}, {45, 55}, {58, 62}, {65, 68} };
	utility::vector1< std::pair< core::Size, core::Size > > SS2_span = { {1, 7}, {11, 22}, {29, 40}, {41, 50}, {51, 57}, {59, 62}, {63, 65} };
	utility::vector1< std::pair< core::Size, core::Size > > SS3_span = { {1, 9}, {11, 18}, {20, 28}, {30, 30}, {32, 36}, {38, 38}, {40, 40}, {42, 42}, {44, 51} };

	std::string SS4 = "   EEEEEEE    EEEEEEE         EEEEEEEEE    EEEEEEEEEE   HHHHHH         EEEEEEEEE         EEEEE     ";

public:

	void setUp() {
		core_init();
		// core::kinematics::FoldTree SS4_ft;
		// SS4_ft.get_residue_edge(SS4); //!!! This is not right!
	}

	void tearDown() {

	}

	void test_SS_span() {
		TS_TRACE("Test SS span");
		TS_ASSERT_EQUALS(protocols::bootcamp::identify_secondary_structure_spans( SS1 ), SS1_span);
		TS_ASSERT_EQUALS(protocols::bootcamp::identify_secondary_structure_spans( SS2 ), SS2_span);
		TS_ASSERT_EQUALS(protocols::bootcamp::identify_secondary_structure_spans( SS3 ), SS3_span);
	}

	// This Function is incomplete, it cannot test automatically.
	void test_fold_tree_from_ss() {
		TS_TRACE("Test fold tree from ss");
		// TS_ASSERT_EQUALS(fold_tree_from_ss(SS4), SS4_ft);
		core::kinematics::FoldTree ft = protocols::bootcamp::fold_tree_from_dssp_string(SS4);
		std::cout << ft << std::endl;
	}

};