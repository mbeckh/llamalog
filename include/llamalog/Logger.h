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
/// @copyright Code marked with "from NanoLog" is based on NanoLog (https://github.com/Iyengar111/NanoLog, commit
/// 40a53c36e0336af45f7664abeb939f220f78273e), copyright 2016 Karthik Iyengar and distributed unter the MIT License.
#pragma once

/*

Distributed under the MIT License (MIT)

	Copyright (c) 2016 Karthik Iyengar

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <llamalog/LogLine.h>
// IWYU pragma: no_include "llamalog/winapi_log.h"

#include <sal.h>

#include <cstdint>
#include <memory>
#include <utility>

/// @brief The main namespace.
namespace llamalog {

//
// Logging
//

class LogWriter;

namespace internal {

/// @brief Initialize the logger.
/// @note `Start` MUST be called after `Initialize` before any logging takes place.
/// @copyright Derived from `Initialize` from NanoLog.
void Initialize();

/// @brief Actually start logging.
/// @note `Start` MUST be called after `Initialize` before any logging takes place.
void Start();

/// @brief Calculate the `Priority` for internal logging messages.
/// @remarks The function prevents endless loops by encoding an error counter in the priority.
/// @param priority The desired logging priority.
/// @return The `Priority` to use for an internal logging message.
[[nodiscard]] Priority GetInternalPriority(Priority priority) noexcept;

/// @brief Helper for `#LogNoExcept`.
/// @details Call a logging function and swallow all exceptions.
/// @remarks The original source location is retained for all errors that are logged while calling the logger.
/// @param file The file where the message is created.
/// @param line The line where the message is created.
/// @param function The function where the message is created.
void CallNoExcept(const char* __restrict const file, const std::uint32_t line, const char* __restrict const function, void (*const thunk)(_In_z_ const char* __restrict, std::uint32_t, _In_z_ const char* __restrict, _In_ void*), _In_ void* const log) noexcept;

/// @brief Log a message if logging fails.
/// @details The output is sent to `OutputDebugStringA`.
/// @param file The file where the message is created.
/// @param line The line where the message is created.
/// @param function The function where the message is created.
/// @param message The message to log.
void Panic(const char* file, std::uint32_t line, const char* function, const char* message) noexcept;

/// @brief A function to silence any warnings about unused arguments.
template <typename... T>
void Unused(T&&...) noexcept {
	// empty
}

}  // namespace internal


/// @brief Initialize the logger, add writers and start logging.
/// @note `Initialize` MUST be called before any logging takes place.
/// @tparam LogWriter MUST be of type `LogWriter`.
/// @param writers One or more `LogWriter` objects.
template <typename... LogWriter>
void Initialize(std::unique_ptr<LogWriter>&&... writers) {
	internal::Initialize();
	(..., AddWriter(std::move(writers)));
	internal::Start();
}

/// @brief Checks if the logger has been initialized.
/// @return `true` if the logger is available, `false` if not.
bool IsInitialized() noexcept;

/// @brief Add a log writer.
/// @param writer The `LogWriter` to add.
void AddWriter(std::unique_ptr<LogWriter>&& writer);

/// @brief Get the filename after the last slash or backslash character.
/// @param path A start of the path to shorten.
/// @param check The next character to check.
/// @return Filename and extension after the last path separator.
[[nodiscard]] constexpr __declspec(noalias) _Ret_z_ const char* GetFilename(_In_z_ const char* const path, _In_opt_z_ const char* const check = nullptr) noexcept {
	if (!check) {
		// first call
		return GetFilename(path, path);
	}
	if (*check) {
		if (*check == '/' || *check == '\\') {
			return GetFilename(check + 1, check + 1);
		}
		return GetFilename(path, check + 1);
	}
	return path;
}


/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void Log(LogLine& logLine);

/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void Log(LogLine&& logLine);

/// @brief Logs a new `LogLine`.
/// @tparam T The types of the message arguments.
/// @param priority The `#Priority`.
/// @param file The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param function The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p message.
template <typename... T>
void Log(const Priority priority, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	LogLine logLine(priority, file, line, function, message);
	Log((logLine << ... << std::forward<T>(args)));
}

/// @brief Logs a new `LogLine` without throwing an exception.
/// @tparam T The types of the message arguments.
/// @param priority The `#Priority`.
/// @param file The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param function The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p message.
/// @copyright The function uses a trick that allows calling a binding lamba using a function pointer. It is published
/// by Joaquín M López Muñoz at http://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html.
template <typename... T>
void LogNoExcept(const Priority priority, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) noexcept {
	auto log = [priority, message, &args...](_In_z_ const char* const file, const std::uint32_t line, _In_z_ const char* const function) {
		LogLine logLine(priority, file, line, function, message);
		Log((logLine << ... << std::forward<T>(args)));
	};
	auto thunk = [](_In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_ void* const p) {
		(*static_cast<decltype(log)*>(p))(file, line, function);
	};

	internal::CallNoExcept(file, line, function, thunk, &log);
}

/// @brief Logs a new `LogLine` for an internal message from the logger itself.
/// @details The function includes code to prevent endless loops caused by logging errors that happen while logging errors.
/// @tparam T The types of the message arguments.
/// @param priority The `#Priority`.
/// @param file The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param function The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p message.
template <typename... T>
void LogInternal(const Priority priority, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	const Priority internalPriority = internal::GetInternalPriority(priority);
	if ((static_cast<std::uint8_t>(internalPriority) & 3u) == 3u) {
		internal::Panic(file, line, function, "Error logging error");
		return;
	}
	LogLine logLine(internalPriority, file, line, function, message);
	Log((logLine << ... << std::forward<T>(args)));
}

/// @brief Waits until all currently available entries have been written.
/// @details This function might block for a long time and its main purpose is to flush the log for testing.
/// Use with care in your own code.
void Flush();

/// @brief End all logging. This MUST be the last function called.
void Shutdown() noexcept;

}  // namespace llamalog

//
// Macros
//

#if !(defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE))
#define LLAMALOG_LEVEL_DEBUG
#endif

/// @brief Emit a log line. @details Without the explicit variable `file_` the compiler does not reliably evaluate
/// `#llamalog::GetFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param message_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG(priority_, message_, ...)                                      \
	do {                                                                            \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);              \
		llamalog::Log(priority_, file_, __LINE__, __func__, message_, __VA_ARGS__); \
	} while (0)

/// @brief Emit a log line. @details Without the explicit variable `file_` the compiler does not reliably evaluate
/// `#llamalog::GetFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param message_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG_NOEXCEPT(priority_, message_, ...)                                     \
	do {                                                                                    \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                      \
		llamalog::LogNoExcept(priority_, file_, __LINE__, __func__, message_, __VA_ARGS__); \
	} while (0)

/// @brief Log a message at `#llamalog::Priority` @p priority_ for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG_RESULT(priority_, result_, message_, ...)                              \
	[&](decltype(result_) const result, const char* const function) -> decltype(result_) {  \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                      \
		llamalog::Log(priority_, file_, __LINE__, function, message_, result, __VA_ARGS__); \
		return result;                                                                      \
	}(result_, __func__)

/// @brief Log a message at `#llamalog::Priority` @p priority_ for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG_RESULT_NOEXCEPT(priority_, result_, message_, ...)                             \
	[&](decltype(result_) const result, const char* const function) noexcept->decltype(result_) {   \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                              \
		llamalog::LogNoExcept(priority_, file_, __LINE__, function, message_, result, __VA_ARGS__); \
		return result;                                                                              \
	}                                                                                               \
	(result_, __func__)

/// @brief Emit a log line for an internal message from the logger itself.
/// @details Without the explicit variable `file_` the compiler does not reliably evaluate
/// `#llamalog::GetFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param message_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_LOG(priority_, message_, ...)                                     \
	do {                                                                                    \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                      \
		llamalog::LogInternal(priority_, file_, __LINE__, __func__, message_, __VA_ARGS__); \
	} while (0)


//
// LOG_...

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE(message_, ...) LLAMALOG_LOG(llamalog::Priority::kTrace, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG(message_, ...) LLAMALOG_LOG(llamalog::Priority::kDebug, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO(message_, ...) LLAMALOG_LOG(llamalog::Priority::kInfo, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN(message_, ...) LLAMALOG_LOG(llamalog::Priority::kWarn, message_, __VA_ARGS__)
#else
#define LOG_WARN(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR(message_, ...) LLAMALOG_LOG(llamalog::Priority::kError, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL(message_, ...) LLAMALOG_LOG(llamalog::Priority::kFatal, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif


//
// SLOG_...

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kTrace, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kDebug, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kInfo, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_WARN(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kWarn, message_, __VA_ARGS__)
#else
#define SLOG_WARN(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kError, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal` without throwing an exception.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL(message_, ...) LLAMALOG_LOG_NOEXCEPT(llamalog::Priority::kFatal, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif


//
// LOG_..._RESULT

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kTrace, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kDebug, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kInfo, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kWarn, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kError, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT(llamalog::Priority::kFatal, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif


//
// SLOG_..._RESULT

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kTrace, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_TRACE_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kDebug, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_DEBUG_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kInfo, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_INFO_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_WARN_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kWarn, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_WARN_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kError, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_ERROR_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal` for function return values without throwing an exception.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#if defined(LLAMALOG_LEVEL_FATAL) || defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL_RESULT(result_, message_, ...) LLAMALOG_LOG_RESULT_NOEXCEPT(llamalog::Priority::kFatal, result_, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define SLOG_FATAL_RESULT(result_, message_, ...) (llamalog::internal::Unused(__VA_ARGS__), (result_))
#endif


//
// LLAMALOG_INTERNAL_...

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_WARN(message_, ...) LLAMALOG_INTERNAL_LOG(llamalog::Priority::kWarn, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_WARN(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
#if defined(LLAMALOG_LEVEL_ERROR) || defined(LLAMALOG_LEVEL_WARN) || defined(LLAMALOG_LEVEL_INFO) || defined(LLAMALOG_LEVEL_DEBUG) || defined(LLAMALOG_LEVEL_TRACE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_ERROR(message_, ...) LLAMALOG_INTERNAL_LOG(llamalog::Priority::kError, message_, __VA_ARGS__)
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_ERROR(message_, ...) llamalog::internal::Unused(__VA_ARGS__)
#endif


//
// LLAMALOG_PANIC

/// @brief Output a message when logging fails.
/// @details The output is sent to `OutputDebugStringA`.
/// @param message_ The message.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_PANIC(message_) llamalog::internal::Panic(__FILE__, __LINE__, __func__, message_)
