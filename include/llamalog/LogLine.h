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


#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace llamalog {

/// @brief Enum for different log levels.
/// @copyright This enum is based on `enum class LogLevel` from NanoLog.
enum class LogLevel : std::uint8_t {
	kTrace,  ///< Output useful for inspecting program flow.
	kDebug,  ///< Output useful for debugging.
	kInfo,   ///< Informational message which should be logged.
	kWarn,   ///< A condition which should be inspected.
	kError,  ///< A recoverable error.
	kFatal   ///< A condition leading to program abort.
};

/// @brief The class contains all data for formatting and output which happens asynchronously.
/// @copyright The interface of this class is based on `class NanoLogLine` from NanoLog.
class LogLine final {
public:
	/// @brief Create a new target for the various `operator<<` overloads.
	/// @details The timestamp is not set until `#GenerateTimestamp` is called.
	/// @param logLevel The `#LogLevel`.
	/// @param szFile The logged file name. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	/// @param szFunction The logged function. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	/// @param line The logged line number.
	/// @param szMessage The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	LogLine(LogLevel logLevel, _In_z_ const char* __restrict szFile, _In_z_ const char* __restrict szFunction, std::uint32_t line, _In_z_ const char* __restrict szMessage) noexcept;

	LogLine(const LogLine&) = delete;       ///< @nocopyconstructor
	LogLine(LogLine&&) noexcept = default;  ///< @defaultconstructor
	~LogLine() noexcept = default;

public:
	LogLine& operator=(const LogLine&) = delete;       ///< @noassignmentoperator
	LogLine& operator=(LogLine&&) noexcept = default;  ///< @defaultoperator

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(bool arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogLine& operator<<(signed char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogLine& operator<<(unsigned char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
	LogLine& operator<<(signed long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(unsigned long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(float arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(double arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(long double arg);

	/// @brief Add an address as a log argument. @note Please note that one MUST NOT use this pointer to access	an
	/// object because at the time of logging, the object might no longer exist.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(_In_opt_ const void* arg);

	/// @brief Add a `nullptr`  as a log argument. @details The output is the same as providing a `void` pointer having
	/// the value `nullptr`.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(_In_opt_ std::nullptr_t arg);

	/// @brief Add a log argument. @details The value is copied into the buffer.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(_In_opt_z_ const char* arg);

	/// @brief Add a log argument. @details The value is copied into the buffer.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(_In_opt_z_ const wchar_t* arg);

	/// @brief Add a log argument. @details The value is copied into the buffer.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::string_view& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::wstring_view& arg);

public:
	/// @brief Get the timestamp for the log event.
	/// @return The timestamp.
	const FILETIME& GetTimestamp() const noexcept {
		return m_timestamp;
	}
	/// @brief Generate the timestamp for the log event.
	/// @note Not part of the constructor to support setting it from the logger queue.
	void GenerateTimestamp() noexcept {
		GetSystemTimeAsFileTime(&m_timestamp);
	}

	/// @brief Get the log level.
	/// @return The log level.
	LogLevel GetLevel() const noexcept {
		return m_logLevel;
	}

	/// @brief Get the thread if.
	/// @return The thread id.
	DWORD GetThreadId() const noexcept {
		return m_threadId;
	}

	/// @brief Get the name of the file.
	/// @return The file name.
	const char* GetFile() const noexcept {
		return m_szFile;
	}

	/// @brief Get the name of the function.
	/// @return The function name.
	const char* GetFunction() const noexcept {
		return m_szFunction;
	}

	/// @brief Get the source code line.
	/// @return The line number.
	uint32_t GetLine() const noexcept {
		return m_line;
	}

	/// @brief Returns the formatted log message.
	/// @return The log message.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	std::string GetMessage() const;

private:
	/// @brief Get the argument buffer.
	/// @return The start of the buffer.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	_Ret_notnull_ __declspec(restrict) const std::byte* GetBuffer() const noexcept;

	/// @brief Get the current position in the argument buffer ensuring that enough space exists for the next argument.
	/// @param cbAdditionalBytes The number of bytes that will be appended.
	/// @return The next write position.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	_Ret_notnull_ __declspec(restrict) std::byte* GetWritePosition(std::size_t cbAdditionalBytes);

	/// @brief Copy an argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	/// @copyright Derived from both methods `NanoLogLine::encode` from NanoLog.
	template <typename T>
	void Write(T arg);

	/// @brief Copy a string to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the string in characters (NOT
	/// including a terminating null character), optional padding as required and finally the string's characters (again
	/// NOT including a terminating null character).
	/// @param arg The string to add.
	/// @param cchLength The string length in characters NOT including a terminating null character.
	/// @copyright Derived from `NanoLogLine::encode_c_string` from NanoLog.
	template <typename T, typename std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, wchar_t>, int> = 0>
	void WriteString(_In_z_ const T* arg, std::size_t cchLength);

	/// @brief Add a custom object to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the padding for @p T, the
	/// pointer to the function to create the formatter	argument, the size of the data, padding as required and finally
	/// the bytes of @p ptr.
	/// @remark The function to create the formatter argument is supplied as a void pointer which removes the compile
	/// time dependency to {fmt} from this header.
	/// @param ptr A pointer to the argument data.
	/// @param cbObjectSize The size of the object.
	/// @param cbAlign The alignment requirement of the type.
	/// @param createFormatArg A pointer to a function which has a single argument of type `std::byte*` and returns a
	/// newly created `fmt::basic_format_arg` object.
	void WriteCustomArgument(_In_reads_bytes_(cbSize) const std::byte* __restrict ptr, std::size_t cbObjectSize, std::size_t cbAlign, const void* createFormatArg);

public:
	/// @brief Copy a log argument of a custom type to the argument buffer.
	/// @remark Include `CustomArgument.h` in your implementation file before calling this function.
	/// @tparam T The type of the argument. This type MUST be copyable using `std::memcpy`.
	/// @param arg The object.
	template <typename T>
	void AddCustomArgument(const T& arg);

private:
	/// @copyright Same as `NanoLogLine::m_bytes_used` from NanoLog. @hideinitializer
	std::size_t m_cbUsed = 0;  ///< @brief The number of bytes used in the buffer.

	/// @copyright Same as `NanoLogLine::m_buffer_size` from NanoLog. @hideinitializer
	std::size_t m_cbSize = sizeof(m_stackBuffer);  ///< @brief The current capacity of the buffer in bytes.

	/// @copyright Same as `NanoLogLine::m_heap_buffer` from NanoLog.
	std::unique_ptr<std::byte[]> m_heapBuffer;  ///< The buffer on the heap if the stack buffer became too small.

	/// @brief The stack buffer used for small payloads.
	/// @copyright Same as `NanoLogLine::m_stack_buffer` from NanoLog.
	std::byte m_stackBuffer[256
							- sizeof(std::size_t) * 2
							- sizeof(std::unique_ptr<std::byte[]>)
							- sizeof(LogLevel)
							- sizeof(DWORD)
							- sizeof(std::uint32_t)
							- sizeof(FILETIME)
							- sizeof(const char*) * 3];

	// The following fields start with the smallest aligning requirement to optimize the size of m_stackBuffer.
	LogLevel m_logLevel;   ///< @brief The entry's log level.
	DWORD m_threadId;      ///< @brief The id of the thread which created the log entry.
	std::uint32_t m_line;  ///< @brief The line number in the source file.
	FILETIME m_timestamp;  ///< @brief The timestamp at which this entry had been created.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_szFile;  ///< @brief The source file of the log statement creating this entry.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_szFunction;  ///< @brief The name of the function creating this entry.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_szMessage;  ///< @brief The log message.
};

}  // namespace llamalog
