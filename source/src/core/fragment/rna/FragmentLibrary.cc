// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.


// Rosetta Headers
#include <core/fragment/rna/FragmentLibrary.hh>
#include <core/fragment/rna/TorsionSet.hh>

#include <core/fragment/rna/FullAtomRNA_Fragments.hh>
#include <core/chemical/rna/util.hh>
#include <core/types.hh>
#include <numeric/random/random.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/rna.OptionKeys.gen.hh>

#include <ObjexxFCL/StaticIndexRange.hh>

namespace core {
namespace fragment {
namespace rna {

using namespace core;
using ObjexxFCL::SRange;

FragmentLibrary::~FragmentLibrary() = default;

Real FragmentLibrary::get_fragment_torsion( Size const num_torsion,  Size const which_frag, Size const offset ){
	return align_torsions_[ which_frag - 1 ].torsions( num_torsion, offset) ;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
TorsionSet const &
FragmentLibrary::get_fragment_torsion_set( Size const which_frag ) const
{
	return align_torsions_[ which_frag - 1 ];
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void  FragmentLibrary::add_torsion( TorsionSet const & torsion_set ){
	align_torsions_.push_back( torsion_set );
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void  FragmentLibrary::add_torsion(
	FullAtomRNA_Fragments const & vall,
	Size const position,
	Size const size
) {
	// Always add unfuzzed fragment
	TorsionSet torsion_set( size, position );
	for ( Size offset = 0; offset < size; offset++ ) {
		for ( Size j = 1; j <= core::chemical::rna::NUM_RNA_TORSIONS; j++ ) {
			torsion_set.torsions( j, offset) = vall.torsions( j, position+offset);
		}
		torsion_set.torsion_source_name( offset ) = vall.name( position+offset );
		torsion_set.secstruct( offset ) = vall.secstruct( position+offset );

		//Defined non-ideal geometry of sugar ring -- to keep it closed.
		if ( vall.non_main_chain_sugar_coords_defined() ) {
			torsion_set.non_main_chain_sugar_coords_defined = true;
			torsion_set.non_main_chain_sugar_coords.dimension( SRange(0,size), 3, 3 );
			for ( Size j = 1; j <= 3; j++ ) {
				for ( Size k = 1; k <= 3; k++ ) {
					torsion_set.non_main_chain_sugar_coords( offset, j, k ) =
						vall.non_main_chain_sugar_coords( position+offset, j, k );
				}
			}
		} else {
			torsion_set.non_main_chain_sugar_coords_defined = false;
		}
	}

	align_torsions_.push_back( torsion_set );

	using namespace basic::options;
	if ( option[ OptionKeys::rna::denovo::fuzz_fragments ]() != 0 ) {
		// Arbitrarily... let's start with five.
		for ( Size ii = 1; ii <= 5; ++ii ) {
			TorsionSet torsion_set2( size, position );
			for ( Size offset = 0; offset < size; offset++ ) {
				// This might mess with sugar closure unless we are careful
				// about what torsions we fuzz... all but delta safe?

				for ( Size j = 1; j <= core::chemical::rna::NUM_RNA_TORSIONS; j++ ) {
					torsion_set2.torsions( j, offset ) = vall.torsions( j, position + offset );
					// AMW: to maintain overall 'shape' could consider shearing.
					if ( j != 4 ) { // DELTA -- change later to enum
						torsion_set2.torsions( j, offset ) += numeric::random::rg().gaussian() * option[ OptionKeys::rna::denovo::fuzz_fragments ]();
					}
				}
				torsion_set2.torsion_source_name( offset ) = vall.name( position+offset );
				torsion_set2.secstruct( offset ) = vall.secstruct( position+offset );

				//Defined non-ideal geometry of sugar ring -- to keep it closed.
				if ( vall.non_main_chain_sugar_coords_defined() ) {
					torsion_set2.non_main_chain_sugar_coords_defined = true;
					torsion_set2.non_main_chain_sugar_coords.dimension( SRange( 0, size ), 3, 3 );
					for ( Size j = 1; j <= 3; j++ ) {
						for ( Size k = 1; k <= 3; k++ ) {
							torsion_set2.non_main_chain_sugar_coords( offset, j, k ) =
								vall.non_main_chain_sugar_coords( position + offset, j, k );
						}
					}
				} else {
					torsion_set2.non_main_chain_sugar_coords_defined = false;
				}
			}

			align_torsions_.push_back( torsion_set2 );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
Size FragmentLibrary::get_align_depth() const {
	return align_torsions_.size();
}

} //denovo
} //rna
} //protocols
