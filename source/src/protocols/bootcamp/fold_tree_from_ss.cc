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

#include<iostream>
#include<utility>
#include<core/kinematics/FoldTree.hh>
#include<core/pose/Pose.hh>
#include<core/scoring/dssp/Dssp.hh>
#include<protocols/bootcamp/fold_tree_from_ss.hh>

namespace protocols {
namespace bootcamp  {
utility::vector1< std::pair< core::Size, core::Size > >
identify_secondary_structure_spans( std::string const & ss_string )
{
utility::vector1< std::pair< core::Size, core::Size > > ss_boundaries;
core::Size strand_start = -1;
for ( core::Size ii = 0; ii < ss_string.size(); ++ii ) {
    if ( ss_string[ ii ] == 'E' || ss_string[ ii ] == 'H'  ) {
    if ( int( strand_start ) == -1 ) {
        strand_start = ii;
    } else if ( ss_string[ii] != ss_string[strand_start] ) {
        ss_boundaries.push_back( std::make_pair( strand_start+1, ii ) );
        strand_start = ii;
    }
    } else {
    if ( int( strand_start ) != -1 ) {
        ss_boundaries.push_back( std::make_pair( strand_start+1, ii ) );
        strand_start = -1;
    }
    }
}
if ( int( strand_start ) != -1 ) {
    // last residue was part of a ss-eleemnt                                                                                                                                
    ss_boundaries.push_back( std::make_pair( strand_start+1, ss_string.size() ));
}
for ( core::Size ii = 1; ii <= ss_boundaries.size(); ++ii ) {
    std::cout << "SS Element " << ii << " from residue "
    << ss_boundaries[ ii ].first << " to "
    << ss_boundaries[ ii ].second << std::endl;
}
return ss_boundaries;
}

//!!! when ss with only one residue is not taken into consideration
core::kinematics::FoldTree fold_tree_from_dssp_string(std::string & str) {
    core::kinematics::FoldTree ft;
    
    utility::vector1< std::pair< core::Size, core::Size > > ss_boundaries = identify_secondary_structure_spans(str);
    core::Size jump_index = 1;

    core::Size first_mid = ( ss_boundaries[ 1 ].first + ss_boundaries[ 1 ].second ) / 2;
    ft.add_edge( first_mid, 1, core::kinematics::Edge::PEPTIDE );
    ft.add_edge( first_mid, ss_boundaries[ 1 ].second, core::kinematics::Edge::PEPTIDE );
    
    for ( core::Size ii = 2; ii < ss_boundaries.size(); ++ii ) {
        if (ss_boundaries[ ii-1 ].second+1 != ss_boundaries[ ii ].first ) {
            core::Size loop_mid = (ss_boundaries[ ii-1 ].second + ss_boundaries[ ii ].first) / 2;
            ft.add_edge( first_mid, loop_mid, jump_index++ );
            ft.add_edge( loop_mid, ss_boundaries[ ii-1 ].second+1, core::kinematics::Edge::PEPTIDE );
            ft.add_edge( loop_mid, ss_boundaries[ ii ].first-1, core::kinematics::Edge::PEPTIDE );
        }
        core::Size ss_mid = (ss_boundaries[ ii ].first + ss_boundaries[ ii ].second) / 2;
        ft.add_edge( first_mid, ss_mid, jump_index++ );
        ft.add_edge( ss_mid, ss_boundaries[ ii ].first, core::kinematics::Edge::PEPTIDE );
        ft.add_edge( ss_mid, ss_boundaries[ ii ].second, core::kinematics::Edge::PEPTIDE );
    }
    core::Size last_loop_mid = (ss_boundaries[ ss_boundaries.size()-1 ].second + ss_boundaries[ ss_boundaries.size() ].first) / 2;
    ft.add_edge( first_mid, last_loop_mid, jump_index++ );
    ft.add_edge( last_loop_mid, ss_boundaries[ ss_boundaries.size()-1 ].second+1, core::kinematics::Edge::PEPTIDE );
    ft.add_edge( last_loop_mid, ss_boundaries[ ss_boundaries.size() ].first-1, core::kinematics::Edge::PEPTIDE );
    core::Size last_mid = ( ss_boundaries[ ss_boundaries.size() ].first + ss_boundaries[ ss_boundaries.size() ].second ) / 2;
    ft.add_edge( first_mid, last_mid, jump_index);
    ft.add_edge( last_mid, ss_boundaries[ ss_boundaries.size() ].first, core::kinematics::Edge::PEPTIDE );
    ft.add_edge( last_mid, str.size(), core::kinematics::Edge::PEPTIDE );

    return ft;
}

core::kinematics::FoldTree fold_tree_from_ss(core::pose::Pose & mypose) {
    // pose to dssp
    core::scoring::dssp::Dssp mydssp(mypose);
    std::string dssp_string = mydssp.get_dssp_secstruct();
    core::kinematics::FoldTree ft = fold_tree_from_dssp_string(dssp_string);
    return ft;
}
}
}