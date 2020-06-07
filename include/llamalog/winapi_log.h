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
struct error_code {  // NOLINT(readability-identifier-naming): Looks better when named like std::error_code.
	constexpr explicit error_code(const int code) noexcept
		: code(code) {
		// change variable type if this assertion fails
		static_assert(sizeof(int) <= sizeof(DWORD), "data truncation in error_code");
		// empty
	}
	constexpr explicit error_code(const DWORD code) noexcept
		: code(code) {
		// empty
	}
	constexpr explicit error_code(const HRESULT hr) noexcept
		: code(hr) {
		// empty
	}

	// NOLINTNEXTLINE(readability-identifier-naming, misc-non-private-member-variables-in-classes): clang-tidy can't properly decide between public member and constant member.
	const DWORD code;  ///< @brief The system error code.
};

/// @brief Get the result of `GetLastError()` as an `error_code` suitable as a formatter argument.
/// @return A newly created `error_code` structure.
[[nodiscard]] inline error_code LastError() noexcept {
	return error_code{GetLastError()};
}

#ifdef __clang_analyzer__
// make clang happy and define in namespace for ADL. MSVC can't find correct overload when the declaration is present.
LogLine& operator<<(LogLine& logLine, error_code arg);
#endif

}  // namespace llamalog

/// @brief Log an `error_code` structure.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, llamalog::error_code arg);

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

/// @brief Log a `POINT` structure as `(<x>, <y>)`.
/// @details Any formatting specifiers are applied to both values,
/// e.g. `{:03}` will output `POINT{10, 20}` as `(010, 020)`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT* arg);

/// @brief Log a `RECT` structure as `((<left>, <top>), (<right>, <bottom>))`.
/// @details Any formatting specifiers are applied to all values,
/// e.g. `{:02}` will output `RECT{1, 2, 3, 4}` as `((01, 02), (03, 04))`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT& arg);

/// @brief Log a `RECT` structure as `((<left>, <top>), (<right>, <bottom>))`.
/// @details Any formatting specifiers are applied to all values,
/// e.g. `{:02}` will output `RECT{1, 2, 3, 4}` as `((01, 02), (03, 04))`.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The value.
/// @return @p logLine to allow method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT* arg);


//
// Macros
//

#if !(defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE))
#define LLAMALOG_LEVEL_DEBUG
#endif

/// @brief Log a message at `#llamalog::Priority` @p priority_ for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG_HRESULT(priority_, result_, message_, ...)                                                                    \
	[&](const HRESULT result, const char* const function) -> HRESULT {                                                             \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                                                             \
		llamalog::Log(llamalog::Priority::kTrace, file_, __LINE__, function, message_, llamalog::error_code{result}, __VA_ARGS__); \
		return result;                                                                                                             \
	}(result_, __func__)

/// @brief Log a message at `#llamalog::Priority` @p priority_ for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG_HRESULT_NOEXCEPT(priority_, result_, message_, ...)                                                                   \
	[&](const HRESULT result, const char* const function) noexcept -> HRESULT {                                                            \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                                                                     \
		llamalog::LogNoExcept(llamalog::Priority::kTrace, file_, __LINE__, function, message_, llamalog::error_code{result}, __VA_ARGS__); \
		return result;                                                                                                                     \
	}(result_, __func__)


//
// LOG_..._HRESULT

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT(llamalog::Priority::kTrace, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG_HRESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kDebug, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO_HRESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kInfo, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN_HRESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kWarn, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT(llamalog::Priority::kError, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal` for function return values of type `HRESULT`.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT(llamalog::Priority::kFatal, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif


//
// SLOG_..._HRESULT

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kTrace, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kDebug, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kInfo, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_WARN_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kWarn, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_WARN_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kError, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal` for function return values of type `HRESULT` without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL_HRESULT(result_, message_, ...) LLAMALOG_LOG_HRESULT_NOEXCEPT(llamalog::Priority::kFatal, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL_HRESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif
