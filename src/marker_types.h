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
/// @brief Types for formatting which are shared by several translation units but which are not required to be publicly visible.
#pragma once

#include <cstddef>

namespace llamalog::marker {

/// @brief Marker type for a `nullptr` value for pointers.
/// @details Required as a separate type to ignore formatting placeholders. It is only used for (safe) type punning.
/// Therefore all constructors, destructors and assignment operators deleted.
struct NullValue final {
	// empty
};

/// @brief Helper class to pass a character string stored inline in the buffer through escaping.
/// @details The struct is only used for (safe) type punning to guide the output to the correct formatter.
/// Therefore all constructors, destructors and assignment operators deleted.
struct InlineChar final {
	// empty
};

/// @brief Helper class to pass a wide character string stored inline in the buffer to the formatter.
/// @details The struct is only used for (safe) type punning to guide the output to the correct formatter.
/// Therefore all constructors, destructors and assignment operators deleted.
struct InlineWideChar final {
	// empty
};

/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct TriviallyCopyable final {
	// empty
};

/// @brief Marker type for type-based lookup.
/// @details All constructors, destructors and assignment operators are intentionally deleted.
struct NonTriviallyCopyable final {
	// empty
};

}  // namespace llamalog::marker
