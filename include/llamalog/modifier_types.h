/*
Copyright 2020 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file
/// @brief Include with output modifiers which must be publicly visible.
#pragma once

#include <cstddef>

namespace llamalog::internal {

/// @brief Marker type for a pointer value.

/// @details Required as a separate type to ignore formatting placeholders for `null` value.
/// The struct is only used for (safe) type punning to guide the output to the correct formatter.
/// Therefore all constructors, destructors and assignment operators deleted.
template <typename T>
struct PointerArgument final {
	// empty
};

/// @brief Helper class to format an argument with output escaping.
/// @details The struct is only used for (safe) type punning to guide the output to the correct formatter.
/// Therefore all constructors, destructors and assignment operators deleted.
/// @tparam T The type of the actual argument.
template <typename T>
struct EscapedArgument final {
	// empty
};

}  // namespace llamalog::internal
