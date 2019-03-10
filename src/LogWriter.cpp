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
_Ret_z_ char const* LogWriter::FormatLogLevel(const LogLevel logLevel) const noexcept {
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
		assert(false);
		return "-";
	}
	__assume(false);
}

// Derived from `format_timestamp` from NanoLog.
std::string LogWriter::FormatTimestamp(const FILETIME& timestamp) const noexcept {
	fmt::basic_memory_buffer<char, 23> buffer;
	FormatTimestampTo(buffer, timestamp);
	return fmt::to_string(buffer);
}

template <typename Out>
void LogWriter::FormatTimestampTo(Out& out, const FILETIME& timestamp) const noexcept {
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&timestamp, &st)) {
		st = {0};
	}
	fmt::format_to(out, "{}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

namespace {

/// @brief Helper function for appending a null-terminated string to a `fmt::basic_memory_buffer`.
/// @tparam Out MUST be of type `fmt::basic_memory_buffer`.
/// @param out The target buffer.
/// @param sz The null-terminated string.
template <typename Out>
inline void Append(Out& out, _In_z_ const char* sz) noexcept {
	out.append(sz, sz + std::strlen(sz));
}

}  // namespace

//
// DebugWriter
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void DebugWriter::Log(const LogLine& logLine) {
	fmt::basic_memory_buffer<char, 256> buffer;

	FormatTimestampTo(buffer, logLine.GetTimestamp());
	buffer.push_back(' ');
	Append(buffer, FormatLogLevel(logLine.GetLevel()));
	fmt::format_to(buffer, " [{}] ", logLine.GetThreadId());

	Append(buffer, logLine.GetFile());
	fmt::format_to(buffer, ":{} ", logLine.GetLine());
	Append(buffer, logLine.GetFunction());
	buffer.push_back(' ');

	std::vector<fmt::basic_format_arg<fmt::format_context>> args;
	logLine.CopyArgumentsTo(args);
	fmt::vformat_to(buffer, fmt::to_string_view(logLine.GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::basic_format_args<fmt::format_context>::size_type>(args.size())));
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
	const char* const szPattern;  ///< @brief Pattern for variable part of filename.
};

/// @brief The data for the various `RollingFileWriter::Frequency` values.
constexpr FrequencyInfo kFrequencyInfos[] = {
	// *10[= microseconds] * 1000[= milliseconds] * 1000[= seconds] * 3600[= hours] * 24[= days] * 7[= weeks]
	{10ui64 * 1000 * 1000 * 3600 * 24, "{}{:0>2}"},              // kMonthly - check every day to make logic easier, code will just re-open the same file every day
	{10ui64 * 1000 * 1000 * 3600 * 24, "{}{:0>2}{:0>2}"},        // kDaily
	{10ui64 * 1000 * 1000 * 3600, "{}{:0>2}{:0>2}_{:0>2}00"},    // kHourly
	{10ui64 * 1000 * 1000 * 60, "{}{:0>2}{:02}_{:0>2}{:0>2}"},   // kEveryMinute
	{10ui64 * 1000 * 1000, "{}{:0>2}{:0>2}_{:0>2}{:0>2}{:0>2}"}  // kEverySecond
};
static_assert(sizeof(kFrequencyInfos) / sizeof(kFrequencyInfos[0]) == static_cast<std::uint8_t>(RollingFileWriter::Frequency::kCount));

}  // namespace

RollingFileWriter::RollingFileWriter(const LogLevel level, std::string directory, std::string fileName, const Frequency frequency, const std::uint32_t maxFiles)
	: LogWriter(level)
	, m_directory(std::move(directory))
	, m_fileName(std::move(fileName))
	, m_frequency(frequency)
	, m_maxFiles(maxFiles) {
	// empty
}

RollingFileWriter::~RollingFileWriter() noexcept {
	if (m_hFile != INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		CloseHandle(m_hFile);
	}
}

// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void RollingFileWriter::Log(const LogLine& logLine) {
	const FILETIME timestamp = logLine.GetTimestamp();
	if (CompareFileTime(&m_nextRollAt, &timestamp) != 1) {
		RollFile(timestamp);
	}

	fmt::basic_memory_buffer<char, 256> buffer;
	FormatTimestampTo(buffer, logLine.GetTimestamp());
	buffer.push_back(' ');
	Append(buffer, FormatLogLevel(logLine.GetLevel()));
	fmt::format_to(buffer, " [{}] ", logLine.GetThreadId());

	Append(buffer, logLine.GetFile());
	fmt::format_to(buffer, ":{} ", logLine.GetLine());
	Append(buffer, logLine.GetFunction());
	buffer.push_back(' ');

	std::vector<fmt::basic_format_arg<fmt::format_context>> args;
	logLine.CopyArgumentsTo(args);
	fmt::vformat_to(buffer, fmt::to_string_view(logLine.GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::basic_format_args<fmt::format_context>::size_type>(args.size())));
	buffer.push_back('\n');

	DWORD cbWritten;
	const char* const __restrict data = buffer.data();
	const std::size_t cbLength = buffer.size();
	for (std::size_t cbPosition = 0; cbPosition < cbLength; cbPosition += cbWritten) {
		// please contact me if you REALLY do log messages whose size does not fit in a DWORD... ;-)
		DWORD count = static_cast<DWORD>(std::min<std::size_t>(std::numeric_limits<DWORD>::max(), cbLength - cbPosition));
		if (!WriteFile(m_hFile, data + cbPosition, count, &cbWritten, nullptr)) {
			fprintf(stderr, "WriteFile");
		}
		if (cbWritten == cbLength) {
			// spare an addition for the most common case
			break;
		}
	}
}

void RollingFileWriter::RollFile(const FILETIME& timestamp) {
	ULARGE_INTEGER nextRollAt;
	nextRollAt.LowPart = timestamp.dwLowDateTime;
	nextRollAt.HighPart = timestamp.dwHighDateTime;

	const FrequencyInfo& frequencyInfo = kFrequencyInfos[static_cast<std::uint8_t>(m_frequency)];
	const std::uint64_t breakpoint = frequencyInfo.breakpoint;
	nextRollAt.QuadPart -= nextRollAt.QuadPart % breakpoint;
	nextRollAt.QuadPart += breakpoint;
	m_nextRollAt.dwLowDateTime = nextRollAt.LowPart;
	m_nextRollAt.dwHighDateTime = nextRollAt.HighPart;

	SYSTEMTIME time;
	if (!FileTimeToSystemTime(&timestamp, &time)) {
	}

	std::filesystem::path path(m_directory);
	std::filesystem::path fileName(m_fileName);

	path /= fileName.filename().stem();
	path += '.';
	std::filesystem::path pattern(path);

	fmt::basic_memory_buffer<char, 16> buffer;
	fmt::format_to(buffer, frequencyInfo.szPattern, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	path += std::string_view(buffer.data(), buffer.size());
	path += fileName.extension();

	buffer.clear();
	fmt::format_to(buffer, frequencyInfo.szPattern, "????", "??", "??", "??", "??", "??", "??");
	pattern += std::string_view(buffer.data(), buffer.size());
	pattern += fileName.extension();

	std::vector<std::wstring> files;
	WIN32_FIND_DATAW findData;
	HANDLE hFindResult = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFindResult == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		if (GetLastError() != ERROR_NO_MORE_FILES) {
			fprintf(stderr, "FindFirstFileExW");
		}
	} else {
		do {
			files.emplace_back(findData.cFileName);
		} while (FindNextFileW(hFindResult, &findData));  // NOLINT(readability-implicit-bool-conversion): Compare BOOL in a condition.
		if (GetLastError() != ERROR_NO_MORE_FILES) {
			fprintf(stderr, "FindNextFileW");
		} else {
			std::sort(files.begin(), files.end());
			for (std::uint32_t i = 0, len = static_cast<std::uint32_t>(files.size()); i < len - std::min(len, m_maxFiles); ++i) {
				std::filesystem::path file = std::filesystem::path(m_directory) / files[i];
				if (!DeleteFileW(file.c_str())) {
					fprintf(stderr, "DeleteFileW");
				}
			}
		}
		if (!FindClose(hFindResult)) {
			fprintf(stderr, "FindClose");
		}
	}

	if (m_hFile != INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		if (!CloseHandle(m_hFile)) {
			fprintf(stderr, "CloseHandle");
		}
		m_hFile = INVALID_HANDLE_VALUE;  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
	}
	// NOLINTNEXTLINE(hicpp-signed-bitwise): FILE_ATTRIBUTE_NORMAL comes from the Windows API.
	m_hFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (m_hFile == INVALID_HANDLE_VALUE) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
		fprintf(stderr, "CreateFileW");
	}

	if (!SetFilePointerEx(m_hFile, {0}, nullptr, FILE_END)) {
		fprintf(stderr, "SetFilePointerEx");
	}
}

}  // namespace llamalog
