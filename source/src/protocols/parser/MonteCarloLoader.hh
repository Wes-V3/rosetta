// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/parser/MonteCarloLoader.hh
/// @brief  Declartion of the XML parser's MonteCarloLoader class for adding named ScoreFunctions to the basic::datacache::DataMap
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

#ifndef INCLUDED_protocols_parser_MonteCarloLoader_hh
#define INCLUDED_protocols_parser_MonteCarloLoader_hh

// Package Headers
#include <protocols/parser/DataLoader.hh>

// Project Headers

// Utility Headers
#include <utility/tag/Tag.fwd.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

#include <string>


namespace protocols {
namespace parser {

/// @brief The MonteCarloLoader will create named MonteCarlo objects and load them into the basic::datacache::DataMap
class MonteCarloLoader : public DataLoader
{
public:
	MonteCarloLoader();
	~MonteCarloLoader() override;

	/// @brief The MonteCarloLoader will create named MonteCarlo objects and load them into the basic::datacache::DataMap
	void load_data(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data
	) const override;

	static std::string loader_name();
	static std::string monte_carlo_loader_ct_namer( std::string const & element_name );
	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

};

} //namespace parser
} //namespace protocols

#endif
