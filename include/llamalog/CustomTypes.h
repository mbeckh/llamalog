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
/// @brief Include for implementation of custom types. Include this file in every .cpp calling
/// `LogLine::AddCustomArgument`. @details The templated function `llamalog::LogLine::AddCustomArgument` has been moved
/// to a separate file to not make {fmt} a dependency for all users of llamalog.
#pragma once

#include <llamalog/LogLine.h>

#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace llamalog {

namespace internal {

/// @brief Helper function to create a type-safe formatter argument.
/// @tparam T The type of the argument.
/// @param objectData The serialized byte stream of an object of type @p T.
/// @return A newly created `fmt::basic_format_arg`.
template <typename T>
fmt::basic_format_arg<fmt::format_context> CreateFormatArg(_In_reads_bytes_(sizeof(T)) const std::byte* __restrict const objectData) noexcept {
	return fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const T*>(objectData));
}

/// @brief Helper function to create a type by copying from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T>
void Copy(_In_reads_bytes_(sizeof(T)) const std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) {
	new (dst) T(*reinterpret_cast<const T*>(src));
}

/// @brief Helper function to create a type by moving from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T, typename std::enable_if_t<std::is_nothrow_move_constructible_v<T>, int> = 0>
void Move(_Inout_updates_bytes_(sizeof(T)) std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	new (dst) T(std::move(*reinterpret_cast<T*>(src)));
}

/// @brief Helper function to create a type by copying from the source.
/// @details This version is used if the type does not support moving.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T, typename std::enable_if_t<!std::is_nothrow_move_constructible_v<T>, int> = 0>
void Move(_In_reads_bytes_(sizeof(T)) std::byte* __restrict const src, _Out_writes_bytes_(sizeof(T)) std::byte* __restrict const dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Move MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_copy_constructible_v<T>, "type MUST be nothrow copy constructible of it's not nothrow move constructible");
	new (dst) T(*reinterpret_cast<const T*>(src));
}

/// @brief Helper function to call the destructor of the internal object.
/// @tparam T The type of the argument.
/// @param obj The object.
template <typename T>
void Destruct(_Inout_updates_bytes_(sizeof(T)) std::byte* __restrict const obj) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Destruct MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_destructible_v<T>, "type MUST be nothrow destructible");
	reinterpret_cast<T*>(obj)->~T();
}

/// @brief A struct with all functions to manage objects in the buffer.
/// @details This is the version used to access the data of `FunctionTableInstance` in the buffer.
struct FunctionTable {
	/// @brief Type of the function to construct an argument in the buffer by copying.
	using Copy = void (*)(_In_ const std::byte* __restrict, _Out_ std::byte* __restrict);
	/// @brief Type of the function to construct an argument in the buffer by moving.
	using Move = void (*)(_Inout_ std::byte* __restrict, _Out_ std::byte* __restrict) noexcept;
	/// @brief Type of the function to destruct an argument in the buffer.
	using Destruct = void (*)(_Inout_ std::byte* __restrict) noexcept;
	/// @brief Type of the function to create a formatter argument.
	using CreateFormatArg = fmt::basic_format_arg<fmt::format_context> (*)(const std::byte* __restrict) noexcept;

	/// @brief A pointer to a function which creates the custom type either by copying. Both adresses can be assumed to be properly aligned.
	Copy copy;
	/// @brief A pointer to a function which creates the custom type either by copy or move, whichever is
	/// more efficient. Both adresses can be assumed to be properly aligned.
	Move move;
	/// @brief A pointer to a function which calls the type's destructor.
	Destruct destruct;
	/// @brief A pointer to a function which has a single argument of type `std::byte*` and returns a
	/// newly created `fmt::basic_format_arg` object.
	CreateFormatArg createFormatArg;
};

/// @brief A struct with all functions to manage objects in the buffer.
/// @details This is the actual function table with all pointers.
template <typename T>
struct FunctionTableInstance {
	/// @brief A pointer to a function which creates the custom type either by copying. Both adresses can be assumed to be properly aligned.
	FunctionTable::Copy copy = Copy<T>;
	/// @brief A pointer to a function which creates the custom type either by copy or move, whichever is
	/// more efficient. Both adresses can be assumed to be properly aligned.
	FunctionTable::Move move = Move<T>;
	/// @brief A pointer to a function which calls the type's destructor.
	FunctionTable::Destruct destruct = Destruct<T>;
	/// @brief A pointer to a function which has a single argument of type `std::byte*` and returns a
	/// newly created `fmt::basic_format_arg` object.
	FunctionTable::CreateFormatArg createFormatArg = CreateFormatArg<T>;
};

}  // namespace internal

template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int>>
LogLine& LogLine::AddCustomArgument(const T& arg) {
	using X = std::remove_cv_t<T>;
	static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
	static_assert(sizeof(X) <= 0xFFFFFFFu, "custom type is too large");  // allow max. 255 MB, NOLINT(bugprone-sizeof-expression): comparison with constant is intended
	WriteTriviallyCopyable(reinterpret_cast<const std::byte*>(std::addressof(arg)), sizeof(X), alignof(X), reinterpret_cast<void (*)()>(internal::CreateFormatArg<X>));
	return *this;
}

template <typename T, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int>>
LogLine& LogLine::AddCustomArgument(const T& arg) {
	using X = std::remove_cv_t<T>;

	static_assert(alignof(X) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of custom type");
	static_assert(sizeof(X) <= 0xFFFFFFFu, "custom type is too large");  // allow max. 255 MB, NOLINT(bugprone-sizeof-expression): comparison with constant is intended
	static_assert(std::is_copy_constructible_v<X>, "type MUST be copy constructible");
	static_assert(sizeof(internal::FunctionTableInstance<X>) == sizeof(internal::FunctionTable));
	static_assert(offsetof(internal::FunctionTableInstance<X>, copy) == offsetof(internal::FunctionTable, copy));
	static_assert(offsetof(internal::FunctionTableInstance<X>, move) == offsetof(internal::FunctionTable, move));
	static_assert(offsetof(internal::FunctionTableInstance<X>, destruct) == offsetof(internal::FunctionTable, destruct));
	static_assert(offsetof(internal::FunctionTableInstance<X>, createFormatArg) == offsetof(internal::FunctionTable, createFormatArg));

	static constexpr internal::FunctionTableInstance<X> functionTable;
	std::byte* __restrict const ptr = WriteNonTriviallyCopyable(sizeof(X), alignof(X), static_cast<const void*>(&functionTable));
	new (ptr) X(arg);
	return *this;
}

}  // namespace llamalog
