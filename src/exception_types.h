/*
Copyright 2020 Michael Beckh

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

#pragma once

#include "llamalog/LogLine.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace llamalog::exception {

/// @brief Basic information of a logged exception inside the buffer.
struct ExceptionInformation final {
	FILETIME timestamp;               ///< @brief Same as `LogLine::m_timestamp`.
	const char* __restrict file;      ///< @brief Same as `LogLine::m_file`.
	const char* __restrict function;  ///< @brief Same as `LogLine::m_function`.
	const char* __restrict message;   ///< @brief Same as `LogLine::m_message`.
	DWORD threadId;                   ///< @brief Same as `LogLine::m_threadId`.
	std::uint32_t line;               ///< @brief Same as `LogLine::m_line`.
	LogLine::Size used;               ///< @brief Same as `LogLine::m_used`.
	LogLine::Length length;           ///< @brief Length of the exception message (if `message` is `nullptr`).
	bool hasNonTriviallyCopyable;     ///< @brief Same as `LogLine::m_hasNonTriviallyCopyable`.
	std::byte padding[1];             ///< @brief Padding, but used for `exceptionMessage` in `StackBasedException`.
};

/// @brief Marker type for type-based lookup and layout of exception not using the heap for log arguments.
struct StackBasedException final {
	ExceptionInformation exceptionInformation;
	/* char exceptionMessage[length] */  // dynamic length
	/* std::byte padding[] */            // dynamic length
	/* std::byte stackBuffer[used] */    // dynamic length
};

/// @brief Marker type for type-based lookup and layout of exception using the heap for log arguments.
struct HeapBasedException final {
	ExceptionInformation exceptionInformation;
	std::byte* __restrict pHeapBuffer;   ///< @brief Same as `LogLine::m_heapBuffer`.
	/* char exceptionMessage[length] */  // dynamic length
};

#pragma pack(push, 1)  // structs are only used as templates
/// @brief A struct defining the layout of a `std::system_error` as a log argument.
/// @details This version is used if the data fits into the stack-based buffer.
struct StackBasedSystemError final {
	/* StackBasedException base */
	int code;                              ///< @brief The error code.
	const std::error_category* pCategory;  ///< @brief The error category.
};

/// @brief A struct defining the layout of a `std::system_error` as a log argument.
/// @details This version is used if the data does not fit into the stack-based buffer.
struct HeapBasedSystemError final {
	/* HeapBasedException base */
	int code;                              ///< @brief The error code.
	const std::error_category* pCategory;  ///< @brief The error category.
};

/// @brief A struct defining the layout of a `std::exception` as a log argument if it is throw using plain `throw`.
struct PlainException final {
	LogLine::Length length;              ///< @brief The length of the message in characters (NOT including the terminating null character).
	/* char exceptionMessage[length] */  // dynamic length
};

/// @brief A struct defining the layout of a `std::system_error` as a log argument if it is throw using plain `throw`.
struct PlainSystemError final {
	LogLine::Length length;                ///< @brief The length of the message in characters (NOT including the terminating null character).
	int code;                              ///< @brief The error code.
	const std::error_category* pCategory;  ///< @brief The error category.
	/* char exceptionMessage[length] */    // dynamic length
};
#pragma pack(pop)

// assert packing
// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.
static_assert(sizeof(StackBasedSystemError) == sizeof(StackBasedSystemError::code) + sizeof(StackBasedSystemError::pCategory));
// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.
static_assert(sizeof(HeapBasedSystemError) == sizeof(HeapBasedSystemError::code) + sizeof(HeapBasedSystemError::pCategory));
static_assert(sizeof(PlainException) == sizeof(PlainException::length));
// NOLINTNEXTLINE(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.
static_assert(sizeof(PlainSystemError) == sizeof(PlainSystemError::length) + sizeof(PlainSystemError::code) + sizeof(PlainSystemError::pCategory));

}  // namespace llamalog::exception
