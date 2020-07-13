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
/// @brief Include with formatters for output modifiers which must be publicly visible (at least for custom types).
#pragma once

#include <fmt/core.h>

#include <cstddef>
#include <string>

namespace llamalog::internal {

template <typename T>
struct EscapedArgument;

template <typename T>
struct PointerArgument;

/// @brief A common base class for formatters providing shared functionality.
/// @details The base class moves code out of the templated struct.
struct __declspec(novtable) ModifierBaseFormatter {
protected:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @param stopAtQuestionMark Stop parsing at a `?` character (required for custom formatting of null values).
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator ParseFormatString(fmt::format_parse_context& ctx, bool stopAtQuestionMark);

	[[nodiscard]] const std::string& GetFormat() const noexcept {
		return m_format;
	}

private:
	/// @brief The format pattern for the actual value.
	/// @details To allow referencing other arguments, all numeric ids are shifted up by 1.
	std::string m_format;
};

struct __declspec(novtable) PointerBaseFormatter : public ModifierBaseFormatter {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

protected:
	fmt::format_context::iterator Format(fmt::format_context::format_arg&& arg, fmt::format_context& ctx) const;
};

struct __declspec(novtable) EscapeBaseFormatter : public ModifierBaseFormatter {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

protected:
	fmt::format_context::iterator Format(fmt::format_context::format_arg&& arg, fmt::format_context& ctx) const;
};

}  // namespace llamalog::internal

//
// Specializations of fmt::formatter
//

/// @brief Specialization of `fmt::formatter` for a pointer value.
/// @tparam T The type of the actual argument.
template <typename T>
struct fmt::formatter<llamalog::internal::PointerArgument<T>> : public llamalog::internal::PointerBaseFormatter {
	/// @brief Format the pointer value.
	/// @param arg A pointer value.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::internal::PointerArgument<T>& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
		const T* const obj = reinterpret_cast<const T*>(&arg);
		return Format(fmt::detail::make_arg<fmt::format_context>(*obj), ctx);
	}
};

/// @brief Specialization of `fmt::formatter` for escaped values.
/// @tparam T The type of the actual argument.
template <typename T>
struct fmt::formatter<llamalog::internal::EscapedArgument<T>> : public llamalog::internal::EscapeBaseFormatter {
	/// @brief Format the value and escape its output.
	/// @param arg A value for formatting.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::internal::EscapedArgument<T>& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
		const T* const obj = reinterpret_cast<const T*>(&arg);
		return Format(fmt::detail::make_arg<fmt::format_context>(*obj), ctx);
	}
};
