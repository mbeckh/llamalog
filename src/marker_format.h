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
/// @brief Formatters for marker types which provide special output handling.
#pragma once

#include <fmt/core.h>

#include <string>

namespace llamalog::marker {

struct NullValue;
struct InlineChar;
struct InlineWideChar;

}  // namespace llamalog::marker


namespace llamalog::internal {

/// @brief A common base class for sharing the `parse` method.
struct __declspec(novtable) InlineCharBaseFormatter {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

protected:
	[[nodiscard]] const std::string& GetFormat() const noexcept {
		return m_format;
	}

private:
	std::string m_format;  ///< @brief The original format pattern with the argument number (if any) removed.
};

}  // namespace llamalog::internal

//
// Specializations of fmt::formatter
//

/// @brief Specialization of `fmt::formatter` for a `null` pointer value.
template <>
struct fmt::formatter<llamalog::marker::NullValue> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	/// @brief Format the `null` value.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::marker::NullValue& /* arg */, fmt::format_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

private:
	std::string m_value;  ///< @brief The format pattern for the null value
};

template <>
struct fmt::formatter<llamalog::marker::InlineChar> : public llamalog::internal::InlineCharBaseFormatter {
	/// @brief Format the wide character string stored inline in the buffer.
	/// @param arg A structure providing the address of the wide character string.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::marker::InlineChar& arg, fmt::format_context& ctx) const;  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
};


/// @brief Specialization of a `fmt::formatter` to perform automatic conversion of wide characters to UTF-8.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::marker::InlineWideChar> : public llamalog::internal::InlineCharBaseFormatter {
	/// @brief Format the wide character string stored inline in the buffer.
	/// @param arg A structure providing the address of the wide character string.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::marker::InlineWideChar& arg, fmt::format_context& ctx) const;  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
};
