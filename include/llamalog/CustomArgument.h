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

namespace llamalog {

namespace {

/// @brief Helper function to create a type-safe formatter argument.
/// @tparam T The type of the argument.
/// @param objectData The serialized byte stream of an object of type @p T.
/// @return A newly created `fmt::basic_format_arg`.
template <typename T>
static fmt::basic_format_arg<fmt::format_context> CreateFormatArg(_In_reads_bytes_(sizeof(T)) const std::byte* const objectData) noexcept {
	return fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const T*>(objectData));
}

}  // anonymous namespace

template <typename T>
void LogLine::AddCustomArgument(const T& arg) {
	WriteCustomArgument(reinterpret_cast<const std::byte*>(std::addressof(arg)), sizeof(T), alignof(T), reinterpret_cast<const void*>(&CreateFormatArg<T>));
}

}  // namespace llamalog
