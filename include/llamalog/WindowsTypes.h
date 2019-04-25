/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file
#pragma once

#include <llamalog/LogLine.h>

#include <fmt/core.h>

#include <windows.h>

#include <string>

namespace llamalog {

/// @brief A struct for logging system error codes, e.g. `GetLastError()`, `HRESULT`, etc.
struct ErrorCode {
	constexpr ErrorCode(const DWORD code) noexcept
		: code(code) {
		// empty
	}
	constexpr ErrorCode(const HRESULT hr) noexcept
		: code(hr) {
		// empty
	}
	const DWORD code;  ///< @brief The system error code.
};

/// @brief Get the result of `GetLastError()` as an `ErrorCode` suitable as a formatter argument.
/// @return A newly created `ErrorCode` structure.
[[nodiscard]] inline ErrorCode LastError() noexcept {
	return ErrorCode{GetLastError()};
}

#ifdef __clang_analyzer__
// make clang happy and define in namespace for ADL. MSVC can't find correct overload when the declaration is present.
LogLine& operator<<(LogLine& logLine, ErrorCode arg);
#endif

}  // namespace llamalog

/// @brief Log an `ErrorCode` structure.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, llamalog::ErrorCode arg);

/// @brief Log a `LARGE_INTEGER` structure as a 64 bit signed integer.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
inline llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const LARGE_INTEGER arg) {
	return logLine << arg.QuadPart;
}

/// @brief Log a `ULARGE_INTEGER` structure as a 64 bit unsigned integer.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
inline llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const ULARGE_INTEGER arg) {
	return logLine << arg.QuadPart;
}

/// @brief Log a `HINSTANCE` argument as a pointer value.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
inline llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const HINSTANCE arg) {  // NOLINT(misc-misplaced-const): We DO want const HINSTANCE.
	return logLine << static_cast<void*>(arg);
}

/// @brief Log a `POINT` structure as `(<x>, <y>)`.
/// @details Any formatting specifiers are applied to both values,
/// e.g. `{:03}` will output `POINT{10, 20}` as `(010, 020)`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT& arg);

/// @brief Log a `RECT` structure as `((<left>, <top>), (<right>, <bottom>))`.
/// @details Any formatting specifiers are applied to all values,
/// e.g. `{:02}` will output `RECT{1, 2, 3, 4}` as `((01, 02), (03, 04))`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT& arg);


//
// Specializations of fmt::formatter
//

/// @brief Specialization of `fmt::formatter` for a `llamalog::ErrorCode`.
/// @details Regular system error codes from e.g. `GetLastError()` are logged as decimal values. `HRESULT`s and other error codes are logged in hex.
template <>
struct fmt::formatter<llamalog::ErrorCode> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

	/// @brief Format the `llamalog::ErrorCode`.
	/// @param arg A `llamalog::ErrorCode`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::ErrorCode& arg, fmt::format_context& ctx);  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.

private:
	std::string m_format;  ///< The format pattern for the numerical error code.
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
	std::string m_format;  ///< The format pattern for all four values.
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
	std::string m_format;  ///< The format pattern for all four values.
};
