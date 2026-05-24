#pragma once

// Master include — pulls in every sub-module.
// Existing code that includes <templatedefines.h> will continue to compile.
//
// New code should include only the headers it actually needs.

#include "concepts_std.h"
#include "concepts_qt.h"
#include "function_traits.h"
#include "function_helper.h"
#include "constexpr_helper.h"
#include "types_helper.h"
#include "conversion_types.h"
#include "qt_support_helper.h"
