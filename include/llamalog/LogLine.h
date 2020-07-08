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
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifndef LLAMALOG_LOGLINE_SIZE
/// @brief The size of a log line in bytes.
/// @details Defined as a macro to allow redefinition for different use cases.
/// @note The value MUST be a power of 2, else compilation will fail.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Use macro to allow override in build settings.
#define LLAMALOG_LOGLINE_SIZE 256
#endif

namespace llamalog {

class BaseException;

/// @brief Enum for different log priorities.
/// @note Priorities MUST be divisible by 4 because the values `| 1`, `| 2` and `| 3` are reserved for internal use.
/// @copyright This enum is based on `enum class LogLevel` from NanoLog.
enum class Priority : std::uint8_t {
	kNone = 0,    ///< Value for "not set"
	kTrace = 4,   ///< Output useful for inspecting program flow.
	kDebug = 8,   ///< Output useful for debugging.
	kInfo = 16,   ///< Informational message which should be logged.
	kWarn = 32,   ///< A condition which should be inspected.
	kError = 64,  ///< A recoverable error.
	kFatal = 128  ///< A condition leading to program abort.
};

/// @brief Wrap a value in this type to get its output escaped according to C rules. Currently, only low ASCII
/// characters are replaced by their named or numeric counterparts, i.e. 0x0A becomes \\n, 0x11 becomes \\x11.
/// @tparam T The type of the argument.
template <typename T>
struct escape final {  // NOLINT(readability-identifier-naming): Infrastructure is less prominent in lower case.
	/// @brief Create a new wrapper for a parameter.
	/// @param val The parameter value.
	explicit escape(const T& val) noexcept
		: value(val) {
		// empty
	}
	escape(const escape&) = delete;
	escape(escape&&) = delete;
	~escape() noexcept = default;

	escape& operator=(const escape&) = delete;
	escape& operator=(escape&&) = delete;

	const T& value;  ///< @brief The parameter value.
};

/// @brief The class contains all data for formatting and output which happens asynchronously.
/// @details @internal The stack buffer is allocated in the base class for a better memory layout.
/// @copyright The interface of this class is based on `class NanoLogLine` from NanoLog.
class alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) LogLine final {
public:
	/// @brief Create a new target for the various `operator<<` overloads.
	/// @details The timestamp is not set until `#GenerateTimestamp` is called.
	/// @param priority The `#Priority`.
	/// @param file The logged file name. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	/// @param line The logged line number.
	/// @param function The logged function. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	/// @param message The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
	LogLine(Priority priority, _In_z_ const char* __restrict file, std::uint32_t line, _In_z_ const char* __restrict function, _In_opt_z_ const char* __restrict message) noexcept;

	/// @brief Copy the buffers. @details The copy constructor is required for `std::curent_exception`.
	/// @param logLine The source log line.
	LogLine(const LogLine& logLine);

	/// @brief Move the buffers.
	/// @param logLine The source log line.
	LogLine(LogLine&& logLine) noexcept;

	/// @brief Calls the destructor of every custom argument.
	~LogLine() noexcept;

public:
	/// @brief Copy the buffers. @details The assignment operator is required for `std::exception`.
	/// @param logLine The source log line.
	/// @return This object.
	LogLine& operator=(const LogLine& logLine);

	/// @brief Move the buffers.
	/// @param logLine The source log line.
	/// @return This object.
	LogLine& operator=(LogLine&& logLine) noexcept;

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(bool arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const bool* arg);

	/// @brief Add a log argument. @note A `char` is distinct from both `signed char` and `unsigned char`.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogLine& operator<<(char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogLine& operator<<(signed char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const signed char* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(char)` from NanoLog.
	LogLine& operator<<(unsigned char arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const unsigned char* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const signed short* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned short arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const unsigned short* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const signed int* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned int arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const unsigned int* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
	LogLine& operator<<(signed long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const signed long* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
	LogLine& operator<<(unsigned long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const unsigned long* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
	LogLine& operator<<(signed long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const signed long long* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(unsigned long long arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const unsigned long long* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(float arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const float* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(double arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const double* arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(double)` from NanoLog.
	LogLine& operator<<(long double arg);

	/// @brief Add a log argument.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const long double* arg);

	/// @brief Add an address as a log argument. @note Please note that one MUST NOT use this pointer to access	an
	/// object because at the time of logging, the object might no longer exist.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(_In_opt_ const void* __restrict arg);

	/// @brief Add a `nullptr`  as a log argument. @details The output is the same as providing a `void` pointer having
	/// the value `nullptr`.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
	LogLine& operator<<(_In_opt_ std::nullptr_t /* arg */);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(_In_opt_z_ const char* __restrict arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(_In_opt_z_ const wchar_t* __restrict arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::string& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::wstring& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::string_view& arg);

	/// @brief Add a log argument. @details The value is copied into the buffer. A maximum of 2^16 characters is printed.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	/// @copyright Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
	LogLine& operator<<(const std::wstring_view& arg);

	/// @brief Add a `std::exception` as an argument.
	/// @details If the `std::exception` is of type `std::system_error` the additional details are made available for formatting.
	/// @note The function MUST be called from within a catch block.
	/// @param arg The argument.
	/// @return The current object for method chaining.
	LogLine& operator<<(const std::exception& arg);

	/// @brief Request output escaping for a parameter.
	/// @tparam T The type of the actual parameter.
	/// @param arg The parameter wrapped in a `escape` struct.
	/// @return The current object for method chaining.
	template <typename T>
	LogLine& operator<<(const escape<T>& arg) {
		m_escape = true;
		*this << arg.value;
		m_escape = false;
		return *this;
	}

public:
	/// @brief Get the timestamp for the log event.
	/// @return The timestamp.
	[[nodiscard]] const FILETIME& GetTimestamp() const noexcept {
		return m_timestamp;
	}

	/// @brief Generate the timestamp for the log event.
	/// @note Not part of the constructor to support setting it from the logger queue.
	void GenerateTimestamp() noexcept {
		GetSystemTimeAsFileTime(&m_timestamp);
	}

	/// @brief Get the priority.
	/// @return The priority.
	[[nodiscard]] Priority GetPriority() const noexcept {
		return m_priority;
	}

	/// @brief Get the thread if.
	/// @return The thread id.
	[[nodiscard]] DWORD GetThreadId() const noexcept {
		return m_threadId;
	}

	/// @brief Get the name of the file.
	/// @return The file name.
	[[nodiscard]] _Ret_z_ const char* GetFile() const noexcept {
		return m_file;
	}

	/// @brief Get the source code line.
	/// @return The line number.
	[[nodiscard]] std::uint32_t GetLine() const noexcept {
		return m_line;
	}

	/// @brief Get the name of the function.
	/// @return The function name.
	[[nodiscard]] _Ret_z_ const char* GetFunction() const noexcept {
		return m_function;
	}

	/// @brief Get the unformatted log message, i.e. before patter replacement.
	/// @return The message pattern.
	[[nodiscard]] _Ret_z_ const char* GetPattern() const noexcept {
		return m_message;
	}

	/// @brief Get the arguments for formatting the message.
	/// @remarks Using a template removes the need to include the headers of {fmt} in all translation units.
	/// MUST use argument for `std::vector` because guaranteed copy elision does not take place in debug builds.
	/// @tparam T This MUST be `std::vector<fmt::format_context::format_arg>`.
	/// @param args The `std::vector` to receive the message arguments.
	template <typename T>
	void CopyArgumentsTo(T& args) const;

	/// @brief Returns the formatted log message. @note The name `GetMessage` would conflict with the function from the
	/// Windows API having the same name.
	/// @return The log message.
	[[nodiscard]] std::string GetLogMessage() const;

	/// @brief Copy a log argument of a custom type to the argument buffer.
	/// @details This function handles types which are trivially copyable.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument. This type MUST have a copy constructor.
	/// @param arg The object.
	/// @return The current object for method chaining.
	template <typename T, bool kEscaped = false, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
	LogLine& AddCustomArgument(const T& arg);

	/// @brief Copy a log argument of a pointer to a custom type to the argument buffer.
	/// @details This function handles types which are trivially copyable.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument. This type MUST have a copy constructor.
	/// @param arg The pointer to an object.
	/// @return The current object for method chaining.
	template <typename T, bool kEscaped = false, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
	LogLine& AddCustomArgument(const T* arg);

	/// @brief Copy a log argument of a custom type to the argument buffer.
	/// @details This function handles types which are not trivially copyable. However, the type MUST support copy construction.
	/// @note The type @p T MUST be both copy constructible and either nothrow move constructible or nothrow copy constructible.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument.
	/// @param arg The object.
	/// @return The current object for method chaining.
	template <typename T, bool kEscaped = false, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
	LogLine& AddCustomArgument(const T& arg);

	/// @brief Copy a log argument of a pointer to a custom type to the argument buffer.
	/// @details This function handles types which are not trivially copyable. However, the type MUST support copy construction.
	/// @note The type @p T MUST be both copy constructible and either nothrow move constructible or nothrow copy constructible.
	/// @remark Include `<llamalog/custom_types.h>` in your implementation file before calling this function.
	/// @tparam T The type of the argument.
	/// @param arg The pointer to an object.
	/// @return The current object for method chaining.
	template <typename T, bool kEscaped = false, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int> = 0>
	LogLine& AddCustomArgument(const T* arg);

public:
	using Size = std::uint32_t;    ///< @brief A data type for indexes in the buffer representing *bytes*.
	using Length = std::uint16_t;  ///< @brief The length of a string in number of *characters*.
	using Align = std::uint8_t;    ///<@ brief Alignment requirement of a data type in *bytes*.

private:
	/// @brief Get the argument buffer for writing.
	/// @return The start of the buffer.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) std::byte* GetBuffer() noexcept;

	/// @brief Get the argument buffer.
	/// @return The start of the buffer.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) const std::byte* GetBuffer() const noexcept;

	/// @brief Get the current position in the argument buffer ensuring that enough space exists for the next argument.
	/// @param additionalBytes The number of bytes that will be appended.
	/// @return The next write position.
	/// @copyright Derived from `NanoLogLine::buffer` from NanoLog.
	[[nodiscard]] _Ret_notnull_ __declspec(restrict) std::byte* GetWritePosition(Size additionalBytes);

	/// @brief Copy an argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	/// @copyright Derived from both methods `NanoLogLine::encode` from NanoLog.
	template <typename T>
	void Write(T arg);

	/// @brief Copy a pointer argument to the buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the bytes of the value.
	/// If @p arg is the `nullptr`, a special NullValue argument is added.
	/// @tparam T The type of the argument. The type MUST be copyable using `std::memcpy`.
	/// @param arg The value to add.
	template <typename T>
	void WritePointer(const T* arg);

	/// @brief Copy a nullptr argument to the buffer.
	/// @remarks This function is used internally by `#WritePointer` and `#AddCustomArgument`.
	void WriteNullPointer();

	/// @brief Copy a string to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the string in characters (NOT
	/// including a terminating null character) and finally the string's characters (again NOT including a terminating null character).
	/// This function is optimized compared to `WriteString(const wchar_t*, std::size_t)` because `char` has no alignment requirements.
	/// @remarks The type of @p len is `std::size_t` because the check if the length exceeds the capacity of `#Length` happens inside this function.
	/// @param arg The string to add.
	/// @param len The string length in characters NOT including a terminating null character.
	/// @copyright Derived from `NanoLogLine::encode_c_string` from NanoLog.
	void WriteString(_In_reads_(len) const char* __restrict arg, std::size_t len);

	/// @brief Copy a string to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the string in characters (NOT
	/// including a terminating null character), optional padding as required and finally the string's characters (again
	/// NOT including a terminating null character).
	/// @remarks The type of @p len is `std::size_t` because the check if the length exceeds the capacity of `#Length` happens inside this function.
	/// @param arg The string to add.
	/// @param len The string length in characters NOT including a terminating null character.
	/// @copyright Derived from `NanoLogLine::encode_c_string` from NanoLog.
	void WriteString(_In_reads_(len) const wchar_t* __restrict arg, std::size_t len);

	/// @brief Add an exception object to the argument buffer.
	/// @param message The exception message.
	/// @param pBaseException The (optional) `BaseException` object carrying additional logging information.
	/// @param pCode The error code for `std::system_error`s.
	void WriteException(_In_opt_z_ const char* message, _In_opt_ const BaseException* pBaseException, _In_opt_ const std::error_code* pCode);

	/// @brief Add a custom object to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the padding for @p T, a pointer
	/// to the function to create the formatter argument, the size of the data, padding as required and finally the
	/// bytes of @p ptr.
	/// @remark The function to create the formatter argument is supplied as a void (*)() pointer which removes the compile
	/// time dependency to {fmt} from this header.
	/// @param ptr A pointer to the argument data.
	/// @param objectSize The size of the object.
	/// @param align The alignment requirement of the type.
	/// @param createFormatArg A pointer to a function which has a single argument of type `std::byte*` and returns a
	/// newly created `fmt::format_context::format_arg` object.
	void WriteTriviallyCopyable(_In_reads_bytes_(objectSize) const std::byte* __restrict ptr, Size objectSize, Align align, _In_ void (*createFormatArg)());

	/// @brief Add a custom object to the argument buffer.
	/// @details @internal The internal layout is the `TypeId` followed by the size of the padding for @p T, the pointer
	/// to the function table, the size of the data, padding as required and finally the bytes of the object.
	/// @note This function does NOT copy the object but only returns the target address.
	/// @remark @p functionTable is supplied as a void pointer to remove the compile time dependency to {fmt} from this header.
	/// @param objectSize The size of the object.
	/// @param align The alignment requirement of the type.
	/// @param functionTable A pointer to the `internal::FunctionTable`.
	/// @return An address where to copy the current argument.
	[[nodiscard]] __declspec(restrict) std::byte* WriteNonTriviallyCopyable(Size objectSize, Align align, _In_ const void* functionTable);

private:
	inline static thread_local bool m_escape = false;  ///< @brief `true` if the currently added argument should use output escaping.

private:
	/// @brief The stack buffer used for small payloads.
	/// @copyright Same as `NanoLogLine::m_stack_buffer` from NanoLog.
	std::byte m_stackBuffer[LLAMALOG_LOGLINE_SIZE                     // target size
							- sizeof(Priority)                        // m_priority
							- sizeof(bool)                            // m_hasNonTriviallyCopyable
							- sizeof(FILETIME)                        // m_timestamp
							- sizeof(const char*) * 3                 // m_file, m_function, m_message
							- sizeof(DWORD)                           // m_threadId
							- sizeof(std::uint32_t)                   // m_line
							- sizeof(Size) * 2                        // m_used, m_size
							- sizeof(std::unique_ptr<std::byte[]>)];  // m_heapBuffer

	Priority m_priority;                     ///< @brief The entry's priority.
	bool m_hasNonTriviallyCopyable = false;  ///< @brief `true` if at least one argument needs special handling on buffer operations. @hideinitializer
	FILETIME m_timestamp;                    ///< @brief The timestamp at which this entry had been created.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_file;  ///< @brief The source file of the log statement creating this entry.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_function;  ///< @brief The name of the function creating this entry.

	/// @details Only a pointer is stored, i.e. the string MUST NOT go out of scope.
	const char* __restrict m_message;  ///< @brief The log message.

	DWORD m_threadId;      ///< @brief The id of the thread which created the log entry.
	std::uint32_t m_line;  ///< @brief The line number in the source file.

	/// @copyright Same as `NanoLogLine::m_bytes_used` from NanoLog. @hideinitializer
	Size m_used = 0;  ///< @brief The number of bytes used in the buffer.

	/// @copyright Same as `NanoLogLine::m_buffer_size` from NanoLog. @hideinitializer
	Size m_size = sizeof(m_stackBuffer);  ///< @brief The current capacity of the buffer in bytes.

	/// @copyright Same as `NanoLogLine::m_heap_buffer` from NanoLog.
	std::unique_ptr<std::byte[]> m_heapBuffer;  ///< The buffer on the heap if the stack buffer became too small.
};

}  // namespace llamalog


/// @brief Operator for printing std::align_val_t values.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The argument.
/// @return The @p logLine for method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, std::align_val_t arg);

/// @brief Operator for printing pointers to std::align_val_t values.
/// @param logLine The `llamalog::LogLine`.
/// @param arg The argument.
/// @return The @p logLine for method chaining.
llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const std::align_val_t* arg);
