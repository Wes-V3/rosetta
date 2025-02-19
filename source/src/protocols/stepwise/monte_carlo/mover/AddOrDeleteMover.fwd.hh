// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/stepwise/AddOrDeleteMover.fwd.hh
/// @brief  Various suite/nucleoside moves.
/// @author Rhiju Das

#ifndef INCLUDED_protocols_stepwise_monte_carlo_AddOrDeleteMover_fwd_hh
#define INCLUDED_protocols_stepwise_monte_carlo_AddOrDeleteMover_fwd_hh


#include <utility/pointer/owning_ptr.hh>


// C++

namespace protocols {
namespace stepwise {
namespace monte_carlo {
namespace mover {

class AddOrDeleteMover;
typedef utility::pointer::shared_ptr< AddOrDeleteMover > AddOrDeleteMoverOP;

} //mover
} //monte_carlo
} //stepwise
} //protocols

#endif
