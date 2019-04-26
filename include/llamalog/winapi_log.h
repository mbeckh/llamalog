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

/// @file Enables logging of some types from the Windows API.

#pragma once

#include <llamalog/LogLine.h>

#include <windows.h>

namespace llamalog {

/// @brief A struct for logging Windows system error codes, e.g. `GetLastError()`, `HRESULT`, etc.
struct ErrorCode {
	constexpr ErrorCode(const int code) noexcept
		: code(code) {
		// change variable type if this assertion fails
		static_assert(sizeof(int) <= sizeof(DWORD), "data truncation in ErrorCode");
		// empty
	}
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
