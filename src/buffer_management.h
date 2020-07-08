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

/// @file
#pragma once

#include "exception_types.h"
#include "marker_types.h"

#include "llamalog/LogLine.h"
#include "llamalog/custom_types.h"

#include <fmt/core.h>

#include <sal.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace llamalog::buffer {

/// @brief The types supported by the logger as arguments. Use `#kTypeId` to get the `#TypeId`.
/// @copyright This type is derived from `SupportedTypes` from NanoLog.
using Types = std::tuple<
	marker::NullValue,
	bool,
	char,
	signed char,
	unsigned char,
	signed short,
	unsigned short,
	signed int,
	unsigned int,
	signed long,
	unsigned long,
	signed long long,
	unsigned long long,
	float,
	double,
	long double,
	const void*,     // MUST NOT cast this back to any object because the object might no longer exist when the message is logged
	const char*,     // string is stored WITHOUT a terminating null character
	const wchar_t*,  // string is stored WITHOUT a terminating null character
	exception::StackBasedException,
	exception::StackBasedSystemError,
	exception::HeapBasedException,
	exception::HeapBasedSystemError,
	exception::PlainException,
	exception::PlainSystemError,
	marker::TriviallyCopyable,
	marker::NonTriviallyCopyable>;

/// @brief Get `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
/// @tparam Types Supply `#Types` for this parameter.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename Types>
struct TypeIndex;

/// @brief Get `#TypeId` at compile time. @details Specialization for the final step in recursive evaluation.
/// @tparam T The type to get the id for.
/// @tparam Types The tuple of types.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename... Types>
struct TypeIndex<T, std::tuple<T, Types...>> {
	static constexpr std::uint8_t kValue = 0;  ///< The tuple index.
};

/// @brief Get `#TypeId` at compile time. @details Specialization for recursive evaluation.
/// @tparam T The type to get the id for.
/// @tparam U The next type required for recursive evaluation.
/// @tparam Types The remaining tuple of types.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename U, typename... Types>
struct TypeIndex<T, std::tuple<U, Types...>> {
	static constexpr std::uint8_t kValue = 1 + TypeIndex<T, std::tuple<Types...>>::kValue;  ///< The tuple index.
};

/// @brief Type of the type marker in the argument buffer.
constexpr std::uint8_t kPointerFlag = 0x80u;
constexpr std::uint8_t kEscapedFlag = 0x40u;
using TypeId = std::uint8_t;
static_assert(std::tuple_size_v<Types> <= (std::numeric_limits<TypeId>::max() & static_cast<TypeId>(~static_cast<TypeId>(kPointerFlag | kEscapedFlag))), "too many types for type of TypeId");

/// @brief A constant to get the `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
/// @tparam kPointer `true` to get the type for a pointer to a value.
template <typename T, bool kPointer = false>
inline constexpr TypeId kTypeId = TypeIndex<T, Types>::kValue | (kPointer ? kPointerFlag : 0u);

constexpr bool IsPointer(const TypeId typeId) noexcept {
	return (typeId & kPointerFlag) != 0;
}

/// @brief Get the `#TypeId` for a type and an optional escaping setting.
/// @tparam T The type.
/// @tparam kPointer `true` to get the type for a pointer to a value.
/// @param escape `true` to mark the parameter for output escaping when build the result.
template <typename T, bool kPointer = false>
constexpr TypeId GetTypeId(const bool escape) noexcept {
	return kTypeId<T, kPointer> | (escape ? kEscapedFlag : 0u);
}

constexpr bool IsEscaped(const TypeId typeId) noexcept {
	return (typeId & kEscapedFlag) != 0;
}

/// @brief Pre-calculated array of sizes required to store values in the buffer. Use `#kTypeSize` to get the size in code.
/// @hideinitializer
constexpr std::uint8_t kTypeSizes[] = {
	sizeof(TypeId),  // NullValue
	sizeof(TypeId) + sizeof(bool),
	sizeof(TypeId) + sizeof(char),
	sizeof(TypeId) + sizeof(signed char),
	sizeof(TypeId) + sizeof(unsigned char),
	sizeof(TypeId) + sizeof(signed short),
	sizeof(TypeId) + sizeof(unsigned short),
	sizeof(TypeId) + sizeof(signed int),
	sizeof(TypeId) + sizeof(unsigned int),
	sizeof(TypeId) + sizeof(signed long),
	sizeof(TypeId) + sizeof(unsigned long),
	sizeof(TypeId) + sizeof(signed long long),
	sizeof(TypeId) + sizeof(unsigned long long),
	sizeof(TypeId) + sizeof(float),
	sizeof(TypeId) + sizeof(double),
	sizeof(TypeId) + sizeof(long double),
	sizeof(TypeId) + sizeof(void*),
	sizeof(TypeId) + sizeof(LogLine::Length) /* + std::byte[padding] + char[std::strlen(str)] */,
	sizeof(TypeId) + sizeof(LogLine::Length) /* + std::byte[padding] + wchar_t[std::wcslen(str)] */,
	sizeof(TypeId) /* + std::byte[padding] */ + offsetof(exception::ExceptionInformation /* exception::StackBasedException */, padding) /* + char[exception::ExceptionInformation::length] + std::byte[padding] + std::byte[exception::ExceptionInformation::m_used] */,
	sizeof(TypeId) /* + std::byte[padding] */ + offsetof(exception::ExceptionInformation /* exception::StackBasedException */, padding) /* + char[exception::ExceptionInformation::length] + std::byte[padding] + std::byte[exception::ExceptionInformation::m_used] */ + sizeof(exception::StackBasedSystemError),
	sizeof(TypeId) /* + std::byte[padding] */ + sizeof(exception::HeapBasedException) /* + char[exception::ExceptionInformation::length] */,
	sizeof(TypeId) /* + std::byte[padding] */ + sizeof(exception::HeapBasedException) /* + char[exception::ExceptionInformation::length] */ + sizeof(exception::HeapBasedSystemError),
	sizeof(TypeId) + sizeof(exception::PlainException) /* + char[exception::PlainException::length] */,
	sizeof(TypeId) + sizeof(exception::PlainSystemError) /* + char[exception::PlainSystemError::length] */,
	sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(internal::FunctionTable::CreateFormatArg) + sizeof(LogLine::Size) /* + std::byte[padding] + sizeof(arg) */,
	sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(internal::FunctionTable*) + sizeof(LogLine::Size) /* + std::byte[padding] + sizeof(arg) */
};
static_assert(sizeof(TypeId) + offsetof(exception::ExceptionInformation /* exception::StackBasedException */, padding) + sizeof(exception::StackBasedSystemError) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(sizeof(TypeId) + sizeof(exception::HeapBasedException) + sizeof(exception::HeapBasedSystemError) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(internal::FunctionTable::CreateFormatArg) + sizeof(LogLine::Size) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(internal::FunctionTable*) + sizeof(LogLine::Size) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(std::tuple_size_v<Types> == sizeof(kTypeSizes) / sizeof(kTypeSizes[0]), "length of kTypeSizes does not match Types");

/// @brief A constant to get the (basic) buffer size of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
inline constexpr std::uint8_t kTypeSize = kTypeSizes[kTypeId<T>];


/// @brief The number of bytes to add to the argument buffer after it became too small.
constexpr LogLine::Size kGrowBytes = 512u;

/// @brief Get the next allocation chunk, i.e. the next block which is a multiple of `#kGrowBytes`.
/// @param value The required size.
/// @return The value rounded up to multiples of `#kGrowBytes`.
constexpr __declspec(noalias) LogLine::Size GetNextChunk(const LogLine::Size value) noexcept {
	constexpr LogLine::Size kMask = kGrowBytes - 1u;
	static_assert((kGrowBytes & kMask) == 0, "kGrowBytes must be a multiple of 2");

	return value + ((kGrowBytes - (value & kMask)) & kMask);
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @tparam T The type.
/// @param ptr The target address.
/// @return The padding to account for a properly aligned type.
template <typename T>
[[nodiscard]] __declspec(noalias) LogLine::Align GetPadding(_In_ const std::byte* __restrict const ptr) noexcept {
	static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of type is too large");
	constexpr LogLine::Align kMask = alignof(T) - 1u;
	return static_cast<LogLine::Align>(alignof(T) - (reinterpret_cast<std::uintptr_t>(ptr) & kMask)) & kMask;
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @param ptr The target address.
/// @param align The required alignment in number of bytes.
/// @return The padding to account for a properly aligned type.
[[nodiscard]] __declspec(noalias) LogLine::Align GetPadding(_In_ const std::byte* __restrict ptr, LogLine::Align align) noexcept;

/// @brief Read a value from the buffer.
/// @details Use std::memcpy to comply with strict aliasing and alignment rules.
/// @tparam T The target type to read.
/// @param buffer The source address to read from.
/// @return The read value.
template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T> && std::is_trivially_constructible_v<T> && (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>), int> = 0>
[[nodiscard]] __declspec(noalias) T GetValue(_In_ const std::byte* __restrict const buffer) noexcept {
	T result;
	std::memcpy(&result, buffer, sizeof(T));  // NOLINT(bugprone-sizeof-expression): Deliberately supporting sizeof pointers.
	return result;
}

/// @brief Copy arguments from a `LogInformation` buffer into a `std::vector`.
/// @param buffer The buffer holding the data.
/// @param used The number of valid bytes in @p buffer.
/// @param args The `std::vector` to receive the message arguments.
/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict buffer, LogLine::Size used, std::vector<fmt::format_context::format_arg>& args);

/// @brief Copy all objects from one buffer to another.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict src, _Out_writes_bytes_(used) std::byte* __restrict dst, LogLine::Size used);

/// @brief Move all objects from one buffer to another. @details The function also calls the moved-from's destructor.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict src, _Out_writes_bytes_(used) std::byte* __restrict dst, LogLine::Size used) noexcept;

/// @brief Call all the destructors of non-trivially copyable custom arguments in a buffer.
/// @param buffer The buffer.
/// @param used The number of bytes used in the buffer.
void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogLine::Size used) noexcept;

}  // namespace llamalog::buffer
