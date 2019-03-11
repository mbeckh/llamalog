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

#include <memory>

/// @brief The main namespace.
namespace llamalog {

class LogWriter;

/// @brief Initialize the logger.
/// @note `Initialize` MUST be called before any logging takes place.
/// @copyright Derived from `initialize` from NanoLog.
void Initialize();

/// @brief Initialize the logger.
/// @note `Initialize` MUST be called before any logging takes place.
/// @tparam LogWriter MUST be of type `LogWriter`.
/// @param writers One or more `LogWriter` objects.
template <typename... LogWriter>
void Initialize(std::unique_ptr<LogWriter>&&... writers) {
	Initialize();
	(..., AddWriter(std::move(writers)));
}

/// @brief Add a log writer.
/// @param writer The `LogWriter` to add.
void AddWriter(std::unique_ptr<LogWriter>&& writer);

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
/// @param level The `#LogLevel`.
/// @param szFile The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param szFunction The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param szMessage The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p szMessage.
template <typename... T>
void Log(const LogLevel level, _In_z_ const char* __restrict const szFile, const std::uint32_t line, _In_z_ const char* __restrict const szFunction, _In_z_ const char* __restrict const szMessage, T&&... args) {
	Log((LogLine(level, szFile, line, szFunction, szMessage) << ... << args));
}

/// @brief End all logging. This MUST be the last function called.
void Shutdown() noexcept;

/// @brief Internal methods which are not part of the API but which must be included in a header for technical reasons.
namespace internal {

/// @brief Get the filename after the last slash or backslash character.
/// @param szPath A path.
/// @param szStart The character after the last seen path separator.
/// @return Filename and extension after the last path separator.
constexpr _Ret_z_ const char* GetFilename(_In_z_ const char* const szPath, _In_opt_z_ const char* const szStart = nullptr) noexcept {
	if (!*szPath) {
		return szStart ? szStart : szPath;
	}
	if (*szPath == '/' || *szPath == '\\') {
		return GetFilename(szPath + 1, szPath + 1);
	}
	return GetFilename(szPath + 1, szStart);
}

}  // namespace internal

}  // namespace llamalog

/// @brief Emit a log line. @details Without the explicit variable `szFile_` the compiler does not reliably evaluate
/// `#llamalog::internal::GetFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param level_ The LogLevel.
/// @param szMessage_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
#define LLAMALOG_LOG(level_, szMessage_, ...)                                        \
	do {                                                                             \
		constexpr const char* szFile_ = llamalog::internal::GetFilename(__FILE__);   \
		llamalog::Log(level_, szFile_, __LINE__, __func__, szMessage_, __VA_ARGS__); \
	} while (0)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kTrace`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_TRACE(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kTrace, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kDebug`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_DEBUG(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kDebug, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kInfo`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_INFO(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kInfo, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kWarn`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_WARN(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kWarn, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kError`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_ERROR(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kError, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::LogLevel` `#llamalog::LogLevel::kFatal`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_FATAL(szMessage, ...) LLAMALOG_LOG(llamalog::LogLevel::kFatal, szMessage, __VA_ARGS__)
