/*
Copyright 2019 Michael Beckh

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
/// @brief Contains formatters for types from the Windows API which may be used from own code but which are not required in the public namespace for logging.

#pragma once

#include <fmt/core.h>

#include <windows.h>

#include <string>

namespace llamalog {

struct error_code;  // NOLINT(readability-identifier-naming): Interface follows std::error_code.

}  // namespace llamalog

//
// Specializations of fmt::formatter
//

/// @brief Specialization of `fmt::formatter` for a `llamalog::error_code`.
/// @details Regular system error codes from e.g. `GetLastError()` are logged as decimal values. `HRESULT`s and other error codes are logged in hex.
template <>
struct fmt::formatter<llamalog::error_code> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	/// @brief Format the `llamalog::error_code`.
	/// @param arg A `llamalog::error_code`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::error_code& arg, fmt::format_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

private:
	std::string m_format;  ///< @brief The format pattern for the numerical error code.
};


/// @brief Specialization of `fmt::formatter` for a `POINT`.
template <>
struct fmt::formatter<POINT> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	/// @brief Format the `POINT`.
	/// @param arg A `POINT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const POINT& arg, fmt::format_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

private:
	std::string m_format;  ///< @brief The format pattern for both values.
};


/// @brief Specialization of `fmt::formatter` for a `RECT`.
template <>
struct fmt::formatter<RECT> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	/// @brief Format the `RECT`.
	/// @param arg A `RECT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const RECT& arg, fmt::format_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

private:
	std::string m_format;  ///< @brief The format pattern for all four values.
};
