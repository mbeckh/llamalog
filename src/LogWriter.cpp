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

#include "llamalog/LogWriter.h"

#include "llamalog/LogLine.h"

#include <fmt/format.h>

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace llamalog {

//
// LogWriter
//

LogWriter::LogWriter(const LogLevel level) noexcept
	: m_logLevel(level) {
	std::atomic_thread_fence(std::memory_order_release);
}

// Derived from `is_logged(LogLevel)` from NanoLog.
bool LogWriter::IsLogged(const LogLevel level) const noexcept {
	return level >= m_logLevel.load(std::memory_order_relaxed);
}

// Derived from `set_log_level(LogLevel)` from NanoLog.
void LogWriter::SetLogLevel(const LogLevel level) noexcept {
	m_logLevel.store(level, std::memory_order_release);
}

// Derived from `to_string(LogLevel)` from NanoLog.
char const* LogWriter::FormatLogLevel(const LogLevel logLevel) noexcept {
	switch (logLevel) {
	case LogLevel::kTrace:
		return "TRACE";
	case LogLevel::kDebug:
		return "DEBUG";
	case LogLevel::kInfo:
		return "INFO";
	case LogLevel::kWarn:
		return "WARN";
	case LogLevel::kError:
		return "ERROR";
	case LogLevel::kFatal:
		return "FATAL";
	default:
		return "-";
	}
	__assume(false);
}

// Derived from `format_timestamp` from NanoLog.
std::string LogWriter::FormatTimestamp(const FILETIME& timestamp) {
	SYSTEMTIME st;
	if (FileTimeToSystemTime(&timestamp, &st) != FALSE) {
		static thread_local fmt::basic_memory_buffer<char, 23> buffer;
		buffer.clear();
		fmt::format_to(buffer, "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		return std::string(buffer.data(), buffer.size());
	}
	return "0000-00-00 00:00:00.000";
}


//
// DebugWriter
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void DebugWriter::Log(const LogLine& logLine) {
	fmt::basic_memory_buffer<char, 256> buffer;
	fmt::format_to(buffer, "{} {} [{}] {}:{} {} {}\n\0",
				   FormatTimestamp(logLine.GetTimestamp()),
				   FormatLogLevel(logLine.GetLevel()),
				   logLine.GetThreadId(),
				   logLine.GetFile(),
				   logLine.GetLine(),
				   logLine.GetFunction(),
				   logLine.GetLogMessage());

	OutputDebugStringA(buffer.data());
}


//
// DailyRollingFileWriter
//

DailyRollingFileWriter::DailyRollingFileWriter(const LogLevel level, std::string directory, std::string fileName)
	: LogWriter(level)
	, m_directory(std::move(directory))
	, m_fileName(std::move(fileName)) {
	// empty
}

// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void DailyRollingFileWriter::Log(const LogLine& logLine) {
	const FILETIME timestamp = logLine.GetTimestamp();
	if (CompareFileTime(&m_nextRollAt, &timestamp) != 1) {
		RollFile();
	}

	*m_os << FormatTimestamp(timestamp) << ' '
		  << FormatLogLevel(logLine.GetLevel())
		  << " [" << logLine.GetThreadId() << "] "
		  << logLine.GetFile() << ':' << logLine.GetLine() << ' '
		  << logLine.GetFunction() << ' '
		  << logLine.GetLogMessage() << std::endl;
}

void DailyRollingFileWriter::RollFile() {
	SYSTEMTIME time;
	GetSystemTime(&time);

	SystemTimeToFileTime(&time, &m_nextRollAt);
	ULARGE_INTEGER li;
	li.LowPart = m_nextRollAt.dwLowDateTime;
	li.HighPart = m_nextRollAt.dwHighDateTime;

	// file time is 100ns intervals
	static constexpr std::uint64_t kOneDay = 24 * 3600 * 10;
	li.QuadPart -= li.QuadPart % kOneDay;
	li.QuadPart += kOneDay;
	m_nextRollAt.dwLowDateTime = li.LowPart;
	m_nextRollAt.dwHighDateTime = li.HighPart;

	fmt::basic_memory_buffer<char, 12> buffer;
	fmt::format_to(buffer, ".{:04}-{:02}-{:02}", time.wYear, time.wMonth, time.wDay);
	std::filesystem::path path(m_directory);
	std::filesystem::path fileName(m_fileName);

	path /= fileName.filename();
	path += std::string_view(buffer.data(), buffer.size());
	path += fileName.extension();

	m_os = std::make_unique<std::ofstream>(path, std::ofstream::out | std::ofstream::ate);
}

}  // namespace llamalog
