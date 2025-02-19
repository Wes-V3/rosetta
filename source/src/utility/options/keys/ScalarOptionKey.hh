// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   utility/options/keys/ScalarOptionKey.hh
/// @brief  Abstract automatic hidden index key for scalar-valued options
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)


#ifndef INCLUDED_utility_options_keys_ScalarOptionKey_hh
#define INCLUDED_utility_options_keys_ScalarOptionKey_hh


// Unit headers
#include <utility/options/keys/ScalarOptionKey.fwd.hh>

// Package headers
#include <utility/options/keys/OptionKey.hh>


#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/access.fwd.hpp>
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace utility {
namespace options {


/// @brief Abstract automatic hidden index key for scalar-valued options
class ScalarOptionKey :
	public OptionKey
{


private: // Types


	typedef  OptionKey  Super;


protected: // Creation


	/// @brief Default constructor
	inline
	ScalarOptionKey()
	{}


	/// @brief Copy + identifier constructor
	inline
	ScalarOptionKey(
		ScalarOptionKey const & key,
		std::string const & id_a,
		std::string const & identifier_a = std::string(),
		std::string const & code_a = std::string()
	) :
		Super( key, id_a, identifier_a, code_a )
	{}


	/// @brief Key constructor
	inline
	explicit
	ScalarOptionKey( Key const & key ) :
		Super( key )
	{}


	/// @brief Key + identifier constructor
	inline
	ScalarOptionKey(
		Key const & key,
		std::string const & id_a,
		std::string const & identifier_a = std::string(),
		std::string const & code_a = std::string()
	) :
		Super( key, id_a, identifier_a, code_a )
	{}


	/// @brief Identifier constructor
	inline
	explicit
	ScalarOptionKey(
		std::string const & id_a,
		std::string const & identifier_a = std::string(),
		std::string const & code_a = std::string()
	) :
		Super( id_a, identifier_a, code_a )
	{}


public: // Creation


	/// @brief Clone this
	ScalarOptionKey *
	clone() const override = 0;


	/// @brief Destructor
	inline
	~ScalarOptionKey() override {}


public: // Assignment


	/// @brief Key assignment
	inline
	ScalarOptionKey &
	operator =( Key const & key )
	{
		assign_Key( key );
		return *this;
	}


public: // Properties


	/// @brief Scalar option key?
	bool
	scalar() const override
	{
		return true;
	}


	/// @brief Vector option key?
	bool
	vector() const override
	{
		return false;
	}


#ifdef    SERIALIZATION
	friend class cereal::access;
public:
	template< class Archive > void save( Archive & arc ) const;
	template< class Archive > void load( Archive & arc );
#endif // SERIALIZATION

}; // ScalarOptionKey


} // namespace options
} // namespace utility


#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( utility_options_keys_ScalarOptionKey )
#endif // SERIALIZATION


#endif // INCLUDED_utility_options_keys_ScalarOptionKey_HH
