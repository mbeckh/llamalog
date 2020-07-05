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

#include "exception_format.h"

#include "buffer_management.h"
#include "exception_types.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"

#include <fmt/format.h>

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace llamalog::exception {

namespace {

/// @brief Default size for message buffers for exceptions.
constexpr std::size_t kDefaultBufferSize = 128;

/// @brief A helper to check if a class matches any of a given set.
/// @tparam T The class to check.
/// @tparam A The possible alternatives.
template <class T, class... A>
struct is_any : std::disjunction<std::is_same<T, A>...> {  // NOLINT(readability-identifier-naming): Named like C++ type traits.
														   // empty
};

/// @brief Constant to shorten expressions using `is_any`.
/// @tparam T The class to check.
/// @tparam A The possible alternatives.
template <class T, class... A>
constexpr bool is_any_v = is_any<T, A...>::value;  // NOLINT(readability-identifier-naming): Named like C++ type traits.

/// @brief Helper to get the argument buffer.
/// @param ptr The address of the current exception argument.
/// @return The address of the argument buffer.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const std::byte* GetBuffer(const std::byte* __restrict const ptr) noexcept {
	const std::byte* const buffer = reinterpret_cast<const std::byte*>(&reinterpret_cast<const ExceptionInformation*>(ptr)->padding);
	const LogLine::Size pos = reinterpret_cast<const ExceptionInformation*>(ptr)->length * sizeof(char);
	const LogLine::Align padding = buffer::GetPadding(&buffer[pos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	return &buffer[pos + padding];
}

/// @brief Helper to get the argument buffer.
/// @param ptr The address of the current exception argument.
/// @return The address of the argument buffer.
template <typename T, typename std::enable_if_t<is_any_v<T, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const std::byte* GetBuffer(const std::byte* __restrict const ptr) noexcept {
	return reinterpret_cast<const HeapBasedException*>(ptr)->pHeapBuffer;
}

/// @brief Helper to get the exception message.
/// @param ptr The address of the current exception argument.
/// @return The address of the exception message.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
	return reinterpret_cast<const char*>(&reinterpret_cast<const ExceptionInformation*>(ptr)->padding);
}

/// @brief Helper to get the exception message.
/// @param ptr The address of the current exception argument.
/// @return The address of the exception message.
template <typename T, typename std::enable_if_t<is_any_v<T, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
	return reinterpret_cast<const char*>(&ptr[sizeof(HeapBasedException)]);
}

/// @brief Helper to get the exception message.
/// @param ptr The address of the current exception argument.
/// @return The address of the exception message.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
	return reinterpret_cast<const char*>(&ptr[sizeof(T)]);
}

/// @brief Helper to get the information for `std::system_error`s.
/// @param ptr The address of the current exception argument.
/// @return The address of the information block.
template <typename T, typename std::enable_if_t<std::is_same_v<T, StackBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
	const std::byte* const buffer = GetBuffer<T>(ptr);
	return &buffer[reinterpret_cast<const ExceptionInformation*>(ptr)->used];
}

/// @brief Helper to get the information for `std::system_error`s.
/// @param ptr The address of the current exception argument.
/// @return The address of the information block.
template <typename T, typename std::enable_if_t<std::is_same_v<T, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
	const char* const msg = GetExceptionMessage<T>(ptr);
	return reinterpret_cast<const std::byte*>(&msg[reinterpret_cast<const ExceptionInformation*>(ptr)->length]);
}

/// @brief Helper to get the information for `std::system_error`s.
/// @param ptr The address of the current exception argument.
/// @return The address of the information block.
template <typename T, typename std::enable_if_t<std::is_same_v<T, PlainSystemError>, int> = 0>
[[nodiscard]] static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
	return ptr;
}


/// @brief Parse and format an exception using a subformat, i.e. `{:%[...]}`.
/// @details Output is only written to @p out if at least one pattern was replaced by a value other than the empty string.
/// @note The output target @p out MAY differ from the output target of @p ctx.
/// @param arg The exception to format.
/// @param start The start of the subformat pattern.
/// @param end The end (exclusive) of the subformat pattern.
/// @param ctx The current `fmt::format_context`.
/// @param out The output target.
/// @param args The current formatting arguments.
/// @param formatted Set to `true` if any output was produced.
/// @return The iterator pointing at the last character of the sub pattern, i.e. the `]`. If not `]` exists, the result is @p end.
template <typename T>
[[nodiscard]] std::string::const_iterator ProcessSubformat(const T& arg, const std::string::const_iterator& start, const std::string::const_iterator& end, fmt::format_context& ctx, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args, bool& formatted) {
	std::uint_fast16_t open = 1;
	for (auto it = start; it != end; ++it) {
		if (*it == '\\') {
			if (++it == end) {
				ctx.on_error("invalid escape sequence");
				break;
			}
		} else if (*it == '%') {
			if (++it == end) {
				ctx.on_error("invalid exception specifier");
				break;
			}
			if (*it == '[') {
				++open;
			}
		} else if (*it == ']') {
			if (--open == 0) {
				fmt::basic_memory_buffer<char, kDefaultBufferSize> buf;
				std::back_insert_iterator<fmt::format_context::iterator::container_type> subOut(buf);
				if (Format<T>(arg, start, it, ctx, subOut, args)) {
					std::copy(buf.begin(), buf.end(), out);
					formatted = true;
				}
				return it;
			}
			if (open < 0) {
				ctx.on_error("missing '[' in exception specifier");
			}
		} else {
			// do nothing
		}
	}
	ctx.on_error("missing ']' in exception specifier");
	return end;
}

/// @brief Parse and format an exception using a pattern , i.e. `{:%[...]}`.
/// @note The output target @p out MAY differ from the output target of @p ctx.
/// @param arg The exception to format.
/// @param start The start of the format pattern.
/// @param end The end (exclusive) of the format pattern.
/// @param ctx The current `fmt::format_context`.
/// @param out The output target.
/// @param args The current formatting arguments.
/// @return `true` if any output was produced.
template <typename T>
bool Format(const T& arg, std::string::const_iterator start, const std::string::const_iterator& end, fmt::format_context& ctx, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) {
	bool formatted = false;
	for (auto it = start; it != end; ++it) {
		if (*it == '\\') {
			// copy content up to here
			std::copy(start, it, out);

			start = ++it;
			// leave next character unprocessed
			if (it == end) {
				ctx.on_error("invalid escape sequence");
				break;
			}
		} else if (*it == '{') {
			// copy content up to here
			std::copy(start, it, out);
			if (++it == end) {
				ctx.on_error("missing '}' in exception specifier");
				start = end;
				break;
			}

			auto endOfArgId = it;
			while (endOfArgId != end && *endOfArgId != '}' && *endOfArgId != ':') {
				++endOfArgId;
			}
			auto startOfPattern = endOfArgId;
			auto endOfPattern = endOfArgId;
			if (endOfArgId != end && *endOfArgId == ':') {
				startOfPattern = endOfArgId + 1;
				endOfPattern = startOfPattern;
				while (endOfPattern != end && *endOfPattern != '}') {
					++endOfPattern;
				}
			}
			if (endOfPattern == end) {
				ctx.on_error("missing '}' in exception specifier");
			}

			fmt::format_context::format_arg subArg;
			if (it == endOfArgId) {
				ctx.on_error("exception specifier must have argument identifier");
			} else if (std::all_of(it, endOfArgId, [](const char c) noexcept {
						   return c >= '0' && c <= '9';
					   })) {
				int argId = -1;
				if (std::from_chars(&*it, &*endOfArgId, argId).ptr != &*endOfArgId) {
					ctx.on_error("invalid argument id in exception specifier");
				}
				subArg = ctx.arg(argId);
			} else {
				subArg = ctx.arg(std::string_view(&*it, std::distance(it, endOfArgId)));
			}
			const fmt::format_args subArgs(&subArg, 1);
			fmt::vformat_to(out, fmt::to_string_view("{:" + std::string(startOfPattern, endOfPattern) + "}"), subArgs);

			it = endOfPattern;
			if (it == end) {
				break;
			}
			start = it + 1;
		} else if (*it == '%') {
			// copy content up to here
			std::copy(start, it, out);
			if (++it == end) {
				ctx.on_error("invalid exception specifier");
				start = end;
				break;
			}
			start = it + 1;
			switch (*it) {
			// Subformat
			case '[':
				it = ProcessSubformat(arg, start, end, ctx, out, args, formatted);
				if (it != end) {
					start = it + 1;
				} else {
					start = it;
				}
				break;
			// Timestamp
			case 'T':
				formatted |= FormatTimestamp<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Thread
			case 't':
				formatted |= FormatThread<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// File
			case 'F':
				formatted |= FormatFile<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Line
			case 'L':
				formatted |= FormatLine<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Function
			case 'f':
				formatted |= FormatFunction<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Log Message
			case 'l':
				formatted |= FormatLogMessage<T>(reinterpret_cast<const std::byte*>(&arg), out, args);
				break;
			// what()
			case 'w':
				formatted |= FormatWhat<T>(reinterpret_cast<const std::byte*>(&arg), out, args);
				break;
			// Error Code
			case 'c':
				formatted |= FormatErrorCode<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Category Name
			case 'C':
				formatted |= FormatCategoryName<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// System Error Message
			case 'm':
				formatted |= FormatErrorMessage<T>(reinterpret_cast<const std::byte*>(&arg), out);
				break;
			// Default is to just print the character (but emit an error)
			default:
				ctx.on_error("unknown exception specifier");
				start = it;
				break;
			}
		} else {
			// do nothing
		}
	}
	std::copy(start, end, out);
	return formatted;
}

/// @brief Format the exception timestamp.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatTimestamp(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	std::string str = LogWriter::FormatTimestamp(reinterpret_cast<const ExceptionInformation*>(ptr)->timestamp);
	std::copy(str.cbegin(), str.cend(), out);
	return true;
}

/// @brief SFINAE version for types which do not have a timestamp.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatTimestamp(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the thread id.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatThread(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	fmt::format_to(out, "{}", reinterpret_cast<const ExceptionInformation*>(ptr)->threadId);
	return true;
}

/// @brief SFINAE version for types which do not have a thread id.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatThread(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the file name.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatFile(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::string_view sv(reinterpret_cast<const ExceptionInformation*>(ptr)->file);
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief SFINAE version for types which do not have a file name.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatFile(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the line number.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatLine(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	fmt::format_to(out, "{}", reinterpret_cast<const ExceptionInformation*>(ptr)->line);
	return true;
}

/// @brief SFINAE version for types which do not have a line number.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatLine(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the function name.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatFunction(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::string_view sv(reinterpret_cast<const ExceptionInformation*>(ptr)->function);
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief SFINAE version for types which do not have a function name.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatFunction(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the log message.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @param args The formatter arguments. The vector is populated on the first call.
/// @return `true` if the exception has a log message.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatLogMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) {
	if (!reinterpret_cast<const ExceptionInformation*>(ptr)->length) {
		// custom log message exists
		if (args.empty()) {
			// only copy once
			llamalog::buffer::CopyArgumentsFromBufferTo(GetBuffer<T>(ptr), reinterpret_cast<const ExceptionInformation*>(ptr)->used, args);
		}
		fmt::vformat_to(out, fmt::to_string_view(reinterpret_cast<const ExceptionInformation*>(ptr)->message),
						fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
		return true;
	}
	return false;
}

/// @brief SFINAE version for types which do not have a log message.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatLogMessage(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */, std::vector<fmt::format_context::format_arg>& /* args */) noexcept {
	return false;
}

/// @brief Format the exception message. @details This is the version for exception data having additional logging information.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @param args The formatter arguments. The vector is populated on the first call.
/// @return `true` if there is a message.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, HeapBasedException>, int> = 0>
[[nodiscard]] static bool FormatWhat(const std::byte* __restrict const ptr, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) {
	if (!reinterpret_cast<const ExceptionInformation*>(ptr)->length) {
		// custom log message exists
		return FormatLogMessage<T>(ptr, out, args);
	}

	const std::string_view sv(GetExceptionMessage<T>(ptr), reinterpret_cast<const ExceptionInformation*>(ptr)->length);
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief Format the exception message. @details This is the version for exception data having additional logging information.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @param args The formatter arguments. The vector is populated on the first call.
/// @return `true` if there is a message.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedSystemError, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatWhat(const std::byte* __restrict const ptr, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) {
	if (!reinterpret_cast<const ExceptionInformation*>(ptr)->length) {
		// custom log message exists
		const bool hasMessage = FormatLogMessage<T>(ptr, out, args);
		if (hasMessage) {
			*out = ':';
			*out = ' ';
		}
		return FormatErrorMessage<T>(ptr, out) || hasMessage;
	}

	const std::string_view sv(GetExceptionMessage<T>(ptr), reinterpret_cast<const ExceptionInformation*>(ptr)->length);
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief Format the exception message. @details This is the version exceptions thrown using plain `throw`.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainException, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatWhat(const std::byte* __restrict const ptr, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& /* args */) {
	const LogLine::Length length = llamalog::buffer::GetValue<LogLine::Length>(&ptr[offsetof(T, length)]);

	const std::string_view sv(reinterpret_cast<const char*>(&ptr[sizeof(T)]), length);
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief SFINAE version for types which do not have an error code.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, HeapBasedException, PlainException>, int> = 0>
[[nodiscard]] static bool FormatErrorCode(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the error code for `std::system_error`s.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedSystemError, HeapBasedSystemError, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatErrorCode(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::byte* const systemError = GetSystemError<T>(ptr);

	const int code = llamalog::buffer::GetValue<int>(&systemError[offsetof(T, code)]);

	static_assert(sizeof(code) == sizeof(DWORD));
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers): Limit to represent numbers using > 16 bit IS arbitrary.
	fmt::format_to(out, static_cast<DWORD>(code) & 0xFFFF0000u ? "{:#x}" : "{}", static_cast<DWORD>(code));
	return true;
}

/// @brief SFINAE version for types which do not have a category name.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, HeapBasedException, PlainException>, int> = 0>
[[nodiscard]] static bool FormatCategoryName(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the category name for `std::system_error`s having additional logging data.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedSystemError, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatCategoryName(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::byte* const systemError = GetSystemError<T>(ptr);
	const std::error_category* const pCategory = llamalog::buffer::GetValue<const std::error_category*>(&systemError[offsetof(T, pCategory)]);

	const std::string_view sv(pCategory->name());
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief Format the category name for `std::system_error`s thrown using plain `throw`.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatCategoryName(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::error_category* const pCategory = llamalog::buffer::GetValue<const std::error_category*>(&ptr[offsetof(T, pCategory)]);

	const std::string_view sv(pCategory->name());
	std::copy(sv.cbegin(), sv.cend(), out);
	return true;
}

/// @brief SFINAE version for types which do not have an error code.
/// @tparam T The type of the exception to format.
/// @return Always `false`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedException, HeapBasedException, PlainException>, int> = 0>
[[nodiscard]] static bool FormatErrorMessage(const std::byte* __restrict const /* ptr */, fmt::format_context::iterator& /* out */) noexcept {
	return false;
}

/// @brief Format the error message for `std::system_error`s and `system_error`s having additional logging data.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, StackBasedSystemError, HeapBasedSystemError>, int> = 0>
[[nodiscard]] static bool FormatErrorMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const std::byte* const systemError = GetSystemError<T>(ptr);

	const int code = llamalog::buffer::GetValue<int>(&systemError[offsetof(T, code)]);
	const std::error_category* const pCategory = llamalog::buffer::GetValue<const std::error_category*>(&systemError[offsetof(T, pCategory)]);

	const std::string s = pCategory->message(code);
	std::copy(s.cbegin(), s.cend(), out);
	return true;
}

/// @brief Format the error message for `std::system_error`s and `system_error`s thrown using plain `throw`.
/// @tparam T The type of the exception to format.
/// @param ptr The address of the exception argument in the buffer.
/// @param out The output target.
/// @return Always `true`.
template <typename T, typename std::enable_if_t<is_any_v<T, PlainSystemError>, int> = 0>
[[nodiscard]] static bool FormatErrorMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
	const int code = llamalog::buffer::GetValue<int>(&ptr[offsetof(T, code)]);
	const std::error_category* const pCategory = llamalog::buffer::GetValue<const std::error_category*>(&ptr[offsetof(T, pCategory)]);

	const std::string s = pCategory->message(code);
	std::copy(s.cbegin(), s.cend(), out);
	return true;
}

}  // namespace


fmt::format_parse_context::iterator ExceptionBaseFormatter::parse(fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto start = ctx.begin();
	if (start != ctx.end() && *start == ':') {
		++start;
	}
	int open = 1;
	auto end = start;
	auto const last = ctx.end();
	while (end != last && (*end != '}' || open > 1)) {
		if (*end == '\\') {
			if (++end == last) {
				ctx.on_error("invalid escape sequence");
				break;
			}
		} else if (*end == '{') {
			++open;
		} else if (*end == '}') {
			--open;
		} else {
			// just increment
		}
		++end;
	}
	if (open > 1) {
		ctx.on_error("missing '}' in exception specifier");
	}
	if (start == end) {
		// apply default format
		m_format.assign(R"(%w%[ (%C %c)]%[ @\{%T \[%t\] %F:%L %f\}])");
	} else {
		m_format.assign(start, end);
	}
	return end;
}

template <typename T>
fmt::format_context::iterator ExceptionFormatter<T>::format(const T& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	std::vector<fmt::format_context::format_arg> args;

	fmt::basic_memory_buffer<char, kDefaultBufferSize> buf;
	std::back_insert_iterator<fmt::format_context::iterator::container_type> out(buf);
	const std::string& format = GetFormat();
	Format(arg, format.cbegin(), format.cend(), ctx, out, args);
	return std::copy(buf.begin(), buf.end(), ctx.out());
}

// Explicit instantiation definitions in to keep compile times down
template ExceptionFormatter<StackBasedException>;
template ExceptionFormatter<StackBasedSystemError>;
template ExceptionFormatter<HeapBasedException>;
template ExceptionFormatter<HeapBasedSystemError>;
template ExceptionFormatter<PlainException>;
template ExceptionFormatter<PlainSystemError>;

}  // namespace llamalog::exception
