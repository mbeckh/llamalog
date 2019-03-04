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
#include <new>
#include <type_traits>

namespace llamalog {

namespace {  // NOLINT(cert-dcl59-cpp, google-build-namespaces): This file will be included in implementation files only.

/// @brief Helper function to create a type-safe formatter argument.
/// @tparam T The type of the argument.
/// @param objectData The serialized byte stream of an object of type @p T.
/// @return A newly created `fmt::basic_format_arg`.
template <typename T>
fmt::basic_format_arg<fmt::format_context> CreateFormatArg(_In_reads_bytes_(sizeof(T)) const std::byte* const objectData) noexcept {
	return fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const T*>(objectData));
}

/// @brief Helper function to create a type by moving from the source.
/// @tparam T The type of the argument.
/// @param src The source address.
/// @param dst The target address.
template <typename T, typename std::enable_if_t<std::is_nothrow_move_constructible_v<T>, int> = 0>
void Construct(_In_reads_bytes_(sizeof(T)) std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Construct MUST NOT be used for trivially copyable types");
	new (dst) T(std::move(*reinterpret_cast<T*>(src)));
}

// @brief Helper function to create a type by copying from the source.
// @details This version is used if the type does not support moving.
// @tparam T The type of the argument.
// @param src The source address.
// @param dst The target address.
template <typename T, typename std::enable_if_t<!std::is_nothrow_move_constructible_v<T>, int> = 0>
void Construct(_In_reads_bytes_(sizeof(T)) std::byte* const src, _Out_writes_bytes_(sizeof(T)) std::byte* dst) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Construct MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_copy_constructible_v<T>, "type MUST be nothrow copy constructible of it's not nothrow move constructible");
	new (dst) T(*reinterpret_cast<const T*>(src));
}

/// @brief Helper function to call the destructor of the internal object.
/// @tparam T The type of the argument.
/// @param obj The object.
template <typename T>
void Destruct(_Inout_updates_bytes_(sizeof(T)) std::byte* obj) noexcept {
	static_assert(!std::is_trivially_copyable_v<T>, "Destruct MUST NOT be used for trivially copyable types");
	static_assert(std::is_nothrow_destructible_v<T>, "type MUST be nothrow destructible");
	reinterpret_cast<T*>(obj)->~T();
}

}  // namespace

template <typename T, typename std::enable_if_t<std::is_trivially_copyable_v<T>, int>>
LogLine& LogLine::AddCustomArgument(const T& arg) {
	using X = std::remove_cv_t<T>;
	WriteTriviallyCopyable(reinterpret_cast<const std::byte*>(std::addressof(arg)), sizeof(X), alignof(X), reinterpret_cast<const void*>(&CreateFormatArg<X>));
	return *this;
}

template <typename T, typename std::enable_if_t<!std::is_trivially_copyable_v<T>, int>>
LogLine& LogLine::AddCustomArgument(const T& arg) {
	using X = std::remove_cv_t<T>;
	static_assert(std::is_same_v<X, llamalog::test::MoveConstructible> || std::is_same_v<X, llamalog::test::CopyConstructible>);
	static_assert(std::is_nothrow_move_constructible_v<X> || std::is_nothrow_copy_constructible_v<X>, "type MUST either be nothrow move or copy constructible");
	std::byte* __restrict ptr = WriteNonTriviallyCopyable(sizeof(X), alignof(X), &llamalog::Construct<X>, &llamalog::Destruct<X>, reinterpret_cast<const void*>(&llamalog::CreateFormatArg<X>));
	new (ptr) X(arg);
	return *this;
}

}  // namespace llamalog
