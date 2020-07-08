/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file
/// @copyright Code marked with "from NanoLog" is based on NanoLog (https://github.com/Iyengar111/NanoLog, commit
/// 40a53c36e0336af45f7664abeb939f220f78273e), copyright 2016 Karthik Iyengar and distributed under the MIT License.

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

#include "llamalog/LogLine.h"

#include "buffer_management.h"
#include "exception_types.h"
#include "marker_types.h"

#include "llamalog/Logger.h"
#include "llamalog/custom_types.h"
#include "llamalog/exception.h"
#include "llamalog/modifier_types.h"

#include <fmt/format.h>

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace llamalog {

using buffer::CallDestructors;
using buffer::CopyArgumentsFromBufferTo;
using buffer::CopyObjects;
using buffer::GetNextChunk;
using buffer::GetPadding;
using buffer::GetTypeId;
using buffer::kTypeSize;
using buffer::MoveObjects;
using buffer::TypeId;

using marker::NonTriviallyCopyable;
using marker::NullValue;
using marker::TriviallyCopyable;

using exception::ExceptionInformation;
using exception::HeapBasedException;
using exception::HeapBasedSystemError;
using exception::PlainException;
using exception::PlainSystemError;
using exception::StackBasedException;
using exception::StackBasedSystemError;

//
// Definitions
//

namespace {

static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= std::numeric_limits<LogLine::Align>::max(), "type LogLine::Align is too small");

/// @brief Get the id of the current thread.
/// @return The id of the current thread.
/// @copyright The function is based on `this_thread_id` from NanoLog.
[[nodiscard]] DWORD GetCurrentThreadId() noexcept {
	static const thread_local DWORD kId = ::GetCurrentThreadId();
	return kId;
}

/// @brief Get information for `std::error_code`.
/// @note The function MUST be called from within a catch block to get the object, else `nullptr` is returned.
/// @return A pointer which is set if the exception is of type `std::system_error` or `system_error` (else `nullptr`).
[[nodiscard]] _Ret_maybenull_ const std::error_code* GetCurrentExceptionCode() noexcept {
	try {
		throw;
	} catch (const system_error& e) {
		return &e.code();  // using address is valid because code() returns reference
	} catch (const std::system_error& e) {
		return &e.code();  // using address is valid because code() returns reference
	} catch (...) {
		return nullptr;
	}
}

}  // namespace

#pragma warning(suppress : 26495)
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): m_timestamp and m_stackBuffer need no initialization.
LogLine::LogLine(const Priority priority, _In_z_ const char* __restrict file, std::uint32_t line, _In_z_ const char* __restrict function, _In_opt_z_ const char* __restrict message) noexcept
	: m_priority(priority)
	, m_file(file)
	, m_function(function)
	, m_message(message)
	, m_threadId(llamalog::GetCurrentThreadId())
	, m_line(line) {
	// ensure proper memory layout
	static_assert(sizeof(LogLine) == LLAMALOG_LOGLINE_SIZE, "size of LogLine");
	static_assert(offsetof(LogLine, m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0, "alignment of LogLine::m_stackBuffer");

	static_assert(offsetof(LogLine, m_stackBuffer) == 0, "offset of m_stackBuffer");
#if UINTPTR_MAX == UINT64_MAX
	static_assert(offsetof(LogLine, m_priority) == LLAMALOG_LOGLINE_SIZE - 58, "offset of m_priority");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_hasNonTriviallyCopyable) == LLAMALOG_LOGLINE_SIZE - 57, "offset of m_hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_timestamp) == LLAMALOG_LOGLINE_SIZE - 56, "offset of m_timestamp");                              // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_file) == LLAMALOG_LOGLINE_SIZE - 48, "offset of m_file");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_function) == LLAMALOG_LOGLINE_SIZE - 40, "offset of m_function");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_message) == LLAMALOG_LOGLINE_SIZE - 32, "offset of m_message");                                  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_threadId) == LLAMALOG_LOGLINE_SIZE - 24, "offset of m_threadId");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_line) == LLAMALOG_LOGLINE_SIZE - 20, "offset of m_line");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_used) == LLAMALOG_LOGLINE_SIZE - 16, "offset of m_used");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_size) == LLAMALOG_LOGLINE_SIZE - 12, "offset of m_size");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_heapBuffer) == LLAMALOG_LOGLINE_SIZE - 8, "offset of m_heapBuffer");                             // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.

	static_assert(sizeof(ExceptionInformation) == 48);                                                                  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(ExceptionInformation, hasNonTriviallyCopyable) == 46, "offset of hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(sizeof(StackBasedException) == 48);                                                                   // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(sizeof(HeapBasedException) == 56);                                                                    // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(HeapBasedException, pHeapBuffer) == 48, "offset of pHeapBuffer");                            // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
#elif UINTPTR_MAX == UINT32_MAX
	static_assert(offsetof(LogLine, m_priority) == LLAMALOG_LOGLINE_SIZE - 42, "offset of m_priority");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_hasNonTriviallyCopyable) == LLAMALOG_LOGLINE_SIZE - 41, "offset of m_hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_timestamp) == LLAMALOG_LOGLINE_SIZE - 40, "offset of m_timestamp");                              // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_file) == LLAMALOG_LOGLINE_SIZE - 32, "offset of m_file");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_function) == LLAMALOG_LOGLINE_SIZE - 28, "offset of m_function");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_message) == LLAMALOG_LOGLINE_SIZE - 24, "offset of m_message");                                  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_threadId) == LLAMALOG_LOGLINE_SIZE - 20, "offset of m_threadId");                                // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_line) == LLAMALOG_LOGLINE_SIZE - 16, "offset of m_line");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_used) == LLAMALOG_LOGLINE_SIZE - 12, "offset of m_used");                                        // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_size) == LLAMALOG_LOGLINE_SIZE - 8, "offset of m_size");                                         // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(LogLine, m_heapBuffer) == LLAMALOG_LOGLINE_SIZE - 4, "offset of m_heapBuffer");                             // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.

	static_assert(sizeof(ExceptionInformation) == 36);                                                                  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(ExceptionInformation, hasNonTriviallyCopyable) == 34, "offset of hasNonTriviallyCopyable");  // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(sizeof(StackBasedException) == 36);                                                                   // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(sizeof(HeapBasedException) == 40);                                                                    // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
	static_assert(offsetof(HeapBasedException, pHeapBuffer) == 36, "offset of pHeapBuffer");                            // NOLINT(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Assert exact layout.
#else
	static_assert(false, "layout assertions not defined");
#endif

	// Struct for arguments
	static_assert(offsetof(ExceptionInformation, timestamp) == offsetof(LogLine, m_timestamp) - offsetof(LogLine, m_timestamp), "offset of timestamp");
	static_assert(offsetof(ExceptionInformation, file) == offsetof(LogLine, m_file) - offsetof(LogLine, m_timestamp), "offset of file");
	static_assert(offsetof(ExceptionInformation, function) == offsetof(LogLine, m_function) - offsetof(LogLine, m_timestamp), "offset of function");
	static_assert(offsetof(ExceptionInformation, message) == offsetof(LogLine, m_message) - offsetof(LogLine, m_timestamp), "offset of message");
	static_assert(offsetof(ExceptionInformation, threadId) == offsetof(LogLine, m_threadId) - offsetof(LogLine, m_timestamp), "offset of threadId");
	static_assert(offsetof(ExceptionInformation, line) == offsetof(LogLine, m_line) - offsetof(LogLine, m_timestamp), "offset of line");
	static_assert(offsetof(ExceptionInformation, used) == offsetof(LogLine, m_used) - offsetof(LogLine, m_timestamp), "offset of used");
	static_assert(offsetof(ExceptionInformation, padding) == offsetof(ExceptionInformation, hasNonTriviallyCopyable) + sizeof(ExceptionInformation::hasNonTriviallyCopyable), "offset of padding");
	static_assert(offsetof(ExceptionInformation, padding) == sizeof(ExceptionInformation) - sizeof(ExceptionInformation::padding), "length of padding");

	static_assert(sizeof(StackBasedException) == sizeof(ExceptionInformation), "size of StackBasedException");
	static_assert(sizeof(HeapBasedException) == sizeof(ExceptionInformation) + sizeof(HeapBasedException::pHeapBuffer), "size of HeapBasedException");
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogLine::LogLine(const LogLine& logLine)
	: m_priority(logLine.m_priority)
	, m_hasNonTriviallyCopyable(logLine.m_hasNonTriviallyCopyable)
	, m_timestamp(logLine.m_timestamp)
	, m_file(logLine.m_file)
	, m_function(logLine.m_function)
	, m_message(logLine.m_message)
	, m_threadId(logLine.m_threadId)
	, m_line(logLine.m_line)
	, m_used(logLine.m_used)
	, m_size(logLine.m_size) {
	if (logLine.m_heapBuffer) {
		m_heapBuffer = std::make_unique<std::byte[]>(m_used);
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_heapBuffer.get(), m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), logLine.m_heapBuffer.get(), m_used);
		}
	} else {
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_used);
		}
	}
}

#pragma warning(suppress : 26495)
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): Initialization of m_stackBuffer depends on data.
LogLine::LogLine(LogLine&& logLine) noexcept
	: m_priority(logLine.m_priority)
	, m_hasNonTriviallyCopyable(logLine.m_hasNonTriviallyCopyable)
	, m_timestamp(logLine.m_timestamp)
	, m_file(logLine.m_file)
	, m_function(logLine.m_function)
	, m_message(logLine.m_message)
	, m_threadId(logLine.m_threadId)
	, m_line(logLine.m_line)
	, m_used(logLine.m_used)
	, m_size(logLine.m_size)
	, m_heapBuffer(std::move(logLine.m_heapBuffer)) {
	if (!m_heapBuffer) {
		if (m_hasNonTriviallyCopyable) {
			MoveObjects(logLine.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_used);
		}
	}
	// leave source in a consistent state
	logLine.m_used = 0;
	logLine.m_size = sizeof(m_stackBuffer);
}

LogLine::~LogLine() noexcept {
	if (m_hasNonTriviallyCopyable) {
		CallDestructors(GetBuffer(), m_used);
	}
}

// NOLINTNEXTLINE(bugprone-unhandled-self-assignment, cert-oop54-cpp): Assert that type is never assigned to itself.
LogLine& LogLine::operator=(const LogLine& logLine) {
	assert(&logLine != this);

	m_priority = logLine.m_priority;
	m_hasNonTriviallyCopyable = logLine.m_hasNonTriviallyCopyable;
	m_timestamp = logLine.m_timestamp;
	m_file = logLine.m_file;
	m_function = logLine.m_function;
	m_message = logLine.m_message;
	m_threadId = logLine.m_threadId;
	m_line = logLine.m_line;
	m_used = logLine.m_used;
	m_size = logLine.m_size;
	if (logLine.m_heapBuffer) {
		m_heapBuffer = std::make_unique<std::byte[]>(m_used);
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_heapBuffer.get(), m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), logLine.m_heapBuffer.get(), m_used);
		}
	} else {
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_used);
		}
	}
	return *this;
}

LogLine& LogLine::operator=(LogLine&& logLine) noexcept {
	m_priority = logLine.m_priority;
	m_hasNonTriviallyCopyable = logLine.m_hasNonTriviallyCopyable;
	m_timestamp = logLine.m_timestamp;
	m_file = logLine.m_file;
	m_function = logLine.m_function;
	m_message = logLine.m_message;
	m_threadId = logLine.m_threadId;
	m_line = logLine.m_line;
	m_used = logLine.m_used;
	m_size = logLine.m_size;
	m_heapBuffer = std::move(logLine.m_heapBuffer);
	if (!m_heapBuffer) {
		if (m_hasNonTriviallyCopyable) {
			MoveObjects(logLine.m_stackBuffer, m_stackBuffer, m_used);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_used);
		}
	}
	// leave source in a consistent state
	logLine.m_used = 0;
	logLine.m_size = sizeof(m_stackBuffer);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const bool arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const bool* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogLine& LogLine::operator<<(const char arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogLine& LogLine::operator<<(const signed char arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const signed char* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned char arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const unsigned char* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed short arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const signed short* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned short arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const unsigned short* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed int arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const signed int* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned int arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const unsigned int* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed long arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const signed long* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned long arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const unsigned long* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed long long arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const signed long long* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned long long arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const unsigned long long* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const float arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const float* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const double arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const double* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const long double arg) {
	Write(arg);
	return *this;
}

LogLine& LogLine::operator<<(const long double* const arg) {
	WritePointer(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_ const void* __restrict const arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_ std::nullptr_t /* arg */) {
	Write<const void*>(nullptr);
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const char* __restrict const arg) {
	if (arg) {
		WriteString(arg, std::strlen(arg));
	} else {
		WriteNullPointer();
	}
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const wchar_t* __restrict const arg) {
	if (arg) {
		WriteString(arg, std::wcslen(arg));
	} else {
		WriteNullPointer();
	}
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::string& arg) {
	WriteString(arg.c_str(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::wstring& arg) {
	WriteString(arg.c_str(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::string_view& arg) {
	WriteString(arg.data(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::wstring_view& arg) {
	WriteString(arg.data(), arg.length());
	return *this;
}

LogLine& LogLine::operator<<(const std::exception& arg) {
	const BaseException* const pBaseException = GetCurrentExceptionAsBaseException();
	const std::error_code* const pErrorCode = GetCurrentExceptionCode();

	// do not evaluate arg.what() when a BaseException exists
	WriteException(pBaseException && pBaseException->m_logLine.GetPattern() ? nullptr : arg.what(), pBaseException, pErrorCode);
	return *this;
}

/// @brief The single specialization of `CopyArgumentsTo`.
/// @param args The `std::vector` to receive the message arguments.
template <>
void LogLine::CopyArgumentsTo<std::vector<fmt::format_context::format_arg>>(std::vector<fmt::format_context::format_arg>& args) const {
	CopyArgumentsFromBufferTo(GetBuffer(), m_used, args);
}

std::string LogLine::GetLogMessage() const {
	std::vector<fmt::format_context::format_arg> args;
	CopyArgumentsFromBufferTo(GetBuffer(), m_used, args);

	constexpr std::size_t kDefaultBufferSize = 256;
	fmt::basic_memory_buffer<char, kDefaultBufferSize> buf;
	fmt::vformat_to(buf, fmt::to_string_view(GetPattern()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	return fmt::to_string(buf);
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogLine::GetBuffer() noexcept {
	return !m_heapBuffer ? m_stackBuffer : m_heapBuffer.get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) const std::byte* LogLine::GetBuffer() const noexcept {
	return !m_heapBuffer ? m_stackBuffer : m_heapBuffer.get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogLine::GetWritePosition(const LogLine::Size additionalBytes) {
	const std::size_t requiredSize = static_cast<std::size_t>(m_used) + additionalBytes;
	if (requiredSize > std::numeric_limits<LogLine::Size>::max()) {
		LLAMALOG_THROW(std::length_error(nullptr), "Buffer too big for {} more bytes: {}", additionalBytes, requiredSize);
	}

	if (requiredSize <= m_size) {
		return !m_heapBuffer ? &m_stackBuffer[m_used] : &(m_heapBuffer.get())[m_used];
	}

	m_size = GetNextChunk(static_cast<std::uint32_t>(requiredSize));
	if (!m_heapBuffer) {
		m_heapBuffer = std::make_unique<std::byte[]>(m_size);

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			MoveObjects(m_stackBuffer, m_heapBuffer.get(), m_used);
		} else {
			std::memcpy(m_heapBuffer.get(), m_stackBuffer, m_used);
		}
	} else {
		std::unique_ptr<std::byte[]> newHeapBuffer(std::make_unique<std::byte[]>(m_size));

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(newHeapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			MoveObjects(m_heapBuffer.get(), newHeapBuffer.get(), m_used);
		} else {
			std::memcpy(newHeapBuffer.get(), m_heapBuffer.get(), m_used);
		}
		m_heapBuffer = std::move(newHeapBuffer);
	}
	return &(m_heapBuffer.get())[m_used];
}

// Derived from both methods `NanoLogLine::encode` from NanoLog.
template <typename T>
void LogLine::Write(const T arg) {
	const TypeId typeId = GetTypeId<T>(m_escape);
	constexpr auto kArgSize = kTypeSize<T>;
	std::byte* __restrict const buffer = GetWritePosition(kArgSize);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &arg, sizeof(arg));

	m_used += kArgSize;
}

template <typename T>
void LogLine::WritePointer(const T* const arg) {
	if (arg) {
		const TypeId typeId = GetTypeId<T, true>(m_escape);
		constexpr auto kArgSize = kTypeSize<T>;

		std::byte* __restrict buffer = GetWritePosition(kArgSize);
		const LogLine::Align padding = GetPadding<T>(&buffer[sizeof(typeId)]);
		if (padding) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(kArgSize + padding);
		}

		std::memcpy(buffer, &typeId, sizeof(typeId));
		std::memcpy(&buffer[sizeof(typeId)] + padding, arg, sizeof(*arg));

		m_used += kArgSize + padding;
	} else {
		WriteNullPointer();
	}
}

void LogLine::WriteNullPointer() {
	const TypeId typeId = GetTypeId<NullValue>(m_escape);
	constexpr auto kArgSize = kTypeSize<NullValue>;
	std::byte* __restrict const buffer = GetWritePosition(kArgSize);

	std::memcpy(buffer, &typeId, sizeof(typeId));

	m_used += kArgSize;
}

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
void LogLine::WriteString(_In_reads_(len) const char* __restrict const arg, const std::size_t len) {
	const TypeId typeId = GetTypeId<const char*>(m_escape);
	constexpr auto kArgSize = kTypeSize<const char*>;
	const LogLine::Length length = static_cast<LogLine::Length>(std::min<std::size_t>(len, std::numeric_limits<LogLine::Length>::max()));
	if (length < len) {
		LLAMALOG_INTERNAL_WARN("String of length {} trimmed to {}", len, length);
	}
	const LogLine::Size size = kArgSize + length * sizeof(char);

	std::byte* __restrict buffer = GetWritePosition(size);
	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize], arg, length * sizeof(char));

	m_used += size;
}

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
void LogLine::WriteString(_In_reads_(len) const wchar_t* __restrict const arg, const std::size_t len) {
	const TypeId typeId = GetTypeId<const wchar_t*>(m_escape);
	constexpr auto kArgSize = kTypeSize<const wchar_t*>;
	const LogLine::Length length = static_cast<LogLine::Length>(std::min<std::size_t>(len, std::numeric_limits<LogLine::Length>::max()));
	if (length < len) {
		LLAMALOG_INTERNAL_WARN("String of length {} trimmed to {}", len, length);
	}
	const LogLine::Size size = kArgSize + length * sizeof(wchar_t);

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[kArgSize]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize + padding], arg, length * sizeof(wchar_t));

	m_used += size + padding;
}

void LogLine::WriteException(_In_opt_z_ const char* message, _In_opt_ const BaseException* pBaseException, _In_opt_ const std::error_code* const pCode) {
	const std::size_t messageLen = message ? std::strlen(message) : 0;
	// silently trim message to size
	const LogLine::Length messageLength = static_cast<LogLine::Length>(std::min<std::size_t>(messageLen, std::numeric_limits<LogLine::Length>::max()));
	if (messageLength < messageLen) {
		LLAMALOG_INTERNAL_WARN("Exception message of length {} trimmed to {}", messageLen, messageLength);
	}
	static_assert(alignof(char) == 1, "alignment of char");  // no padding required for message

	if (!pBaseException) {
		const TypeId typeId = pCode ? GetTypeId<PlainSystemError>(m_escape) : GetTypeId<PlainException>(m_escape);
		const auto kArgSize = pCode ? kTypeSize<PlainSystemError> : kTypeSize<PlainException>;

		const LogLine::Size size = kArgSize + messageLength * sizeof(char);
		std::byte* __restrict const buffer = GetWritePosition(size);

		std::memcpy(buffer, &typeId, sizeof(typeId));

		static_assert(offsetof(PlainException, length) == offsetof(PlainSystemError, length));
		std::memcpy(&buffer[sizeof(typeId) + offsetof(PlainException, length)], &messageLength, sizeof(messageLength));
		if (message) {
			std::memcpy(&buffer[kArgSize], message, messageLength);
		}

		if (pCode) {
			const int code = pCode->value();
			std::memcpy(&buffer[sizeof(typeId) + offsetof(PlainSystemError, code)], &code, sizeof(code));
			const std::error_category* const pCategory = &pCode->category();
			// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes
			std::memcpy(&buffer[sizeof(typeId) + offsetof(PlainSystemError, pCategory)], &pCategory, sizeof(pCategory));
		}

		m_used += kArgSize + messageLength;
		return;
	}

	const LogLine& logLine = pBaseException->m_logLine;
	assert(!logLine.m_message ^ !message);  // either pattern or message must be present but not both
	if (!logLine.m_heapBuffer) {
		const TypeId typeId = pCode ? GetTypeId<StackBasedSystemError>(m_escape) : GetTypeId<StackBasedException>(m_escape);
		const auto kArgSize = pCode ? kTypeSize<StackBasedSystemError> : kTypeSize<StackBasedException>;
		const LogLine::Size size = kArgSize + messageLength * sizeof(char) + logLine.m_used;

		std::byte* __restrict buffer = GetWritePosition(size);
		const LogLine::Size pos = sizeof(typeId);
		const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
		const LogLine::Size offset = pos + padding;
		if (padding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding);
		}

		const LogLine::Size messageOffset = offset + offsetof(ExceptionInformation /* StackBasedException */, padding);
		const LogLine::Size bufferPos = messageOffset + messageLength * sizeof(char);
		const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);
		if (bufferPadding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding + bufferPadding);
		}
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		std::memcpy(buffer, &typeId, sizeof(typeId));
		std::memcpy(&buffer[offset], &logLine.m_timestamp, offsetof(ExceptionInformation /* StackBasedException */, length));  // copy up to used
		std::memcpy(&buffer[offset + offsetof(ExceptionInformation /* StackBasedException */, length)], &messageLength, sizeof(messageLength));
		std::memcpy(&buffer[offset + offsetof(ExceptionInformation /* StackBasedException */, hasNonTriviallyCopyable)], &logLine.m_hasNonTriviallyCopyable, sizeof(bool));
		if (message) {
			std::memcpy(&buffer[messageOffset], message, messageLength);
		}

		if (logLine.m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.GetBuffer(), &buffer[bufferOffset], logLine.m_used);
		} else {
			std::memcpy(&buffer[bufferOffset], logLine.GetBuffer(), logLine.m_used);
		}

		const LogLine::Size nextOffset = bufferOffset + logLine.m_used;
		assert(nextOffset + (pCode ? static_cast<LogLine::Size>(sizeof(StackBasedSystemError)) : 0) == size + padding + bufferPadding);
		if (pCode) {
			const int code = pCode->value();
			std::memcpy(&buffer[nextOffset + offsetof(StackBasedSystemError, code)], &code, sizeof(code));
			const std::error_category* const pCategory = &pCode->category();
			// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.
			std::memcpy(&buffer[nextOffset + offsetof(StackBasedSystemError, pCategory)], &pCategory, sizeof(pCategory));
			m_used += nextOffset + sizeof(StackBasedSystemError);
		} else {
			m_used += nextOffset;
		}
	} else {
		const TypeId typeId = pCode ? GetTypeId<HeapBasedSystemError>(m_escape) : GetTypeId<HeapBasedException>(m_escape);
		const auto kArgSize = pCode ? kTypeSize<HeapBasedSystemError> : kTypeSize<HeapBasedException>;
		const LogLine::Size size = kArgSize + messageLength * sizeof(char);

		std::byte* __restrict buffer = GetWritePosition(size);
		const LogLine::Size pos = sizeof(typeId);
		const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
		const LogLine::Size offset = pos + padding;
		if (padding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding);
		}

		const LogLine::Size messageOffset = offset + sizeof(HeapBasedException);

		std::memcpy(buffer, &typeId, sizeof(typeId));
		std::memcpy(&buffer[offset], &logLine.m_timestamp, offsetof(ExceptionInformation /* StackBasedException */, length));  // copy up to used
		std::memcpy(&buffer[offset + offsetof(ExceptionInformation /* StackBasedException */, length)], &messageLength, sizeof(messageLength));
		std::memcpy(&buffer[offset + offsetof(ExceptionInformation /* StackBasedException */, hasNonTriviallyCopyable)], &logLine.m_hasNonTriviallyCopyable, sizeof(bool));
		if (message) {
			std::memcpy(&buffer[messageOffset], message, messageLength);
		}

		std::unique_ptr<std::byte[]> heapBuffer = std::make_unique<std::byte[]>(logLine.m_used);
		std::byte* const pBuffer = heapBuffer.get();
		std::memcpy(&buffer[offset + offsetof(HeapBasedException, pHeapBuffer)], &pBuffer, sizeof(pBuffer));
		if (logLine.m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.GetBuffer(), pBuffer, logLine.m_used);
		} else {
			std::memcpy(pBuffer, logLine.GetBuffer(), logLine.m_used);
		}
		heapBuffer.release();

		const LogLine::Size nextOffset = messageOffset + messageLength * sizeof(char);
		assert(nextOffset + (pCode ? static_cast<LogLine::Size>(sizeof(HeapBasedSystemError)) : 0) == size + padding);
		if (pCode) {
			const int code = pCode->value();
			std::memcpy(&buffer[nextOffset + offsetof(HeapBasedSystemError, code)], &code, sizeof(code));
			const std::error_category* const pCategory = &pCode->category();
			// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.
			std::memcpy(&buffer[nextOffset + offsetof(HeapBasedSystemError, pCategory)], &pCategory, sizeof(pCategory));
			m_used += nextOffset + sizeof(HeapBasedSystemError);
		} else {
			m_used += nextOffset;
		}
	}
	m_hasNonTriviallyCopyable = true;
}

void LogLine::WriteTriviallyCopyable(_In_reads_bytes_(objectSize) const std::byte* __restrict const ptr, const LogLine::Size objectSize, const LogLine::Align align, _In_ void (*const createFormatArg)()) {
	static_assert(sizeof(createFormatArg) == sizeof(internal::FunctionTable::CreateFormatArg));

	const TypeId typeId = GetTypeId<TriviallyCopyable>(m_escape);
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;
	const LogLine::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding != 0) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding)], &createFormatArg, sizeof(createFormatArg));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding) + sizeof(createFormatArg)], &objectSize, sizeof(objectSize));
	std::memcpy(&buffer[kArgSize + padding], ptr, objectSize);

	m_used += size + padding;
}

__declspec(restrict) std::byte* LogLine::WriteNonTriviallyCopyable(const LogLine::Size objectSize, const LogLine::Align align, _In_ const void* const functionTable) {
	static_assert(sizeof(functionTable) == sizeof(internal::FunctionTable*));

	const TypeId typeId = GetTypeId<NonTriviallyCopyable>(m_escape);
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;
	const LogLine::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding != 0) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_size - m_used >= size + padding);

	std::memcpy(buffer, &typeId, sizeof(typeId));
	std::memcpy(&buffer[sizeof(typeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding)], &functionTable, sizeof(functionTable));
	std::memcpy(&buffer[sizeof(typeId) + sizeof(padding) + sizeof(functionTable)], &objectSize, sizeof(objectSize));
	std::byte* result = &buffer[kArgSize + padding];

	m_hasNonTriviallyCopyable = true;
	m_used += size + padding;

	return result;
}

}  // namespace llamalog

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const std::align_val_t arg) {
	static_assert(
		(std::is_signed_v<std::underlying_type_t<std::align_val_t>> && (sizeof(std::align_val_t) == sizeof(std::int64_t) || sizeof(std::align_val_t) == sizeof(std::int32_t)))
			|| (!std::is_signed_v<std::underlying_type_t<std::align_val_t>> && (sizeof(std::align_val_t) == sizeof(std::uint64_t) || sizeof(std::align_val_t) == sizeof(std::uint32_t))),
		"cannot match type of std::align_val_t");

	if constexpr (std::is_signed_v<std::underlying_type_t<std::align_val_t>>) {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::int64_t)) {
			return logLine << static_cast<std::int64_t>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::int32_t)) {
			return logLine << static_cast<std::int32_t>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	} else {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::uint64_t)) {
			return logLine << static_cast<std::uint64_t>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::uint32_t)) {
			return logLine << static_cast<std::uint32_t>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	}
}

/// @brief Operator for printing pointers to std::align_val_t values.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The argument.
/// @return The @p logLine for method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const std::align_val_t* const arg) {
	static_assert(
		(std::is_signed_v<std::underlying_type_t<std::align_val_t>> && ((sizeof(std::align_val_t) == sizeof(std::int64_t) && alignof(std::align_val_t) == alignof(std::int64_t))                 // NOLINT(misc-redundant-expression): Paranoid static-assert.
																		|| (sizeof(std::align_val_t) == sizeof(std::int32_t) && alignof(std::align_val_t) == alignof(std::int32_t)))             // NOLINT(misc-redundant-expression): Paranoid static-assert.
		 ) || (!std::is_signed_v<std::underlying_type_t<std::align_val_t>> && ((sizeof(std::align_val_t) == sizeof(std::uint64_t) && alignof(std::align_val_t) == alignof(std::uint64_t))        // NOLINT(misc-redundant-expression): Paranoid static-assert.
																			   || (sizeof(std::align_val_t) == sizeof(std::uint32_t) && alignof(std::align_val_t) == alignof(std::uint32_t)))),  // NOLINT(misc-redundant-expression): Paranoid static-assert.
		"cannot match type of std::align_val_t");

	if constexpr (std::is_signed_v<std::underlying_type_t<std::align_val_t>>) {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::int64_t)) {
			return logLine << reinterpret_cast<const std::int64_t*>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::int32_t)) {
			return logLine << reinterpret_cast<const std::int32_t*>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	} else {
		if constexpr (sizeof(std::align_val_t) == sizeof(std::uint64_t)) {
			return logLine << reinterpret_cast<const std::uint64_t*>(arg);
		} else if constexpr (sizeof(std::align_val_t) == sizeof(std::uint32_t)) {
			return logLine << reinterpret_cast<const std::uint32_t*>(arg);
		} else {
			assert(false);
			__assume(false);
		}
	}
}
