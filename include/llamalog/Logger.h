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
// IWYU pragma: no_include "llamalog/WindowsTypes.h"

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
/// @note `start` MUST be called after `initialize` before any logging takes place.
/// @copyright Derived from `initialize` from NanoLog.
void initialize();

/// @brief Actually start logging.
/// @note `start` MUST be called after `initialize` before any logging takes place.
void start();

/// @brief Calculate the `Priority` for internal logging messages.
/// @remarks The function prevents endless loops by encoding an error counter in the priority.
/// @param priority The desired logging priority.
/// @return The `Priority` to use for an internal logging message.
[[nodiscard]] Priority getInternalPriority(Priority priority) noexcept;

/// @brief Log a message if logging fails.
/// @details The output is sent to `OutputDebugStringA`.
/// @param file The file where the message is created.
/// @param line The line where the message is created.
/// @param function The function where the message is created.
/// @param message The message to log.
void panic(const char* file, std::uint32_t line, const char* function, const char* message) noexcept;

}  // namespace internal

/// @brief Initialize the logger, add writers and start logging.
/// @note `initialize` MUST be called before any logging takes place.
/// @tparam LogWriter MUST be of type `LogWriter`.
/// @param writers One or more `LogWriter` objects.
template <typename... LogWriter>
void initialize(std::unique_ptr<LogWriter>&&... writers) {
	internal::initialize();
	(..., addWriter(std::move(writers)));
	internal::start();
}

/// @brief Add a log writer.
/// @param writer The `LogWriter` to add.
void addWriter(std::unique_ptr<LogWriter>&& writer);

/// @brief Get the filename after the last slash or backslash character.
/// @param path A start of the path to shorten.
/// @param check The next character to check.
/// @return Filename and extension after the last path separator.
[[nodiscard]] constexpr __declspec(noalias) _Ret_z_ const char* getFilename(_In_z_ const char* const path, _In_opt_z_ const char* const check = nullptr) noexcept {
	if (!check) {
		// first call
		return getFilename(path, path);
	}
	if (*check) {
		if (*check == '/' || *check == '\\') {
			return getFilename(check + 1, check + 1);
		}
		return getFilename(path, check + 1);
	}
	return path;
}


/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void log(LogLine& logLine);

/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void log(LogLine&& logLine);

/// @brief Logs a new `LogLine`.
/// @tparam T The types of the message arguments.
/// @param priority The `#Priority`.
/// @param file The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param function The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p message.
template <typename... T>
void log(const Priority priority, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	LogLine logLine(priority, file, line, function, message);
	log((logLine << ... << std::forward<T>(args)));
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
void logInternal(const Priority priority, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	const Priority internalPriority = internal::getInternalPriority(priority);
	if ((static_cast<std::uint8_t>(internalPriority) & 3u) == 3u) {
		internal::panic(file, line, function, "Error logging error");
		return;
	}
	LogLine logLine(internalPriority, file, line, function, message);
	log((logLine << ... << std::forward<T>(args)));
}

/// @brief Waits until all currently available entries have been written.
/// @details This function might block for a long time and its main purpose is to flush the log for testing.
/// Use with care in your own code.
void flush();

/// @brief End all logging. This MUST be the last function called.
void shutdown() noexcept;

}  // namespace llamalog

//
// Macros
//

/// @brief Emit a log line. @details Without the explicit variable `file_` the compiler does not reliably evaluate
/// `#llamalog::getFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param message_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_LOG(priority_, message_, ...)                                      \
	do {                                                                            \
		constexpr const char* file_ = llamalog::getFilename(__FILE__);              \
		llamalog::log(priority_, file_, __LINE__, __func__, message_, __VA_ARGS__); \
	} while (0)

/// @brief Emit a log line for an internal message from the logger itself.
/// @details Without the explicit variable `file_` the compiler does not reliably evaluate
/// `#llamalog::getFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param message_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_LOG(priority_, message_, ...)                                     \
	do {                                                                                    \
		constexpr const char* file_ = llamalog::getFilename(__FILE__);                      \
		llamalog::logInternal(priority_, file_, __LINE__, __func__, message_, __VA_ARGS__); \
	} while (0)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE(message_, ...) LLAMALOG_LOG(llamalog::Priority::kTrace, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param result_ The function return value. The value is available as argument `{0}` when formatting.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_TRACE_RESULT(result_, message_, ...)                                                             \
	[&](decltype(result_) result, const char* const function) -> decltype(result_) {                         \
		constexpr const char* file_ = llamalog::getFilename(__FILE__);                                       \
		llamalog::log(llamalog::Priority::kTrace, file_, __LINE__, function, message_, result, __VA_ARGS__); \
		return result;                                                                                       \
	}(result_, __func__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_DEBUG(message_, ...) LLAMALOG_LOG(llamalog::Priority::kDebug, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_INFO(message_, ...) LLAMALOG_LOG(llamalog::Priority::kInfo, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_WARN(message_, ...) LLAMALOG_LOG(llamalog::Priority::kWarn, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_ERROR(message_, ...) LLAMALOG_LOG(llamalog::Priority::kError, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal`.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LOG_FATAL(message_, ...) LLAMALOG_LOG(llamalog::Priority::kFatal, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_WARN(message_, ...) LLAMALOG_INTERNAL_LOG(llamalog::Priority::kWarn, message_, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_INTERNAL_ERROR(message_, ...) LLAMALOG_INTERNAL_LOG(llamalog::Priority::kError, message_, __VA_ARGS__)

/// @brief Output a message when logging fails.
/// @details The output is sent to `OutputDebugStringA`.
/// @param message_ The message.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_PANIC(message_) llamalog::internal::panic(__FILE__, __LINE__, __func__, message_)
