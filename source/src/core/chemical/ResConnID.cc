// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @brief
/// @author Andrew Leaver-Fay


// Unit Headers
#include <core/chemical/ResConnID.hh>

#ifdef SERIALIZATION
#include <utility/serialization/serialization.hh>
#endif

namespace core {
namespace chemical {

ResConnID::ResConnID() : res_id_( 0 ), conn_id_( 0 ) {}

ResConnID::ResConnID( ResConnID const & ) = default;

ResConnID::ResConnID( Size resid, Size connid ) : res_id_( resid ), conn_id_( connid ) {}

ResConnID & ResConnID::operator = ( ResConnID const & ) = default;

bool operator < ( ResConnID const & lhs, ResConnID const & rhs )
{
	return ( lhs.res_id_ == rhs.res_id_ ? lhs.conn_id_ < rhs.conn_id_ : lhs.res_id_ < rhs.res_id_ );
}


bool operator == ( ResConnID const & lhs, ResConnID const & rhs )
{
	return ( lhs.res_id_ == rhs.res_id_ && lhs.conn_id_ == rhs.conn_id_ );
}

bool operator != ( ResConnID const & lhs, ResConnID const & rhs )
{
	return !( lhs == rhs );
}

#ifdef SERIALIZATION
template < class Archive >
void
ResConnID::save( Archive & arch ) const {
	arch( res_id_, conn_id_ );
}

template < class Archive >
void
ResConnID::load( Archive & arch ) {
	arch( res_id_, conn_id_ );
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::ResConnID );
#endif

}
}
