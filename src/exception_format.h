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

#pragma once


#include <fmt/format.h>

#include <string>

namespace llamalog::exception {

struct StackBasedException;
struct StackBasedSystemError;
struct HeapBasedException;
struct HeapBasedSystemError;
struct PlainException;
struct PlainSystemError;

/// @brief Base class for a `fmt::formatter` to keep type-independent code out of the template.
struct ExceptionBaseFormatter {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	[[nodiscard]] const std::string& GetFormat() const noexcept {
		return m_format;
	}

private:
	std::string m_format;  ///< The original format pattern with the argument number (if any) removed.
};

/// @brief Base class for a `fmt::formatter` to print exception arguments.
/// @tparam T The type of the exception argument.
template <typename T>
struct ExceptionFormatter : public ExceptionBaseFormatter {
	/// @brief Format the log information stored inline in the buffer.
	/// @param arg A structure providing the data.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const T& arg, fmt::format_context& ctx) const;  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
};

// Explicit instantiation declarations to keep compile times down
extern template struct ExceptionFormatter<StackBasedException>;
extern template struct ExceptionFormatter<StackBasedSystemError>;
extern template struct ExceptionFormatter<HeapBasedException>;
extern template struct ExceptionFormatter<HeapBasedSystemError>;
extern template struct ExceptionFormatter<PlainException>;
extern template struct ExceptionFormatter<PlainSystemError>;

}  // namespace llamalog::exception

//
// Specializations of fmt::formatter
//

/// @brief Specialization of a `fmt::formatter` to print `StackBasedException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::StackBasedException> : public llamalog::exception::ExceptionFormatter<llamalog::exception::StackBasedException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `StackBasedSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::StackBasedSystemError> : public llamalog::exception::ExceptionFormatter<llamalog::exception::StackBasedSystemError> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `HeapBasedException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::HeapBasedException> : public llamalog::exception::ExceptionFormatter<llamalog::exception::HeapBasedException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `HeapBasedSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::HeapBasedSystemError> : public llamalog::exception::ExceptionFormatter<llamalog::exception::HeapBasedSystemError> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `PlainException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::PlainException> : public llamalog::exception::ExceptionFormatter<llamalog::exception::PlainException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `PlainSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::exception::PlainSystemError> : public llamalog::exception::ExceptionFormatter<llamalog::exception::PlainSystemError> {
	// empty
};
