#pragma once

// Master include — pulls in every sub-module of the toolkit.
//
// New code should include only the specific headers it actually needs;
// this umbrella is provided for convenience and for projects migrating
// from a single-header layout.

#include "concepts_std.h"
#include "concepts_qt.h"
#include "function_traits.h"
#include "function_helper.h"
#include "constexpr_helper.h"
#include "types_helper.h"
#include "conversion_types.h"
#include "qt_support_helper.h"
#include "enum_flag.h"
