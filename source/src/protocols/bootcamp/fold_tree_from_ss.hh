// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file 	protocols/bootcamp/fold_tree_from_ss.hh
/// @brief 	get fold tree from secondary structrue
/// @author	Sen Wei (swei25@jhu.com)

#ifndef INCLUDED_protocols_bootcamp_fold_tree_from_ss_hh
#define INCLUDED_protocols_bootcamp_fold_tree_from_ss_hh

#include <iostream>
#include <utility>
#include <core/kinematics/FoldTree.hh>
#include <core/pose/Pose.hh>

namespace protocols {
namespace bootcamp {
utility::vector1< std::pair< core::Size, core::Size > >
identify_secondary_structure_spans( std::string const & ss_string );

//!!! when ss with only one residue is not taken into consideration
core::kinematics::FoldTree fold_tree_from_dssp_string(std::string & str);

core::kinematics::FoldTree fold_tree_from_ss(core::pose::Pose & mypose);

} // bootcamp
} // protocols

#endif // INCLUDED_protocols_bootcamp_fold_tree_from_ss_hh
