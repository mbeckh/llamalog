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

/// @brief The public log interface.
/// @copyright The idea for the public interface is based on `struct NanoLog` from NanoLog.
class Log {
public:
	Log() noexcept = default;
	Log(const Log&) = delete;  ///< @nocopyconstructor
	Log(Log&&) = delete;       ///< @nomoveconstructor

	~Log() noexcept = default;

public:
	Log& operator=(const Log&) = delete;  ///< @noassignmentoperator
	Log& operator=(Log&&) = delete;       ///< @nomoveoperator

	/// @brief Add a log writer.
	/// @param logWriter The `LogWriter` to add.
	/// @return This object to allow method chaining.
	Log& operator+=(std::unique_ptr<LogWriter>&& logWriter);

	/// @brief Add a `LogLine`.
	/// @param logLine The `LogLine` to send to the logger.
	/// @copyright Derived from `Log::operator==` from NanoLog.
	void operator+=(LogLine& logLine);

	/// @brief Add a `LogLine`.
	/// @param logLine The `LogLine` to send to the logger.
	/// @copyright Derived from `Log::operator==` from NanoLog.
	void operator+=(LogLine&& logLine);
};

/// @brief Initialize the logger.
/// @note `Initialize` MUST be called before any logging takes place.
/// @copyright Derived from `initialize` from NanoLog.
void Initialize(std::unique_ptr<LogWriter>&& writer);
void Initialize();
void Shutdown();

inline void LogX2(LogLine&) {
}

template <typename T>
void LogX2(LogLine& logLine, T&& arg) {
	logLine << std::forward<T>(arg);
}

template <typename T, typename... R>
void LogX2(LogLine& logLine, T&& arg, R&&... args) {
	logLine << std::forward<T>(arg);
	LogX2(logLine, std::forward<R>(args)...);
}

template <typename... T>
void LogX(LogLevel level, const char* szFile, uint32_t line, const char* szFunction, const char* szMessage, T&&... args) {
	LogLine logLine(level, szFile, line, szFunction, szMessage);
	LogX2(logLine, std::forward<T>(args)...);
	llamalog::Log() += logLine;
}

}  // namespace llamalog

/// @brief Emit a log line.
/// @param level_ The LogLevel.
/// @param szMessage_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
/// @copyright This macro is based on `NANO_LOG` from NanoLog.
#define LLAMALOG_LOG(level_, ...) llamalog::LogX(level_, __FILE__, __LINE__, __func__, __VA_ARGS__)

//#define M3C_LOG(level_, szMessage_) llamalog::Log() += llamalog::LogLine(level_, __FILE__, __func__, __LINE__, szMessage_)
//#define LOG_INFO(szMessage_) llamalog::IsLogged(llamalog::LogLevel::Info) && M3C_LOG(llamalog::LogLevel::Info, szMessage_)
//#define LOG_WARN(szMessage_) llamalog::IsLogged(llamalog::LogLevel::Warn) && M3C_LOG(llamalog::LogLevel::Warn, szMessage_)
//#define LOG_ERROR(szMessage_) llamalog::IsLogged(llamalog::LogLevel::Error) && M3C_LOG(llamalog::LogLevel::Error, szMessage_)
