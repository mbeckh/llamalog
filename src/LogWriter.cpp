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
#include "llamalog/Logger.h"
#include "llamalog/winapi_log.h"

#include <fmt/format.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace llamalog {

//
// LogWriter
//

LogWriter::LogWriter(const Priority priority) noexcept
	: m_priority(priority) {
	std::atomic_thread_fence(std::memory_order_release);
}

// Derived from `is_logged(LogLevel)` from NanoLog.
bool LogWriter::IsLogged(const Priority priority) const noexcept {
	return priority >= m_priority.load(std::memory_order_relaxed);
}

// Derived from `set_log_level(LogLevel)` from NanoLog.
void LogWriter::SetPriority(const Priority priority) noexcept {
	m_priority.store(priority, std::memory_order_release);
}

// Derived from `to_string(LogLevel)` from NanoLog.
__declspec(noalias) _Ret_z_ char const* LogWriter::FormatPriority(const Priority priority) noexcept {
	switch (priority) {
	case Priority::kTrace:
		return "TRACE";
	case Priority::kDebug:
		return "DEBUG";
	case Priority::kInfo:
		return "INFO";
	case Priority::kWarn:
		return "WARN";
	case Priority::kError:
		return "ERROR";
	case Priority::kFatal:
		return "FATAL";
	default:
		if (static_cast<std::uint8_t>(priority) & 3u) {
			return FormatPriority(static_cast<Priority>(static_cast<std::uint8_t>(priority) & ~3u));
		}
		assert(false);
		return "-";
	}
	__assume(false);
}

namespace {

/// @brief The pattern for formatting a timestamp.
constexpr const char kTimestampPattern[] = "{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}";

/// @brief The size of the output buffer for a formatted timestamp.
constexpr const std::size_t kTimestampOutputBufferSize = 24;

}  // namespace

// Derived from `format_timestamp` from NanoLog.
std::string LogWriter::FormatTimestamp(const FILETIME& timestamp) {
	fmt::basic_memory_buffer<char, kTimestampOutputBufferSize> buffer;
	FormatTimestampTo(buffer, timestamp);
	return fmt::to_string(buffer);
}

template <typename Out>
void LogWriter::FormatTimestampTo(Out& out, const FILETIME& timestamp) {
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&timestamp, &st)) {
		st = {0};
	}
	fmt::format_to(out, kTimestampPattern, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

namespace {

/// @brief Helper function for appending a null-terminated string to a `fmt::basic_memory_buffer`.
/// @tparam Out MUST be of type `fmt::basic_memory_buffer`.
/// @param out The target buffer.
/// @param sz The null-terminated string.
template <typename Out>
inline void Append(Out& out, _In_z_ const char* sz) {
	out.append(sz, sz + std::strlen(sz));
}

/// @brief Default buffer size for log lines.
constexpr std::size_t kDefaultBufferSize = 256;

}  // namespace


//
// StdErrWriter
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void StdErrWriter::Log(const LogLine& logLine) {
	fmt::basic_memory_buffer<char, kDefaultBufferSize> buffer;

	FormatTimestampTo(buffer, logLine.GetTimestamp());
	buffer.push_back(' ');
	Append(buffer, FormatPriority(logLine.GetPriority()));
	fmt::format_to(buffer, " [{}] ", logLine.GetThreadId());

	Append(buffer, logLine.GetFile());
	fmt::format_to(buffer, ":{} ", logLine.GetLine());
	Append(buffer, logLine.GetFunction());
	buffer.push_back(' ');

	std::vector<fmt::format_context::format_arg> args;
	logLine.CopyArgumentsTo(args);
	fmt::vformat_to(buffer, fmt::to_string_view(logLine.GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	buffer.push_back('\n');
	buffer.push_back('\0');

	fputs(buffer.data(), stderr);
}


//
// DebugWriter
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void DebugWriter::Log(const LogLine& logLine) {
	fmt::basic_memory_buffer<char, kDefaultBufferSize> buffer;

	FormatTimestampTo(buffer, logLine.GetTimestamp());
	buffer.push_back(' ');
	Append(buffer, FormatPriority(logLine.GetPriority()));
	fmt::format_to(buffer, " [{}] ", logLine.GetThreadId());

	Append(buffer, logLine.GetFile());
	fmt::format_to(buffer, ":{} ", logLine.GetLine());
	Append(buffer, logLine.GetFunction());
	buffer.push_back(' ');

	std::vector<fmt::format_context::format_arg> args;
	logLine.CopyArgumentsTo(args);
	fmt::vformat_to(buffer, fmt::to_string_view(logLine.GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	buffer.push_back('\n');
	buffer.push_back('\0');

	OutputDebugStringA(buffer.data());
}


//
// RollingFileWriter
//

namespace {

/// @brief Data for calculation next roll-over and filename.
struct FrequencyInfo {
	// NOLINTNEXTLINE(readability-identifier-naming): Public member does not have prefix even if constant.
	const std::uint64_t breakpoint;  ///< @brief Modulus for calculation the next roll over.
	// NOLINTNEXTLINE(readability-identifier-naming): Public member does not have prefix even if constant.
	const char* const pattern;  ///< @brief Pattern for variable part of filename.
};

/// @brief The data for the various `RollingFileWriter::Frequency` values.
constexpr FrequencyInfo kFrequencyInfos[] = {
	// *10[= microseconds] * 1000[= milliseconds] * 1000[= seconds] * 3600[= hours] * 24[= days] * 7[= weeks]
	{10ui64 * 1000 * 1000 * 3600 * 24, "{:0>4}{:0>2}"},              // kMonthly - check every day to make logic easier, code will just re-open the same file every day
	{10ui64 * 1000 * 1000 * 3600 * 24, "{:0>4}{:0>2}{:0>2}"},        // kDaily
	{10ui64 * 1000 * 1000 * 3600, "{:0>4}{:0>2}{:0>2}_{:0>2}"},      // kHourly
	{10ui64 * 1000 * 1000 * 60, "{:0>4}{:0>2}{:0>2}_{:0>2}{:0>2}"},  // kEveryMinute
	{10ui64 * 1000 * 1000, "{:0>4}{:0>2}{:0>2}_{:0>2}{:0>2}{:0>2}"}  // kEverySecond
};
static_assert(sizeof(kFrequencyInfos) / sizeof(kFrequencyInfos[0]) == static_cast<std::uint8_t>(RollingFileWriter::Frequency::kCount));

/// @brief Maximum size of output buffer for the kFrequenceInfos patterns.
constexpr std::size_t kFrequencyOutputBufferSize = 16;

}  // namespace

RollingFileWriter::RollingFileWriter(const Priority priority, std::string directory, std::string fileName, const Frequency frequency, const std::uint32_t maxFiles) noexcept
	: LogWriter(priority)
	, m_directory(std::move(directory))
	, m_fileName(std::move(fileName))
	, m_frequency(frequency)
	, m_maxFiles(maxFiles) {
	// empty
}

RollingFileWriter::~RollingFileWriter() noexcept {
	if (m_hFile != INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		if (!CloseHandle(m_hFile)) {
			try {
				LLAMALOG_INTERNAL_WARN("Error closing log: {}", LastError());
			} catch (...) {
				LLAMALOG_PANIC("Error closing log");
			}
		}
	}
}

// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void RollingFileWriter::Log(const LogLine& logLine) {
	const FILETIME timestamp = logLine.GetTimestamp();

	// also try to roll when the file is invalid
	if (CompareFileTime(&m_nextRollAt, &timestamp) != 1 || m_hFile == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		RollFile(logLine);
	}

	fmt::basic_memory_buffer<char, kDefaultBufferSize> buffer;
	FormatTimestampTo(buffer, timestamp);
	buffer.push_back(' ');
	Append(buffer, FormatPriority(logLine.GetPriority()));
	fmt::format_to(buffer, " [{}] ", logLine.GetThreadId());

	Append(buffer, logLine.GetFile());
	fmt::format_to(buffer, ":{} ", logLine.GetLine());
	Append(buffer, logLine.GetFunction());
	buffer.push_back(' ');

	std::vector<fmt::format_context::format_arg> args;
	logLine.CopyArgumentsTo(args);
	fmt::vformat_to(buffer, fmt::to_string_view(logLine.GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	buffer.push_back('\n');

	DWORD written;  // NOLINT(cppcoreguidelines-init-variables): Initialized before first use.
	const char* const __restrict data = buffer.data();
	const std::size_t length = buffer.size();
	for (std::size_t position = 0; position < length; position += written) {
		// it will work, however please contact me if you REALLY do log messages whose size does not fit in a DWORD... ;-)
		const DWORD count = static_cast<DWORD>(std::min<std::size_t>(std::numeric_limits<DWORD>::max(), length - position));
		if (!WriteFile(m_hFile, data + position, count, &written, nullptr)) {
			if (m_hFile != INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
				// no need to log if there is no file
				LLAMALOG_INTERNAL_ERROR("Error writing {} bytes to log: {}", count, LastError());
			}
			// try the next event
			return;
		}
		if (written == length) {
			// spare an addition for the most common case
			break;
		}
	}
}

void RollingFileWriter::RollFile(const LogLine& logLine) {
	const FILETIME timestamp = logLine.GetTimestamp();

	ULARGE_INTEGER nextRollAt = {.LowPart = timestamp.dwLowDateTime, .HighPart = timestamp.dwHighDateTime};

	const FrequencyInfo& frequencyInfo = kFrequencyInfos[static_cast<std::uint8_t>(m_frequency)];
	const std::uint64_t breakpoint = frequencyInfo.breakpoint;
	nextRollAt.QuadPart -= nextRollAt.QuadPart % breakpoint;
	nextRollAt.QuadPart += breakpoint;
	m_nextRollAt.dwLowDateTime = nextRollAt.LowPart;
	m_nextRollAt.dwHighDateTime = nextRollAt.HighPart;

	SYSTEMTIME time;
	if (!FileTimeToSystemTime(&timestamp, &time)) {
		LLAMALOG_INTERNAL_ERROR("Error rolling log: {}", LastError());
		// if we can't get the time, we can't roll the file, try again at next log event
		return;
	}

	std::filesystem::path path(m_directory);
	std::filesystem::path fileName(m_fileName);

	path /= fileName.filename().stem();
	path += '.';
	std::filesystem::path pattern(path);

	fmt::basic_memory_buffer<char, kFrequencyOutputBufferSize> buffer;
	fmt::format_to(buffer, frequencyInfo.pattern, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	path += std::string_view(buffer.data(), buffer.size());
	path += fileName.extension();

	buffer.clear();
	fmt::format_to(buffer, frequencyInfo.pattern, "????", "??", "??", "??", "??", "??", "??");
	pattern += std::string_view(buffer.data(), buffer.size());
	pattern += fileName.extension();

	if (m_hFile != INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		if (!CloseHandle(m_hFile)) {
			LLAMALOG_INTERNAL_WARN("Error closing log: {}", LastError());
			// if we can't close the file, leave it open
		}
	}

	// Clean any old log files.
	std::vector<std::wstring> files;
	WIN32_FIND_DATAW findData;
	HANDLE hFindResult = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFindResult == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		if (const DWORD lastError = GetLastError(); lastError != ERROR_FILE_NOT_FOUND) {
			LLAMALOG_INTERNAL_WARN("Error deleting log: {}", error_code{lastError});
		}
	} else {
		do {
			files.emplace_back(findData.cFileName);
		} while (FindNextFileW(hFindResult, &findData));  // NOLINT(readability-implicit-bool-conversion): Compare BOOL in a condition.
		const DWORD error = GetLastError();
		if (error != ERROR_NO_MORE_FILES) {
			LLAMALOG_INTERNAL_WARN("Error deleting log: {}", error_code{error});
			// do not try to delete anything on find errors
		} else {
			std::sort(files.begin(), files.end());
			const std::uint32_t len = static_cast<std::uint32_t>(files.size());
			for (std::uint32_t i = 0; i < len - std::min(len, m_maxFiles); ++i) {
				std::filesystem::path file = std::filesystem::path(m_directory) / files[i];
				if (!DeleteFileW(file.c_str())) {
					LLAMALOG_INTERNAL_WARN("Error deleting log '{}': {}", files[i], LastError());
					// delete failed, but continue
				}
			}
		}
		if (!FindClose(hFindResult)) {
			LLAMALOG_INTERNAL_WARN("Error deleting log: {}", LastError());
			// just leave the handle open
		}
	}

	// NOLINTNEXTLINE(hicpp-signed-bitwise): FILE_ATTRIBUTE_NORMAL comes from the Windows API.
	m_hFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (m_hFile == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		LLAMALOG_INTERNAL_ERROR("Error creating log: {}", LastError());
		// no file created, try again for next message
	}

	// using CreateFile with FILE_APPEND_DATA does not requre a SetFilePointer to the end
}

}  // namespace llamalog
