/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0
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

#include "llamalog/LogLine.h"

#include "llamalog/LogWriter.h"
#include "llamalog/llamalog.h"

#include <fmt/format.h>

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef __clang_analyzer__
// MSVC does not yet have __builtin_offsetof which gives false errors in the clang-tidy
#pragma push_macro("offsetof")
#define offsetof __builtin_offsetof
#endif

namespace llamalog {

//
// Helpers
//

/// @brief Helper class to access internals of LogLine in the implementation file.
class LogLine::Internal final {
public:
	// Do not allow creation of class.
	Internal() = delete;
	Internal(const Internal&) = delete;  ///< @nocopyconstructor
	Internal(Internal&&) = delete;       ///< @nomoveconstructor
	~Internal() = delete;

public:
	Internal& operator=(const Internal&) = delete;   ///< @noassignmentoperator
	Internal& operator=(const Internal&&) = delete;  ///< @nomoveoperator

public:
	using Copy = LogLine::Copy;          ///< @brief Provide access to type outside of class.
	using Move = LogLine::Move;          ///< @brief Provide access to type outside of class.
	using Destruct = LogLine::Destruct;  ///< @brief Provide access to type outside of class.
};

namespace {

/// @brief Type of the function to create a formatter argument.
using CreateFormatArg = fmt::basic_format_arg<fmt::format_context> (*)(const std::byte* __restrict) noexcept;

/// @brief The number of bytes to add to the argument buffer after it became too small.
constexpr LogLine::Size kGrowBytes = 512u;

/// @brief Basic information of a logged exception inside the buffer.
struct ExceptionInformation {
	FILETIME timestamp;                 ///< @brief Same as `LogLine::m_timestamp`.
	const char* __restrict szFile;      ///< @brief Same as `LogLine::m_szFile`.
	const char* __restrict szFunction;  ///< @brief Same as `LogLine::m_szFunction`.
	const char* __restrict szMessage;   ///< @brief Same as `LogLine::m_szMessage`.
	DWORD threadId;                     ///< @brief Same as `LogLine::m_threadId`.
	std::uint32_t line;                 ///< @brief Same as `LogLine::m_line`.
	LogLine::Size cbUsed;               ///< @brief Same as `LogLine::m_cbUsed`.
	LogLine::Length cbLength;           ///< @brief Length of the exception message.
	bool hasNonTriviallyCopyable;       ///< @brief Same as `LogLine::m_hasNonTriviallyCopyable`.
	std::byte padding[1];               ///< @brief Padding, but used for `exceptionMessage` in `StackBasedException`.
};

/// @brief Marker type for type-based lookup and layout of exception not using the heap for log arguments.
struct StackBasedException final : ExceptionInformation {
	/* char exceptionMessage[cbLength] */  // dynamic length
	/* std::byte padding[] */              // dynamic length
	/* std::byte stackBuffer[cbUsed] */    // dynamic length
};

/// @brief Marker type for type-based lookup and layout of exception using the heap for log arguments.
struct HeapBasedException final : ExceptionInformation {
	std::byte* __restrict pHeapBuffer;     ///< @brief Same as `LogLine::m_heapBuffer`.
	/* char exceptionMessage[cbLength] */  // dynamic length
};

#pragma pack(push, 1)  // structs are only used as templates
/// @brief A struct defining the layout of a `std::system_error` as a log argument.
/// @details This version is used if the data fits into the stack-based buffer.
struct StackBasedSystemError final {
	/* StackBasedException base */
	int code;                    ///< @brief The error code.
	LogLine::Length length;      ///< @brief The length of the category in characters (NOT including the terminating null character).
	/* char category[length] */  // dynamic length
};

/// @brief A struct defining the layout of a `std::system_error` as a log argument.
/// @details This version is used if the data does not fit into the stack-based buffer.
struct HeapBasedSystemError final {
	/* HeapBasedException base */
	int code;                    ///< @brief The error code.
	LogLine::Length length;      ///< @brief The length of the category in characters (NOT including the terminating null character).
	/* char category[length] */  // dynamic length
};

/// @brief A struct defining the layout of a `std::exception` as a log argument if it is throw using plain `throw`.
struct PlainException final {
	LogLine::Length length;  ///< @brief The length of the message in characters (NOT including the terminating null character).
};

/// @brief A struct defining the layout of a `std::system_error` as a log argument if it is throw using plain `throw`.
struct PlainSystemError final {
	LogLine::Length length;              ///< @brief The length of the message in characters (NOT including the terminating null character).
	int code;                            ///< @brief The error code.
	LogLine::Length categoryLength;      ///< @brief The length of the category in characters (NOT including the terminating null character).
	/* char exceptionMessage[length] */  // dynamic length
	/* char category[categoryLength] */  // dynamic length
};
#pragma pack(pop)

static_assert(sizeof(StackBasedSystemError) == sizeof(StackBasedSystemError::code) + sizeof(StackBasedSystemError::length));
static_assert(sizeof(HeapBasedSystemError) == sizeof(HeapBasedSystemError::code) + sizeof(HeapBasedSystemError::length));
static_assert(sizeof(PlainException) == sizeof(PlainException::length));
static_assert(sizeof(PlainSystemError) == sizeof(PlainSystemError::length) + sizeof(PlainSystemError::code) + sizeof(PlainSystemError::categoryLength));

/// @brief Marker type for type-based lookup.
struct TriviallyCopyable final {
	// empty
};

/// @brief Marker type for type-based lookup.
struct NonTriviallyCopyable final {
	// empty
};

/// @brief The types supported by the logger as arguments. Use `#kTypeId` to get the `#TypeId`.
/// @copyright This type is derived from `SupportedTypes` from NanoLog.
using Types = std::tuple<
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
	StackBasedException,
	StackBasedSystemError,
	HeapBasedException,
	HeapBasedSystemError,
	PlainException,
	PlainSystemError,
	TriviallyCopyable,
	NonTriviallyCopyable>;

/// @brief Get `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
/// @tparam Types Supply `#Types` for this parameter.
/// @copyright The template is copied from `TupleIndex` from NanoLog.
template <typename T, typename Types>
struct TypeIndex;

/// @brief Get `#TypeId` at compile time. @details Specialization for the the final step in recursive evaluation.
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
using TypeId = std::uint8_t;
static_assert(std::tuple_size_v<Types> <= std::numeric_limits<TypeId>::max(), "too many types for type of TypeId");

/// @brief A constant to get the `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
constexpr TypeId kTypeId = TypeIndex<T, Types>::kValue;

/// @brief Pre-calculated array of sizes required to store values in the buffer. Use `#kTypeSize` to get the size in code.
/// @hideinitializer
constexpr std::uint8_t kTypeSizes[] = {
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
	sizeof(TypeId) + sizeof(LogLine::Length) /* + padding + std::strlen(str) * sizeof(char) */,
	sizeof(TypeId) + sizeof(LogLine::Length) /* + padding + std::wcslen(str) * sizeof(wchar_t) */,
	sizeof(TypeId) /* + padding */ + offsetof(StackBasedException, padding) /* + StackBasedException::m_cbLength + padding + StackBasedException::m_cbUsed */,
	sizeof(TypeId) /* + padding */ + offsetof(StackBasedException, padding) /* + StackBasedException::m_cbLength + padding + StackBasedException::m_cbUsed */ + sizeof(StackBasedSystemError) /* + StackBasedSystemError::categoryNameLength */,
	sizeof(TypeId) /* + padding */ + sizeof(HeapBasedException) /* + HeapBasedException::m_cbLength */,
	sizeof(TypeId) /* + padding */ + sizeof(HeapBasedException) /* + HeapBasedException::m_cbLength */ + sizeof(HeapBasedSystemError) /* + HeapBasedSystemError::categoryLength */,
	sizeof(TypeId) + sizeof(PlainException) /* + PlainException::length */,
	sizeof(TypeId) + sizeof(PlainSystemError) /* + PlainSystemError::length + PlainSystemError::categoryLength */,
	sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(CreateFormatArg) + sizeof(LogLine::Size) /* + padding + sizeof(arg) */,
	sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move) + sizeof(LogLine::Internal::Destruct) + sizeof(CreateFormatArg) + sizeof(LogLine::Size) /* + padding + sizeof(arg) */
};
static_assert(sizeof(TypeId) + sizeof(LogLine::Align) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move) + sizeof(LogLine::Internal::Destruct) + sizeof(CreateFormatArg) + sizeof(LogLine::Size) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(sizeof(TypeId) + offsetof(StackBasedException, padding) + sizeof(StackBasedSystemError) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(sizeof(TypeId) + sizeof(HeapBasedException) + sizeof(HeapBasedSystemError) <= std::numeric_limits<std::uint8_t>::max(), "type for sizes is too small");
static_assert(std::tuple_size_v<Types> == sizeof(kTypeSizes) / sizeof(kTypeSizes[0]), "length of kTypeSizes does not match Types");

/// @brief A constant to get the (basic) buffer size of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
constexpr std::uint8_t kTypeSize = kTypeSizes[kTypeId<T>];

static_assert(__STDCPP_DEFAULT_NEW_ALIGNMENT__ <= std::numeric_limits<LogLine::Align>::max(), "type LogLine::Align is too small");


/// @brief Get the required padding for a type starting at the next possible offset.
/// @tparam T The type.
/// @param ptr The target address.
/// @return The padding to account for a properly aligned type.
template <typename T>
__declspec(noalias) LogLine::Align GetPadding(_In_ const std::byte* __restrict const ptr) noexcept {
	static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "alignment of type is too large");
	constexpr LogLine::Align kMask = alignof(T) - 1u;
	return static_cast<LogLine::Align>(alignof(T) - (reinterpret_cast<uintptr_t>(ptr) & kMask)) & kMask;
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @param ptr The target address.
/// @param align The required alignment in number of bytes.
/// @return The padding to account for a properly aligned type.
__declspec(noalias) LogLine::Align GetPadding(_In_ const std::byte* __restrict const ptr, const LogLine::Align align) noexcept {
	assert(align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	const LogLine::Align mask = align - 1u;
	return static_cast<LogLine::Align>(align - (reinterpret_cast<uintptr_t>(ptr) & mask)) & mask;
}


/// @brief Get the next allocation chunk, i.e. the next block which is a multiple of `#kGrowBytes`.
/// @param value The required size.
/// @return The value rounded up to multiples of `#kGrowBytes`.
constexpr __declspec(noalias) LogLine::Size GetNextChunk(const LogLine::Size value) noexcept {
	constexpr LogLine::Size kMask = kGrowBytes - 1u;
	static_assert((kGrowBytes & kMask) == 0, "kGrowBytes must be a multiple of 2");
	return value + ((kGrowBytes - (kGrowBytes & kMask)) & kMask);
}


/// @brief Get the id of the current thread.
/// @return The id of the current thread.
/// @copyright The function is based on `this_thread_id` from NanoLog.
DWORD GetCurrentThreadId() noexcept {
	static const thread_local DWORD kId = ::GetCurrentThreadId();
	return kId;
}


/// @brief Get the additional logging context of an exception if it exists and get optional information for `std::system_error`.
/// @note The function MUST be called from within a catch block to get the object, elso `nullptr` is returned for both values.
/// @param pSystemError A pointer which is set if the exception is of type `std::system_error`, else the parameter is set to `nullptr`.
/// @return The logging context if it exists, else `nullptr`.
_Ret_maybenull_ const BaseException* GetCurrentExceptionAsBaseException(const std::system_error*& pSystemError) noexcept {
	try {
		throw;
	} catch (const std::system_error& e) {
		pSystemError = &e;
	} catch (...) {
		pSystemError = nullptr;
	}
	try {
		throw;
	} catch (const BaseException& e) {
		return &e;
	} catch (...) {
		return nullptr;
	}
}


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


//
// Escaping Formatter
//

/// @brief Escape a string according to C escaping rules.
/// @warning If no escaping is needed, an empty string is returned for performance reasons.
/// @param sv The input value.
/// @return The escaped result or an empty string.
std::string EscapeC(const std::string_view& sv) {
	constexpr const char kHexDigits[] = "0123456789ABCDEF";

	std::string result;
	auto begin = sv.cbegin();
	const auto end = sv.cend();

	for (auto it = begin; it != end; ++it) {
		const std::uint8_t c = *it;  // MUST be std::uint8_t, NOT char
		if (c == '"' || c == '\\' || c < 0x20 || c > 0x7f) {
			if (result.empty()) {
				const LogLine::Length len = static_cast<LogLine::Length>(sv.length());
				const LogLine::Length extra = static_cast<LogLine::Length>(std::distance(it, end)) >> 2u;
				result.reserve(len + std::max<LogLine::Length>(extra, 2u));
			}
			result.append(begin, it);
			result.push_back('\\');
			switch (c) {
			case '"':
			case '\\':
				result.push_back(c);
				break;
			case '\n':
				result.push_back('n');
				break;
			case '\r':
				result.push_back('r');
				break;
			case '\t':
				result.push_back('t');
				break;
			case '\b':
				result.push_back('b');
				break;
			case '\f':
				result.push_back('f');
				break;
			case '\v':
				result.push_back('v');
				break;
			case '\a':
				result.push_back('a');
				break;
			default:
				result.push_back('x');
				result.push_back(kHexDigits[c / 16]);
				result.push_back(kHexDigits[c % 16]);
			}
			begin = it + 1;
		}
	}
	if (!result.empty()) {
		result.append(begin, end);
	}
	return result;
}

/// @brief A type definition for easier reading.
using ArgFormatter = fmt::arg_formatter<fmt::back_insert_range<fmt::format_context::iterator::container_type>>;

/// @brief A custom argument formatter to support escaping for strings and characters.
/// @details Any string is escaped according to C escaping rules, except if the type specifier is present.
/// For example, the patterns `{:c}` and `{:s}` prevent escaping.
class EscapingFormatter : public ArgFormatter {
public:
	using ArgFormatter::arg_formatter;
	using ArgFormatter::operator();

	/// @brief Escape a `char` if necessary.
	/// @param value The value to format.
	/// @return see `fmt::arg_formatter::operator()`.
	auto operator()(const char value) {
		const fmt::format_specs* const specs = spec();
		if ((!specs || !specs->type) && (value < 0x20 || value > 0x7f)) {
			std::string_view sv(&value, 1);
			std::string str = EscapeC(sv);
			if (!str.empty()) {
				return ArgFormatter::operator()(str);
			}
		}
		return ArgFormatter::operator()(value);
	}

	/// @brief Escape a C-style string if necessary.
	/// @param value The value to format.
	/// @return see `fmt::arg_formatter::operator()`.
	auto operator()(const char* const value) {
		const fmt::format_specs* const specs = spec();
		if (!specs || !specs->type) {
			std::string_view sv(value);
			std::string str = EscapeC(sv);
			if (!str.empty()) {
				return ArgFormatter::operator()(str);
			}
		}
		return ArgFormatter::operator()(value);
	}

	/// @brief Escape a `std::string` or `std::string_view` if necessary.
	/// @param value The value to format.
	/// @return see `fmt::arg_formatter::operator()`.
	auto operator()(const fmt::string_view value) {
		const fmt::format_specs* const specs = spec();
		if (!specs || !specs->type) {
			std::string_view sv(value.data(), value.size());
			std::string str = EscapeC(sv);
			if (!str.empty()) {
				return ArgFormatter::operator()(str);
			}
		}
		return ArgFormatter::operator()(value);
	}
};


//
// Custom Formatters
//

/// @brief Helper class to pass a wide character string stored inline in the buffer to the formatter.
struct InlineWideChar final {
	std::byte pos;  ///< The address of this variable is placed above the address of the length.
};

}  // namespace
}  // namespace llamalog

/// @brief Specialization of a `fmt::formatter` to perform automatic conversion of wide characters to UTF-8.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::InlineWideChar> {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	auto parse(fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		auto it = ctx.begin();
		if (it != ctx.end() && *it == ':') {
			++it;
		}
		auto end = it;
		while (end != ctx.end() && *end != '}') {
			++end;
		}
		m_format.reserve(end - it + 3);
		m_format.append("{:");
		m_format.append(it, end);
		m_format.push_back('}');
		return end;
	}

	/// @brief Format the wide character string stored inline in the buffer.
	/// @param arg A structure providing the address of the wide character string.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	auto format(const llamalog::InlineWideChar& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		// address of buffer is the address of the lenght field
		const std::byte* const buffer = &arg.pos;

		llamalog::LogLine::Length length;
		std::memcpy(&length, buffer, sizeof(length));

		const llamalog::LogLine::Align padding = llamalog::GetPadding<wchar_t>(&buffer[sizeof(length)]);
		const wchar_t* const wstr = reinterpret_cast<const wchar_t*>(&buffer[sizeof(length) + padding]);

		DWORD errorCode;
		if (length <= 256) {
			// try with a fixed size buffer
			char sz[256];
			const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), sz, sizeof(sz), nullptr, nullptr);
			if (sizeInBytes) {
				return fmt::vformat_to<llamalog::EscapingFormatter>(ctx.out(), fmt::to_string_view(m_format), fmt::basic_format_args(fmt::make_format_args(std::string_view(sz, sizeInBytes / sizeof(char)))));
			}
			errorCode = GetLastError();
			if (errorCode != ERROR_INSUFFICIENT_BUFFER) {
				goto error;  // NOLINT(cppcoreguidelines-avoid-goto): yes, I DO want a goto here
			}
		}
		{
			const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
			if (sizeInBytes) {
				std::unique_ptr<char[]> str = std::make_unique<char[]>(sizeInBytes / sizeof(char));
				if (WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), str.get(), sizeInBytes, nullptr, nullptr)) {
					return fmt::vformat_to<llamalog::EscapingFormatter>(ctx.out(), fmt::to_string_view(m_format), fmt::basic_format_args(fmt::make_format_args(std::string_view(str.get(), sizeInBytes / sizeof(char)))));
				}
			}
			errorCode = GetLastError();
		}

	error:
		LOG_ERROR_INTERNAL("WideCharToMultiByte for length {}: {:#010x})", length, errorCode);
		std::string_view sv("(ERROR)");
		return std::copy(sv.cbegin(), sv.cend(), ctx.out());
	}

private:
	std::string m_format;  ///< The original format pattern with the argument number (if any) removed.
};

namespace llamalog {
namespace {

void CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict buffer, LogLine::Size used, std::vector<fmt::basic_format_arg<fmt::format_context>>& args);

/// @brief Base class for a `fmt::formatter` to print exception arguments.
/// @tparam T The type of the exception argument.
template <typename T>
struct ExceptionFormatter {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	auto parse(fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
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
			m_format.assign(R"(%[%m: ]%[(%C %c) ]%e%[ @\{%T \[%t\] %f(%F:%L)\}])");
		} else {
			m_format.assign(start, end);
		}
		return end;
	}


	/// @brief Format the log information stored inline in the buffer.
	/// @param arg A structure providing the data.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	auto format(const T& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		std::vector<fmt::format_context::format_arg> args;

		fmt::basic_memory_buffer<char, 128> buf;
		std::back_insert_iterator<fmt::format_context::iterator::container_type> out(buf);
		Format(arg, m_format.cbegin(), m_format.cend(), ctx, out, args);
		return std::copy(buf.begin(), buf.end(), ctx.out());
	}

	/// @brief Parse and format an exception using a subformat, i.e. `{:%[...]}`.
	/// @details Output is only written to @p out if at least one pattern was replaced by a value other than the empty string.
	/// @note The output target @p out MAY differ from the output target of @p ctx.
	/// @param arg The exception to format.
	/// @param start The start of the subformat pattern.
	/// @param end The end (exclusive) of the subformat pattern.
	/// @param ctx The current `fmt::format_context`.
	/// @param out The ouput target.
	/// @param args The current formatting arguments.
	/// @param formatted Set to `true` if any output was produced.
	/// @return The iterator pointing at the last character of the sub pattern, i.e. the `]`. If not `]` exists, the result is @p end.
	std::string::const_iterator ProcessSubformat(const T& arg, const std::string::const_iterator& start, const std::string::const_iterator& end, fmt::format_context& ctx, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args, bool& formatted) const {
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
					fmt::basic_memory_buffer<char, 128> buf;
					std::back_insert_iterator<fmt::format_context::iterator::container_type> subOut(buf);
					if (Format(arg, start, it, ctx, subOut, args)) {
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
	/// @param out The ouput target.
	/// @param args The current formatting arguments.
	/// @return `true` if any output was produced.
	bool Format(const T& arg, std::string::const_iterator start, const std::string::const_iterator& end, fmt::format_context& ctx, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) const {
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
					ctx.on_error("exception specifier must argument identifier");
				} else if (*it >= '0' && *it <= '9') {
					unsigned id = 0;
					for (auto x = it; x != endOfArgId; ++x) {
						if (*it < '0' || *it > '9') {
							ctx.on_error("invalid argument id in exception specifier");
							break;
						}
						// check overflow
						if (id > std::numeric_limits<unsigned>::max() / 10) {
							ctx.on_error("argument id in exception specifier is too big");
							break;
						}
						id *= 10;
						id += *x - '0';
					}
					subArg = ctx.arg(id);
				} else {
					subArg = ctx.arg(std::string_view(&*it, std::distance(it, endOfArgId)));
				}
				const fmt::format_args subArgs(&subArg, 1);
				fmt::vformat_to<llamalog::EscapingFormatter>(out, fmt::to_string_view("{:" + std::string(startOfPattern, endOfPattern) + "}"), subArgs);

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
					formatted |= FormatTimestamp(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Thread
				case 't':
					formatted |= FormatThread(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// File
				case 'F':
					formatted |= FormatFile(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Line
				case 'L':
					formatted |= FormatLine(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Function
				case 'f':
					formatted |= FormatFunction(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Log Message
				case 'm':
					formatted |= FormatLogMessage(reinterpret_cast<const std::byte*>(&arg), out, args);
					break;
				// Exception Message
				case 'e':
					formatted |= FormatExceptionMessage(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Error Code
				case 'c':
					formatted |= FormatErrorCode(reinterpret_cast<const std::byte*>(&arg), out);
					break;
				// Category Name
				case 'C':
					formatted |= FormatCategoryName(reinterpret_cast<const std::byte*>(&arg), out);
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

private:
	/// @brief Format the exception timestamp.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatTimestamp(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		std::string str = LogWriter::FormatTimestamp(reinterpret_cast<const ExceptionInformation*>(ptr)->timestamp);
		std::copy(str.cbegin(), str.cend(), out);
		return true;
	}

	/// @brief SFINAE version for types which do not have a timestamp.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatTimestamp([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the thread id.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatThread(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		fmt::format_to(out, "{}", reinterpret_cast<const ExceptionInformation*>(ptr)->threadId);
		return true;
	}

	/// @brief SFINAE version for types which do not have a thread id.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatThread([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the file name.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatFile(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		std::string_view sv(reinterpret_cast<const ExceptionInformation*>(ptr)->szFile);
		std::copy(sv.cbegin(), sv.cend(), out);
		return true;
	}

	/// @brief SFINAE version for types which do not have a file name.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatFile([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the line number.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatLine(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		fmt::format_to(out, "{}", reinterpret_cast<const ExceptionInformation*>(ptr)->line);
		return true;
	}

	/// @brief SFINAE version for types which do not have a line number.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatLine([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the function name.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatFunction(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		std::string_view sv(reinterpret_cast<const ExceptionInformation*>(ptr)->szFunction);
		std::copy(sv.cbegin(), sv.cend(), out);
		return true;
	}

	/// @brief SFINAE version for types which do not have a function name.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatFunction([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the log message.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @param args The formatter arguments. The vector is populated on the first call.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatLogMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out, std::vector<fmt::format_context::format_arg>& args) {
		if (args.empty()) {
			// only copy once
			llamalog::CopyArgumentsFromBufferTo(GetBuffer(ptr), reinterpret_cast<const ExceptionInformation*>(ptr)->cbUsed, args);
		}
		fmt::vformat_to<llamalog::EscapingFormatter>(out, fmt::to_string_view(reinterpret_cast<const ExceptionInformation*>(ptr)->szMessage),
													 fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
		return true;
	}

	/// @brief SFINAE version for types which do not have a log message.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @param args The formatter arguments. The vector is populated on the first call.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatLogMessage([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out, [[maybe_unused]] std::vector<fmt::format_context::format_arg>& args) {
		return false;
	}

	/// @brief Format the exception message. @details This is the version for exception data having additional logging information.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static bool FormatExceptionMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		std::string_view sv(GetExceptionMessage(ptr), reinterpret_cast<const ExceptionInformation*>(ptr)->cbLength);
		std::string escaped = EscapeC(sv);
		if (escaped.empty()) {
			std::copy(sv.cbegin(), sv.cend(), out);
		} else {
			std::copy(escaped.cbegin(), escaped.cend(), out);
		}
		return true;
	}

	/// @brief Format the exception message. @details This is the version exceptions thrown using plain `throw`.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static bool FormatExceptionMessage(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		LogLine::Length length;
		std::memcpy(&length, &ptr[offsetof(T, length)], sizeof(length));

		std::string_view sv(reinterpret_cast<const char*>(&ptr[sizeof(T)]), length);
		std::string escaped = EscapeC(sv);
		if (escaped.empty()) {
			std::copy(sv.cbegin(), sv.cend(), out);
		} else {
			std::copy(escaped.cbegin(), escaped.cend(), out);
		}
		return true;
	}

	/// @brief SFINAE version for types which do not have an error code.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, HeapBasedException, PlainException>, int> = 0>
	static bool FormatErrorCode([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the error code for `std::system_error`s.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedSystemError, HeapBasedSystemError, PlainSystemError>, int> = 0>
	static bool FormatErrorCode(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		const std::byte* const systemError = GetSystemError(ptr);

		int code;
		std::memcpy(&code, &systemError[offsetof(T, code)], sizeof(code));

		fmt::format_to(out, "{}", code);
		return true;
	}

	/// @brief SFINAE version for types which do not have a category name.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `false`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, HeapBasedException, PlainException>, int> = 0>
	static bool FormatCategoryName([[maybe_unused]] const std::byte* __restrict const ptr, [[maybe_unused]] fmt::format_context::iterator& out) {
		return false;
	}

	/// @brief Format the category name for `std::system_error`s having additional logging data.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedSystemError, HeapBasedSystemError>, int> = 0>
	static bool FormatCategoryName(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		const std::byte* const systemError = GetSystemError(ptr);

		LogLine::Length length;
		std::memcpy(&length, &systemError[offsetof(T, length)], sizeof(length));

		std::string_view sv(reinterpret_cast<const char*>(&systemError[sizeof(T)]), length);
		std::string escaped = EscapeC(sv);
		if (escaped.empty()) {
			std::copy(sv.cbegin(), sv.cend(), out);
		} else {
			std::copy(escaped.cbegin(), escaped.cend(), out);
		}
		return true;
	}

	/// @brief Format the category name for `std::system_error`s thrown using plain `throw`.
	/// @tparam A local bound @p T for SFINAE expression.
	/// @param ptr The address of the exception argument in the buffer.
	/// @param out The output target.
	/// @return Always `true`.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainSystemError>, int> = 0>
	static bool FormatCategoryName(const std::byte* __restrict const ptr, fmt::format_context::iterator& out) {
		LogLine::Length length;
		std::memcpy(&length, &ptr[offsetof(T, length)], sizeof(length));

		LogLine::Length categoryLength;
		std::memcpy(&categoryLength, &ptr[offsetof(T, categoryLength)], sizeof(categoryLength));

		std::string_view sv(reinterpret_cast<const char*>(&ptr[sizeof(T) + length]), categoryLength);
		std::string escaped = EscapeC(sv);
		if (escaped.empty()) {
			std::copy(sv.cbegin(), sv.cend(), out);
		} else {
			std::copy(escaped.cbegin(), escaped.cend(), out);
		}
		return true;
	}

	/// @brief Helper to get the argument buffer.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the argument buffer.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const std::byte* GetBuffer(const std::byte* __restrict const ptr) noexcept {
		const std::byte* const buffer = reinterpret_cast<const std::byte*>(&reinterpret_cast<const ExceptionInformation*>(ptr)->padding);
		const LogLine::Size pos = reinterpret_cast<const ExceptionInformation*>(ptr)->cbLength * sizeof(char);
		const LogLine::Align padding = GetPadding(&buffer[pos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);
		return &buffer[pos + padding];
	}

	/// @brief Helper to get the argument buffer.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the argment buffer.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const std::byte* GetBuffer(const std::byte* __restrict const ptr) noexcept {
		return reinterpret_cast<const HeapBasedException*>(ptr)->pHeapBuffer;
	}

	/// @brief Helper to get the exception message.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the exception message.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, StackBasedException, StackBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
		return reinterpret_cast<const char*>(&reinterpret_cast<const ExceptionInformation*>(ptr)->padding);
	}

	/// @brief Helper to get the exception message.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the exception message.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, HeapBasedException, HeapBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
		return reinterpret_cast<const char*>(&ptr[sizeof(HeapBasedException)]);
	}

	/// @brief Helper to get the exception message.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the exception message.
	template <typename A = T, typename std::enable_if_t<is_any_v<A, PlainException, PlainSystemError>, int> = 0>
	static __declspec(restrict, noalias) const char* GetExceptionMessage(const std::byte* __restrict const ptr) noexcept {
		return reinterpret_cast<const char*>(&ptr[sizeof(T)]);
	}

	/// @brief Helper to get the information for `std::system_error`s.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the information block.
	template <typename A = T, typename std::enable_if_t<std::is_same_v<A, StackBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
		const std::byte* const buffer = GetBuffer(ptr);
		return &buffer[reinterpret_cast<const ExceptionInformation*>(ptr)->cbUsed];
	}

	/// @brief Helper to get the information for `std::system_error`s.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the information block.
	template <typename A = T, typename std::enable_if_t<std::is_same_v<A, HeapBasedSystemError>, int> = 0>
	static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
		const char* const msg = GetExceptionMessage(ptr);
		return reinterpret_cast<const std::byte*>(&msg[reinterpret_cast<const ExceptionInformation*>(ptr)->cbLength]);
	}

	/// @brief Helper to get the information for `std::system_error`s.
	/// @param ptr The address of the current exception argument.
	/// @return The address of the information block.
	template <typename A = T, typename std::enable_if_t<std::is_same_v<A, PlainSystemError>, int> = 0>
	static __declspec(restrict, noalias) const std::byte* GetSystemError(const std::byte* __restrict const ptr) noexcept {
		return ptr;
	}

private:
	std::string m_format;  ///< The original format pattern with the argument number (if any) removed.
};

}  // namespace
}  // namespace llamalog

/// @brief Specialization of a `fmt::formatter` to print `StackBasedException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::StackBasedException> : public llamalog::ExceptionFormatter<llamalog::StackBasedException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `StackBasedSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::StackBasedSystemError> : public llamalog::ExceptionFormatter<llamalog::StackBasedSystemError> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `HeapBasedException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::HeapBasedException> : public llamalog::ExceptionFormatter<llamalog::HeapBasedException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `HeapBasedSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::HeapBasedSystemError> : public llamalog::ExceptionFormatter<llamalog::HeapBasedSystemError> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `PlainException` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::PlainException> : public llamalog::ExceptionFormatter<llamalog::PlainException> {
	// empty
};

/// @brief Specialization of a `fmt::formatter` to print `PlainSystemError` arguments.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::PlainSystemError> : public llamalog::ExceptionFormatter<llamalog::PlainSystemError> {
	// empty
};

namespace llamalog {
namespace {

//
// Decoding Arguments
//

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// @tparam T The type of the argument.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T>
void DecodeArgument(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	// use std::memcpy to comply with strict aliasing
	T arg;
	std::memcpy(&arg, &buffer[position + sizeof(TypeId)], sizeof(arg));

	args.push_back(fmt::internal::make_arg<fmt::format_context>(arg));
	position += kTypeSize<T>;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const char*>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId)], sizeof(length));

	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	args.push_back(fmt::internal::make_arg<fmt::format_context>(std::string_view(reinterpret_cast<const char*>(&buffer[position + kTypeSize<const char*>]), length)));
	position += kTypeSize<const char*> + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for wide character strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const wchar_t*>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId)], sizeof(length));

	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[position + kTypeSize<const wchar_t*>]);

	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const InlineWideChar*>(&buffer[position + sizeof(TypeId)])));
	position += kTypeSize<const wchar_t*> + padding + length * sizeof(wchar_t);
}

/// @brief Decode a stack based exception from the buffer. @details The value of @p cbPosition is advanced after decoding.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @return The address of the exception object.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
_Ret_notnull_ const StackBasedException* DecodeStackBasedException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pException = reinterpret_cast<const StackBasedException*>(&buffer[offset]);
	const LogLine::Size messageOffset = offset + offsetof(StackBasedException, padding);
	const LogLine::Size bufferPos = messageOffset + pException->cbLength * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	position = bufferPos + bufferPadding + pException->cbUsed;
	return pException;
}

/// @brief Decode a heap based exception from the buffer. @details The value of @p position is advanced after decoding.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @return The address of the exception object.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
_Ret_notnull_ const HeapBasedException* DecodeHeapBasedException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pException = reinterpret_cast<const HeapBasedException*>(&buffer[offset]);

	position = offset + sizeof(HeapBasedException) + pException->cbLength * sizeof(char);
	return pException;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for `StackBasedException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<StackBasedException>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const StackBasedException* const pException = DecodeStackBasedException(buffer, position);
	args.push_back(fmt::internal::make_arg<fmt::format_context>(*pException));
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for `StackBasedSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<StackBasedSystemError>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const StackBasedException* const pException = DecodeStackBasedException(buffer, position);
	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const StackBasedSystemError*>(pException)));

	LogLine::Length length;
	std::memcpy(&length, &buffer[position + offsetof(StackBasedSystemError, length)], sizeof(length));

	position += sizeof(StackBasedSystemError) + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `HeapBasedException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<HeapBasedException>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const HeapBasedException* const pException = DecodeHeapBasedException(buffer, position);
	args.push_back(fmt::internal::make_arg<fmt::format_context>(*pException));
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `HeapBasedSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<HeapBasedSystemError>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const HeapBasedException* const pException = DecodeHeapBasedException(buffer, position);
	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const HeapBasedSystemError*>(pException)));

	LogLine::Length length;
	std::memcpy(&length, &buffer[position + offsetof(HeapBasedSystemError, length)], sizeof(length));

	position += sizeof(HeapBasedSystemError) + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `PlainException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<PlainException>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId) + offsetof(PlainException, length)], sizeof(length));

	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const PlainException*>(&buffer[position + sizeof(TypeId)])));
	position += kTypeSize<PlainException> + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `PlainSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<PlainSystemError>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, length)], sizeof(length));

	LogLine::Length categoryLength;
	std::memcpy(&categoryLength, &buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, categoryLength)], sizeof(categoryLength));

	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const PlainSystemError*>(&buffer[position + sizeof(TypeId)])));
	position += kTypeSize<PlainSystemError> + length * sizeof(char) + categoryLength * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<TriviallyCopyable>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &buffer[position + sizeof(TypeId)], sizeof(padding));

	CreateFormatArg createFormatArg;
	std::memcpy(&createFormatArg, &buffer[position + sizeof(TypeId) + sizeof(padding)], sizeof(createFormatArg));

	LogLine::Size size;
	std::memcpy(&size, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(createFormatArg)], sizeof(size));

	args.push_back(createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for non-trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<NonTriviallyCopyable>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &buffer[position + sizeof(TypeId)], sizeof(padding));

	CreateFormatArg createFormatArg;
	std::memcpy(&createFormatArg, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move) + sizeof(LogLine::Internal::Destruct)], sizeof(createFormatArg));

	LogLine::Size size;
	std::memcpy(&size, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move) + sizeof(LogLine::Internal::Destruct) + sizeof(createFormatArg)], sizeof(size));

	args.push_back(createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}


//
// Skipping Arguments
//

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @tparam The character type, i.e. either `char` or `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <typename T>
void SkipInlineString(_In_ const std::byte* __restrict buffer, _Inout_ LogLine::Size& position) noexcept;

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `char`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<char>(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId)], sizeof(length));

	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	position += kTypeSize<const char*> + length * sizeof(char);
}

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<wchar_t>(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId)], sizeof(length));

	const LogLine::Size offset = position + kTypeSize<const wchar_t*>;
	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[offset]);

	position += kTypeSize<const wchar_t*> + padding + length * sizeof(wchar_t);
}

/// @brief Skip a log argument of type `PlainException`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipPlainException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId) + offsetof(PlainException, length)], sizeof(length));

	position += kTypeSize<PlainException> + length * sizeof(char);
}

/// @brief Skip a log argument of type `PlainSystemError`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipPlainSystemError(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	LogLine::Length length;
	std::memcpy(&length, &buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, length)], sizeof(length));

	LogLine::Length categoryLength;
	std::memcpy(&categoryLength, &buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, categoryLength)], sizeof(categoryLength));

	position += kTypeSize<PlainSystemError> + length * sizeof(char) + categoryLength * sizeof(char);
}

/// @brief Skip a log argument of custom type.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipTriviallyCopyable(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &buffer[position + sizeof(TypeId)], sizeof(padding));

	LogLine::Size size;
	std::memcpy(&size, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(CreateFormatArg)], sizeof(size));

	position += kArgSize + padding + size;
}


//
// Copying Arguments
//

void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict src, _Out_writes_bytes_(cbUsed) std::byte* __restrict dst, LogLine::Size used);

/// @brief Copies a `StackBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyStackBasedException(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pSrcException = reinterpret_cast<const StackBasedException*>(&src[offset]);
	const LogLine::Size messageOffset = offset + offsetof(StackBasedException, padding);
	const LogLine::Size bufferPos = messageOffset + pSrcException->cbLength * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&src[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pSrcException->hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		// copy management data
		std::memcpy(&dst[offset], pSrcException, offsetof(StackBasedException, padding));

		// copy exception message
		std::memcpy(&dst[messageOffset], &src[messageOffset], pSrcException->cbLength * sizeof(char));

		// copy buffers
		CopyObjects(&src[bufferOffset], &dst[bufferOffset], pSrcException->cbUsed);
	} else {
		// copy everything in one turn
		std::memcpy(&dst[offset], pSrcException, offsetof(StackBasedException, padding) + pSrcException->cbLength * sizeof(char) + bufferPadding + pSrcException->cbUsed);
	}

	position = bufferPos + bufferPadding + pSrcException->cbUsed;
}

/// @brief Copies a `StackBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyStackBasedSystemError(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	CopyStackBasedException(src, dst, position);

	LogLine::Length length;
	std::memcpy(&length, &src[position + offsetof(StackBasedSystemError, length)], sizeof(length));

	const LogLine::Size size = sizeof(StackBasedSystemError) + length * sizeof(char);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Copies a `HeapBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyHeapBasedException(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pSrcException = reinterpret_cast<const HeapBasedException*>(&src[offset]);
	HeapBasedException* const pDstException = reinterpret_cast<HeapBasedException*>(&dst[offset]);

	std::memcpy(pDstException, pSrcException, sizeof(HeapBasedException) + pSrcException->cbLength * sizeof(char));

	std::unique_ptr<std::byte[]> heapBuffer = std::make_unique<std::byte[]>(pSrcException->cbUsed);
	pDstException->pHeapBuffer = heapBuffer.get();

	if (pSrcException->hasNonTriviallyCopyable) {
		// copy buffers
		CopyObjects(pSrcException->pHeapBuffer, pDstException->pHeapBuffer, pSrcException->cbUsed);
	} else {
		// copy everything in one turn
		std::memcpy(pDstException->pHeapBuffer, pSrcException->pHeapBuffer, sizeof(HeapBasedException) + pSrcException->cbUsed);
	}
	heapBuffer.release();

	position = offset + sizeof(HeapBasedException) + pSrcException->cbLength * sizeof(char);
}

/// @brief Copies a `HeapBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyHeapBasedSystemError(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	CopyHeapBasedException(src, dst, position);

	LogLine::Length length;
	std::memcpy(&length, &src[position + offsetof(HeapBasedSystemError, length)], sizeof(length));

	const LogLine::Size size = sizeof(HeapBasedSystemError) + length * sizeof(char);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Copies a custom type by calling construct (new) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyNonTriviallyCopyable(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &src[position + sizeof(TypeId)], sizeof(padding));

	LogLine::Internal::Copy copy;
	std::memcpy(&copy, &src[position + sizeof(TypeId) + sizeof(padding)], sizeof(copy));

	LogLine::Size size;
	std::memcpy(&size, &src[position + sizeof(TypeId) + sizeof(padding) + sizeof(copy) + sizeof(LogLine::Internal::Move) + sizeof(LogLine::Internal::Destruct) + sizeof(CreateFormatArg)], sizeof(size));

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogLine::Size offset = position + kArgSize + padding;
	copy(&src[offset], &dst[offset]);

	position = offset + size;
}


//
// Moving Arguments
//

void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict src, _Out_writes_bytes_(used) std::byte* __restrict dst, LogLine::Size used) noexcept;

/// @brief Moves a `StackBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveStackBasedException(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pSrcException = reinterpret_cast<const StackBasedException*>(&src[offset]);
	const LogLine::Size messageOffset = offset + offsetof(StackBasedException, padding);
	const LogLine::Size bufferPos = messageOffset + pSrcException->cbLength * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&src[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pSrcException->hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		// copy management data
		std::memcpy(&dst[offset], pSrcException, offsetof(StackBasedException, padding));

		// copy exception message
		std::memcpy(&dst[messageOffset], &src[messageOffset], pSrcException->cbLength * sizeof(char));

		// move buffers
		MoveObjects(&src[bufferOffset], &dst[bufferOffset], pSrcException->cbUsed);
	} else {
		// copy everything in one turn
		std::memcpy(&dst[offset], pSrcException, offsetof(StackBasedException, padding) + pSrcException->cbLength * sizeof(char) + bufferPadding + pSrcException->cbUsed);
	}

	position = bufferPos + bufferPadding + pSrcException->cbUsed;
}

/// @brief Moves a `StackBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveStackBasedSystemError(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	MoveStackBasedException(src, dst, position);

	LogLine::Length length;
	std::memcpy(&length, &src[position + offsetof(StackBasedSystemError, length)], sizeof(length));

	const LogLine::Size size = sizeof(StackBasedSystemError) + length * sizeof(char);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Moves a `HeapBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
__declspec(noalias) void MoveHeapBasedException(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pSrcException = reinterpret_cast<const HeapBasedException*>(&src[offset]);

	std::memcpy(&dst[offset], pSrcException, sizeof(HeapBasedException) + pSrcException->cbLength * sizeof(char));

	position = offset + sizeof(HeapBasedException) + pSrcException->cbLength * sizeof(char);
}

/// @brief Moves a `HeapBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
__declspec(noalias) void MoveHeapBasedSystemError(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	MoveHeapBasedException(src, dst, position);

	LogLine::Length length;
	std::memcpy(&length, &src[position + offsetof(HeapBasedSystemError, length)], sizeof(length));

	const LogLine::Size size = sizeof(HeapBasedSystemError) + length * sizeof(char);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Moves a custom type by calling construct (new) and destruct (old) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveNonTriviallyCopyable(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &src[position + sizeof(TypeId)], sizeof(padding));

	LogLine::Internal::Move move;
	std::memcpy(&move, &src[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy)], sizeof(move));

	LogLine::Internal::Destruct destruct;
	std::memcpy(&destruct, &src[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(move)], sizeof(destruct));

	LogLine::Size size;
	std::memcpy(&size, &src[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(move) + sizeof(destruct) + sizeof(CreateFormatArg)], sizeof(size));

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogLine::Size offset = position + kArgSize + padding;
	move(&src[offset], &dst[offset]);
	// and destruct the copied-from version
	destruct(&src[offset]);

	position = offset + size;
}


//
// Calling Argument Destructors
//

void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogLine::Size used) noexcept;

/// @brief Call the destructor of all `StackBasedException`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructStackBasedException(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pException = reinterpret_cast<const StackBasedException*>(&buffer[offset]);
	const LogLine::Size messageOffset = offset + offsetof(StackBasedException, padding);
	const LogLine::Size bufferPos = messageOffset + pException->cbLength * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pException->hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		CallDestructors(&buffer[bufferOffset], pException->cbUsed);
	}

	position = bufferPos + bufferPadding + pException->cbUsed;
}

/// @brief Call the destructor of all `StackBasedSystemError`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructStackBasedSystemError(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	DestructStackBasedException(buffer, position);

	LogLine::Length length;
	std::memcpy(&length, &buffer[position + offsetof(StackBasedSystemError, length)], sizeof(length));

	position += sizeof(StackBasedSystemError) + length * sizeof(char);
}

/// @brief Call the destructor of all `HeapBasedException`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructHeapBasedException(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pException = reinterpret_cast<const HeapBasedException*>(&buffer[offset]);

	if (pException->hasNonTriviallyCopyable) {
		CallDestructors(pException->pHeapBuffer, pException->cbUsed);
	}

	delete[] pException->pHeapBuffer;

	position = offset + sizeof(HeapBasedException) + pException->cbLength * sizeof(char);
}

/// @brief Call the destructor of all `HeapBasedSystemError`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructHeapBasedSystemError(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	DestructHeapBasedException(buffer, position);

	LogLine::Length length;
	std::memcpy(&length, &buffer[position + offsetof(HeapBasedSystemError, length)], sizeof(length));

	position += sizeof(HeapBasedSystemError) + length * sizeof(char);
}

/// @brief Call the destructor of a custom type and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructNonTriviallyCopyable(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	LogLine::Align padding;
	std::memcpy(&padding, &buffer[position + sizeof(TypeId)], sizeof(padding));

	LogLine::Internal::Destruct destruct;
	std::memcpy(&destruct, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move)], sizeof(destruct));

	LogLine::Size size;
	std::memcpy(&size, &buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(LogLine::Internal::Copy) + sizeof(LogLine::Internal::Move) + sizeof(destruct) + sizeof(CreateFormatArg)], sizeof(size));

	const LogLine::Size offset = position + kArgSize + padding;
	destruct(&buffer[offset]);

	position = offset + size;
}


//
// Buffer Management
//

/// @brief Copy arguments from a `LogInformation` buffer into a `std::vector`.
/// @param buffer The buffer holding the data.
/// @param used The number of valid bytes in @p buffer.
/// @param args The `std::vector` to receive the message arguments.
/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogLine::Size used, std::vector<fmt::basic_format_arg<fmt::format_context>>& args) {
	for (LogLine::Size position = 0; position < used;) {
		TypeId typeId;
		std::memcpy(&typeId, &buffer[position], sizeof(typeId));

		/// @cond hide
#pragma push_macro("DECODE_")
#define DECODE_(type_)                                 \
	case kTypeId<type_>:                               \
		DecodeArgument<type_>(args, buffer, position); \
		break
		/// @endcond

		switch (typeId) {
			DECODE_(bool);
			DECODE_(char);
			DECODE_(signed char);
			DECODE_(unsigned char);
			DECODE_(signed short);
			DECODE_(unsigned short);
			DECODE_(signed int);
			DECODE_(unsigned int);
			DECODE_(signed long);
			DECODE_(unsigned long);
			DECODE_(signed long long);
			DECODE_(unsigned long long);
			DECODE_(float);
			DECODE_(double);
			DECODE_(long double);
			DECODE_(const void*);
			DECODE_(const char*);
			DECODE_(const wchar_t*);
			DECODE_(StackBasedException);
			DECODE_(StackBasedSystemError);
			DECODE_(HeapBasedException);
			DECODE_(HeapBasedSystemError);
			DECODE_(PlainException);
			DECODE_(PlainSystemError);
			DECODE_(TriviallyCopyable);
			DECODE_(NonTriviallyCopyable);
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("DECODE_")
	}
}

/// @brief Copy all objects from one buffer to another.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict const src, _Out_writes_bytes_(cbUsed) std::byte* __restrict const dst, const LogLine::Size used) {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogLine::Size start;
	for (LogLine::Size position = start = 0; position < used;) {
		TypeId typeId;
		std::memcpy(&typeId, &src[position], sizeof(typeId));

		/// @cond hide
#pragma push_macro("SKIP_")
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(src, position);
			break;
		case kTypeId<StackBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyStackBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<StackBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyStackBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyHeapBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyHeapBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<PlainException>:
			SkipPlainException(src, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(src, position);
			break;
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

/// @brief Move all objects from one buffer to another. @details The function also calls the moved-from's destructor.
/// @details This function is used only if the buffer contains non-trivially copyable objects.
/// @param src The source buffer.
/// @param dst The target buffer.
/// @param used The number of bytes used in the buffer.
void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const LogLine::Size used) noexcept {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogLine::Size start;
	for (LogLine::Size position = start = 0; position < used;) {
		TypeId typeId;
		std::memcpy(&typeId, &src[position], sizeof(typeId));

		/// @cond hide
#pragma push_macro("SKIP_")
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(src, position);
			break;
		case kTypeId<StackBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveStackBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<StackBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveStackBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveHeapBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveHeapBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<PlainException>:
			SkipPlainException(src, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(src, position);
			break;
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

/// @brief Call all the destructors of non-trivially copyable custom arguments in a buffer.
/// @param buffer The buffer.
/// @param used The number of bytes used in the buffer.
void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogLine::Size used) noexcept {
	for (LogLine::Size position = 0; position < used;) {
		TypeId typeId;
		std::memcpy(&typeId, &buffer[position], sizeof(typeId));

		/// @cond hide
#pragma push_macro("SKIP_")
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
		/// @endcond

		switch (typeId) {
			SKIP_(bool);
			SKIP_(char);
			SKIP_(signed char);
			SKIP_(unsigned char);
			SKIP_(signed short);
			SKIP_(unsigned short);
			SKIP_(signed int);
			SKIP_(unsigned int);
			SKIP_(signed long);
			SKIP_(unsigned long);
			SKIP_(signed long long);
			SKIP_(unsigned long long);
			SKIP_(float);
			SKIP_(double);
			SKIP_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(buffer, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(buffer, position);
			break;
		case kTypeId<StackBasedException>:
			DestructStackBasedException(buffer, position);
			break;
		case kTypeId<StackBasedSystemError>:
			DestructStackBasedSystemError(buffer, position);
			break;
		case kTypeId<HeapBasedException>:
			DestructHeapBasedException(buffer, position);
			break;
		case kTypeId<HeapBasedSystemError>:
			DestructHeapBasedSystemError(buffer, position);
			break;
		case kTypeId<PlainException>:
			SkipPlainException(buffer, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(buffer, position);
			break;
		case kTypeId<TriviallyCopyable>:
			SkipTriviallyCopyable(buffer, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			DestructNonTriviallyCopyable(buffer, position);
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
	}
}

}  // namespace


// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): m_timestamp and m_stackBuffer need no initialization.
LogLine::LogLine(const Priority priority, _In_z_ const char* __restrict szFile, std::uint32_t line, _In_z_ const char* __restrict szFunction, _In_z_ const char* __restrict szMessage) noexcept
	: m_priority(priority)
	, m_szFile(szFile)
	, m_szFunction(szFunction)
	, m_szMessage(szMessage)
	, m_threadId(llamalog::GetCurrentThreadId())
	, m_line(line) {
	// ensure proper memory layout
	static_assert(sizeof(LogLine) == LLAMALOG_LOGLINE_SIZE, "size of LogLine");
	static_assert(offsetof(LogLine, m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0, "alignment of LogLine::m_stackBuffer");

	static_assert(offsetof(LogLine, m_stackBuffer) == 0, "offset of m_stackBuffer");
#if UINTPTR_MAX == UINT64_MAX
	static_assert(offsetof(LogLine, m_priority) == LLAMALOG_LOGLINE_SIZE - 58, "offset of m_priority");
	static_assert(offsetof(LogLine, m_hasNonTriviallyCopyable) == LLAMALOG_LOGLINE_SIZE - 57, "offset of m_hasNonTriviallyCopyable");
	static_assert(offsetof(LogLine, m_timestamp) == LLAMALOG_LOGLINE_SIZE - 56, "offset of m_timestamp");
	static_assert(offsetof(LogLine, m_szFile) == LLAMALOG_LOGLINE_SIZE - 48, "offset of m_szFile");
	static_assert(offsetof(LogLine, m_szFunction) == LLAMALOG_LOGLINE_SIZE - 40, "offset of m_szFunction");
	static_assert(offsetof(LogLine, m_szMessage) == LLAMALOG_LOGLINE_SIZE - 32, "offset of m_szMessage");
	static_assert(offsetof(LogLine, m_threadId) == LLAMALOG_LOGLINE_SIZE - 24, "offset of m_threadId");
	static_assert(offsetof(LogLine, m_line) == LLAMALOG_LOGLINE_SIZE - 20, "offset of m_line");
	static_assert(offsetof(LogLine, m_cbUsed) == LLAMALOG_LOGLINE_SIZE - 16, "offset of m_cbUsed");
	static_assert(offsetof(LogLine, m_cbSize) == LLAMALOG_LOGLINE_SIZE - 12, "offset of m_cbSize");
	static_assert(offsetof(LogLine, m_heapBuffer) == LLAMALOG_LOGLINE_SIZE - 8, "offset of m_heapBuffer");

	static_assert(sizeof(ExceptionInformation) == 48);
	static_assert(offsetof(ExceptionInformation, cbLength) == 44, "offset of hasNonTriviallyCopyable");
	static_assert(offsetof(ExceptionInformation, hasNonTriviallyCopyable) == 46, "offset of hasNonTriviallyCopyable");
	static_assert(sizeof(StackBasedException) == 48);
	static_assert(sizeof(HeapBasedException) == 56);
	static_assert(offsetof(HeapBasedException, pHeapBuffer) == 48, "offset of pHeapBuffer");
#elif UINTPTR_MAX == UINT32_MAX
	static_assert(offsetof(LogLine, m_priority) == LLAMALOG_LOGLINE_SIZE - 42, "offset of m_priority");
	static_assert(offsetof(LogLine, m_hasNonTriviallyCopyable) == LLAMALOG_LOGLINE_SIZE - 41, "offset of m_hasNonTriviallyCopyable");
	static_assert(offsetof(LogLine, m_timestamp) == LLAMALOG_LOGLINE_SIZE - 40, "offset of m_timestamp");
	static_assert(offsetof(LogLine, m_szFile) == LLAMALOG_LOGLINE_SIZE - 32, "offset of m_szFile");
	static_assert(offsetof(LogLine, m_szFunction) == LLAMALOG_LOGLINE_SIZE - 28, "offset of m_szFunction");
	static_assert(offsetof(LogLine, m_szMessage) == LLAMALOG_LOGLINE_SIZE - 24, "offset of m_szMessage");
	static_assert(offsetof(LogLine, m_threadId) == LLAMALOG_LOGLINE_SIZE - 20, "offset of m_threadId");
	static_assert(offsetof(LogLine, m_line) == LLAMALOG_LOGLINE_SIZE - 16, "offset of m_line");
	static_assert(offsetof(LogLine, m_cbUsed) == LLAMALOG_LOGLINE_SIZE - 12, "offset of m_cbUsed");
	static_assert(offsetof(LogLine, m_cbSize) == LLAMALOG_LOGLINE_SIZE - 8, "offset of m_cbSize");
	static_assert(offsetof(LogLine, m_heapBuffer) == LLAMALOG_LOGLINE_SIZE - 4, "offset of m_heapBuffer");

	static_assert(sizeof(ExceptionInformation) == 36);
	static_assert(offsetof(ExceptionInformation, cbLength) == 32, "offset of hasNonTriviallyCopyable");
	static_assert(offsetof(ExceptionInformation, hasNonTriviallyCopyable) == 34, "offset of hasNonTriviallyCopyable");
	static_assert(sizeof(StackBasedException) == 36);
	static_assert(sizeof(HeapBasedException) == 40);
	static_assert(offsetof(HeapBasedException, pHeapBuffer) == 36, "offset of pHeapBuffer");
#elif
	static_assert(false, "layout assertions not defined");
#endif

	// Struct for arguments
	static_assert(offsetof(ExceptionInformation, timestamp) == offsetof(LogLine, m_timestamp) - offsetof(LogLine, m_timestamp), "offset of timestamp");
	static_assert(offsetof(ExceptionInformation, szFile) == offsetof(LogLine, m_szFile) - offsetof(LogLine, m_timestamp), "offset of szFile");
	static_assert(offsetof(ExceptionInformation, szFunction) == offsetof(LogLine, m_szFunction) - offsetof(LogLine, m_timestamp), "offset of szFunction");
	static_assert(offsetof(ExceptionInformation, szMessage) == offsetof(LogLine, m_szMessage) - offsetof(LogLine, m_timestamp), "offset of szMessage");
	static_assert(offsetof(ExceptionInformation, threadId) == offsetof(LogLine, m_threadId) - offsetof(LogLine, m_timestamp), "offset of threadId");
	static_assert(offsetof(ExceptionInformation, line) == offsetof(LogLine, m_line) - offsetof(LogLine, m_timestamp), "offset of line");
	static_assert(offsetof(ExceptionInformation, cbUsed) == offsetof(LogLine, m_cbUsed) - offsetof(LogLine, m_timestamp), "offset of cbUsed");
	static_assert(offsetof(ExceptionInformation, padding) == offsetof(ExceptionInformation, hasNonTriviallyCopyable) + sizeof(ExceptionInformation::hasNonTriviallyCopyable), "offset of padding");
	static_assert(offsetof(ExceptionInformation, padding) == sizeof(ExceptionInformation) - sizeof(ExceptionInformation::padding), "length of padding");

	static_assert(offsetof(StackBasedException, hasNonTriviallyCopyable) == offsetof(ExceptionInformation, hasNonTriviallyCopyable), "offset of hasNonTriviallyCopyable");
	static_assert(offsetof(HeapBasedException, hasNonTriviallyCopyable) == offsetof(ExceptionInformation, hasNonTriviallyCopyable), "offset of hasNonTriviallyCopyable");
}

LogLine::LogLine(const LogLine& logLine)
	: m_priority(logLine.m_priority)
	, m_hasNonTriviallyCopyable(logLine.m_hasNonTriviallyCopyable)
	, m_szFile(logLine.m_szFile)
	, m_szFunction(logLine.m_szFunction)
	, m_szMessage(logLine.m_szMessage)
	, m_timestamp(logLine.m_timestamp)
	, m_threadId(logLine.m_threadId)
	, m_line(logLine.m_line)
	, m_cbUsed(logLine.m_cbUsed)
	, m_cbSize(logLine.m_cbSize) {
	if (logLine.m_heapBuffer) {
		m_heapBuffer = std::make_unique<std::byte[]>(m_cbUsed);
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_heapBuffer.get(), m_heapBuffer.get(), m_cbUsed);
		} else {
			std::memcpy(m_heapBuffer.get(), logLine.m_heapBuffer.get(), m_cbUsed);
		}
	} else {
		if (m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.m_stackBuffer, m_stackBuffer, m_cbUsed);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_cbUsed);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): initialization of m_stackBuffer depends on data.
LogLine::LogLine(LogLine&& logLine) noexcept
	: m_priority(logLine.m_priority)
	, m_hasNonTriviallyCopyable(logLine.m_hasNonTriviallyCopyable)
	, m_szFile(logLine.m_szFile)
	, m_szFunction(logLine.m_szFunction)
	, m_szMessage(logLine.m_szMessage)
	, m_timestamp(logLine.m_timestamp)
	, m_threadId(logLine.m_threadId)
	, m_line(logLine.m_line)
	, m_cbUsed(logLine.m_cbUsed)
	, m_cbSize(logLine.m_cbSize)
	, m_heapBuffer(std::move(logLine.m_heapBuffer)) {
	if (!m_heapBuffer) {
		if (m_hasNonTriviallyCopyable) {
			MoveObjects(logLine.m_stackBuffer, m_stackBuffer, m_cbUsed);
		} else {
			std::memcpy(m_stackBuffer, logLine.m_stackBuffer, m_cbUsed);
		}
	}
	// leave source in a consistent state
	logLine.m_cbUsed = 0;
	logLine.m_cbSize = sizeof(m_stackBuffer);
}

LogLine::~LogLine() noexcept {
	if (m_hasNonTriviallyCopyable) {
		CallDestructors(GetBuffer(), m_cbUsed);
	}
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const bool arg) {
	Write(arg);
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

// Based on `NanoLogLine::operator<<(char)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned char arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed short arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned short arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed int arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned int arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint32_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(int64_t)` from NanoLog.
LogLine& LogLine::operator<<(const signed long long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(const unsigned long long arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const float arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const double arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(double)` from NanoLog.
LogLine& LogLine::operator<<(const long double arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_ const void* __restrict const arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_ const std::nullptr_t arg) {
	Write(static_cast<const void*>(arg));
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const char* __restrict const arg) {
	if (arg) {
		WriteString(arg, std::strlen(arg));
	} else {
		Write<const void*>(nullptr);
	}
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const wchar_t* __restrict const arg) {
	if (arg) {
		WriteString(arg, std::wcslen(arg));
	} else {
		Write<const void*>(nullptr);
	}
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
	const std::system_error* pSystemError;
	const BaseException* const pBaseException = GetCurrentExceptionAsBaseException(pSystemError);

	if (pSystemError) {
		const std::error_code& code = pSystemError->code();
		WriteException(arg.what(), pBaseException, true, code.value(), code.category().name());
	} else {
		WriteException(arg.what(), pBaseException, false, 0, nullptr);
	}
	return *this;
}

/// @brief The single specialization of `CopyArgumentsTo`.
/// @param args The `std::vector` to receive the message arguments.
template <>
void LogLine::CopyArgumentsTo<std::vector<fmt::basic_format_arg<fmt::format_context>>>(std::vector<fmt::basic_format_arg<fmt::format_context>>& args) const {
	CopyArgumentsFromBufferTo(GetBuffer(), m_cbUsed, args);
}

std::string LogLine::GetLogMessage() const {
	std::vector<fmt::basic_format_arg<fmt::format_context>> args;
	CopyArgumentsFromBufferTo(GetBuffer(), m_cbUsed, args);

	fmt::basic_memory_buffer<char, 256> buf;
	fmt::vformat_to<EscapingFormatter>(buf, fmt::to_string_view(GetPattern()),
									   fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::basic_format_args<fmt::format_context>::size_type>(args.size())));
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
	const std::size_t requiredSize = static_cast<std::size_t>(m_cbUsed) + additionalBytes;
	if (requiredSize > std::numeric_limits<LogLine::Size>::max()) {
		LLAMALOG_THROW(std::length_error("m_size"), "Buffer too big for {} more bytes: {}", additionalBytes, requiredSize);
	}

	if (requiredSize <= m_cbSize) {
		return !m_heapBuffer ? &m_stackBuffer[m_cbUsed] : &(m_heapBuffer.get())[m_cbUsed];
	}

	m_cbSize = GetNextChunk(static_cast<std::uint32_t>(requiredSize));
	if (!m_heapBuffer) {
		m_heapBuffer = std::make_unique<std::byte[]>(m_cbSize);

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_stackBuffer) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			MoveObjects(m_stackBuffer, m_heapBuffer.get(), m_cbUsed);
		} else {
			std::memcpy(m_heapBuffer.get(), m_stackBuffer, m_cbUsed);
		}
	} else {
		std::unique_ptr<std::byte[]> newHeapBuffer(std::make_unique<std::byte[]>(m_cbSize));

		// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
		assert(reinterpret_cast<std::uintptr_t>(m_heapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(newHeapBuffer.get()) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

		if (m_hasNonTriviallyCopyable) {
			MoveObjects(m_heapBuffer.get(), newHeapBuffer.get(), m_cbUsed);
		} else {
			std::memcpy(newHeapBuffer.get(), m_heapBuffer.get(), m_cbUsed);
		}
		m_heapBuffer = std::move(newHeapBuffer);
	}
	return &(m_heapBuffer.get())[m_cbUsed];
}

// Derived from both methods `NanoLogLine::encode` from NanoLog.
template <typename T>
void LogLine::Write(T arg) {
	constexpr TypeId kArgTypeId = kTypeId<T>;
	constexpr auto kArgSize = kTypeSize<T>;
	std::byte* __restrict const buffer = GetWritePosition(kArgSize);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &arg, sizeof(arg));

	m_cbUsed += kArgSize;
}

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
void LogLine::WriteString(_In_reads_(len) const char* __restrict const arg, const std::size_t len) {
	constexpr TypeId kArgTypeId = kTypeId<const char*>;
	constexpr auto kArgSize = kTypeSize<const char*>;
	const LogLine::Length length = static_cast<LogLine::Length>(std::min<std::size_t>(len, std::numeric_limits<LogLine::Length>::max()));
	if (length < len) {
		LOG_WARN_INTERNAL("String of length {} trimmed to {}", len, length);
	}
	const LogLine::Size size = kArgSize + length * sizeof(char);

	std::byte* __restrict buffer = GetWritePosition(size);
	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize], arg, length * sizeof(char));

	m_cbUsed += size;
}

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
void LogLine::WriteString(_In_reads_(len) const wchar_t* __restrict const arg, const std::size_t len) {
	constexpr TypeId kArgTypeId = kTypeId<const wchar_t*>;
	constexpr auto kArgSize = kTypeSize<const wchar_t*>;
	const LogLine::Length length = static_cast<LogLine::Length>(std::min<std::size_t>(len, std::numeric_limits<LogLine::Length>::max()));
	if (length < len) {
		LOG_WARN_INTERNAL("String of length {} trimmed to {}", len, length);
	}
	const LogLine::Size size = kArgSize + length * sizeof(wchar_t);

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[kArgSize]);
	if (padding) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_cbSize - m_cbUsed >= size + padding);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &length, sizeof(length));
	std::memcpy(&buffer[kArgSize + padding], arg, length * sizeof(wchar_t));

	m_cbUsed += size + padding;
}

void LogLine::WriteException(_In_z_ const char* message, _In_opt_ const BaseException* pBaseException, bool isSystemError, int code, _In_opt_z_ const char* category) {
	const std::size_t messageLen = std::strlen(message);
	// silenty trim message to size
	const LogLine::Length messageLength = static_cast<LogLine::Length>(std::min<std::size_t>(messageLen, std::numeric_limits<LogLine::Length>::max()));
	if (messageLength < messageLen) {
		LOG_WARN_INTERNAL("Exception message of length {} trimmed to {}", messageLen, messageLength);
	}

	LogLine::Length categoryLength;
	if (isSystemError) {
		const std::size_t categoryLen = std::strlen(category);
		// silenty trim category to size
		categoryLength = static_cast<LogLine::Length>(std::min<std::size_t>(categoryLen, std::numeric_limits<LogLine::Length>::max()));
		if (categoryLength < categoryLen) {
			LOG_WARN_INTERNAL("Exception message of length {} trimmed to {}", categoryLen, categoryLength);
		}
	} else {
		categoryLength = 0;
	}

	static_assert(alignof(char) == 1, "alignment of char");  // no padding required for message

	if (!pBaseException) {
		if (!isSystemError) {
			constexpr TypeId kArgTypeId = kTypeId<PlainException>;
			constexpr auto kArgSize = kTypeSize<PlainException>;

			const LogLine::Size size = kArgSize + messageLength * sizeof(char);
			std::byte* __restrict const buffer = GetWritePosition(size);

			std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));

			std::memcpy(&buffer[sizeof(kArgTypeId) + offsetof(PlainException, length)], &messageLength, sizeof(messageLength));
			std::memcpy(&buffer[kArgSize], message, messageLength);

			m_cbUsed += kArgSize + messageLength;
		} else {
			constexpr TypeId kArgTypeId = kTypeId<PlainSystemError>;
			constexpr auto kArgSize = kTypeSize<PlainSystemError>;

			const LogLine::Size size = kArgSize + messageLength * sizeof(char) + categoryLength * sizeof(char);
			std::byte* __restrict const buffer = GetWritePosition(size);

			std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
			std::memcpy(&buffer[sizeof(kArgTypeId) + offsetof(PlainSystemError, code)], &code, sizeof(code));

			std::memcpy(&buffer[sizeof(kArgTypeId) + offsetof(PlainSystemError, length)], &messageLength, sizeof(messageLength));
			std::memcpy(&buffer[kArgSize], message, messageLength);

			std::memcpy(&buffer[sizeof(kArgTypeId) + offsetof(PlainSystemError, categoryLength)], &categoryLength, sizeof(categoryLength));
			std::memcpy(&buffer[kArgSize + messageLength], category, categoryLength);

			m_cbUsed += kArgSize + messageLength + categoryLength;
		}
		return;
	}

	const LogLine& logLine = pBaseException->m_logLine;
	if (!logLine.m_heapBuffer) {
		const TypeId kArgTypeId = isSystemError ? kTypeId<StackBasedSystemError> : kTypeId<StackBasedException>;
		const auto kArgSize = isSystemError ? kTypeSize<StackBasedSystemError> : kTypeSize<StackBasedException>;
		const LogLine::Size size = kArgSize + messageLength * sizeof(char) + logLine.m_cbUsed + categoryLength * sizeof(char);
		std::byte* __restrict buffer = GetWritePosition(size);
		const LogLine::Size pos = sizeof(kArgTypeId);
		const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
		const LogLine::Size offset = pos + padding;
		if (padding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding);
		}

		const LogLine::Size messageOffset = offset + offsetof(StackBasedException, padding);

		const LogLine::Size bufferPos = messageOffset + messageLength * sizeof(char);
		const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);
		if (bufferPadding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding + bufferPadding);
		}
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
		std::memcpy(&buffer[offset], &logLine.m_timestamp, offsetof(StackBasedException, cbLength));
		std::memcpy(&buffer[offset + offsetof(StackBasedException, cbLength)], &messageLength, sizeof(messageLength));
		std::memcpy(&buffer[offset + offsetof(StackBasedException, hasNonTriviallyCopyable)], &logLine.m_hasNonTriviallyCopyable, sizeof(bool));
		std::memcpy(&buffer[messageOffset], message, messageLength);

		if (logLine.m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.GetBuffer(), &buffer[bufferOffset], logLine.m_cbUsed);
		} else {
			std::memcpy(&buffer[bufferOffset], logLine.GetBuffer(), logLine.m_cbUsed);
		}

		const LogLine::Size nextOffset = bufferOffset + logLine.m_cbUsed;
		assert(nextOffset + (isSystemError ? sizeof(StackBasedSystemError) : 0) + categoryLength * sizeof(char) == size + padding + bufferPadding);
		if (isSystemError) {
			std::memcpy(&buffer[nextOffset + offsetof(StackBasedSystemError, code)], &code, sizeof(code));
			std::memcpy(&buffer[nextOffset + offsetof(StackBasedSystemError, length)], &categoryLength, sizeof(categoryLength));
			std::memcpy(&buffer[nextOffset + sizeof(StackBasedSystemError)], category, categoryLength);
			m_cbUsed += nextOffset + sizeof(StackBasedSystemError) + categoryLength * sizeof(char);
		} else {
			m_cbUsed += nextOffset;
		}
	} else {
		const TypeId kArgTypeId = isSystemError ? kTypeId<HeapBasedSystemError> : kTypeId<HeapBasedException>;
		const auto kArgSize = isSystemError ? kTypeSize<HeapBasedSystemError> : kTypeSize<HeapBasedException>;
		const LogLine::Size size = kArgSize + messageLength * sizeof(char) + categoryLength * sizeof(char);

		std::byte* __restrict buffer = GetWritePosition(size);
		const LogLine::Size pos = sizeof(kArgTypeId);
		const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
		const LogLine::Size offset = pos + padding;
		if (padding != 0) {
			// check if the buffer has enough space for the type AND the padding
			buffer = GetWritePosition(size + padding);
		}

		const LogLine::Size messageOffset = offset + sizeof(HeapBasedException);

		std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
		std::memcpy(&buffer[offset], &logLine.m_timestamp, offsetof(HeapBasedException, cbLength));
		std::memcpy(&buffer[offset + offsetof(HeapBasedException, cbLength)], &messageLength, sizeof(messageLength));
		std::memcpy(&buffer[offset + offsetof(HeapBasedException, hasNonTriviallyCopyable)], &logLine.m_hasNonTriviallyCopyable, sizeof(bool));
		std::memcpy(&buffer[messageOffset], message, messageLength);

		std::unique_ptr<std::byte[]> heapBuffer = std::make_unique<std::byte[]>(logLine.m_cbUsed);
		std::byte* const pBuffer = heapBuffer.get();
		std::memcpy(&buffer[offset + offsetof(HeapBasedException, pHeapBuffer)], &pBuffer, sizeof(pBuffer));
		if (logLine.m_hasNonTriviallyCopyable) {
			CopyObjects(logLine.GetBuffer(), pBuffer, logLine.m_cbUsed);
		} else {
			std::memcpy(pBuffer, logLine.GetBuffer(), logLine.m_cbUsed);
		}
		heapBuffer.release();

		const LogLine::Size nextOffset = messageOffset + messageLength * sizeof(char);
		assert(nextOffset + (isSystemError ? sizeof(HeapBasedSystemError) : 0) + categoryLength * sizeof(char) == size + padding);
		if (isSystemError) {
			std::memcpy(&buffer[nextOffset + offsetof(HeapBasedSystemError, code)], &code, sizeof(code));
			std::memcpy(&buffer[nextOffset + offsetof(HeapBasedSystemError, length)], &categoryLength, sizeof(categoryLength));
			std::memcpy(&buffer[nextOffset + sizeof(HeapBasedSystemError)], category, categoryLength);
			m_cbUsed += nextOffset + sizeof(HeapBasedSystemError) + categoryLength * sizeof(char);
		} else {
			m_cbUsed += nextOffset;
		}
	}
	m_hasNonTriviallyCopyable = true;
}

void LogLine::WriteTriviallyCopyable(_In_reads_bytes_(objectSize) const std::byte* __restrict const ptr, const LogLine::Size objectSize, const LogLine::Align align, _In_ void (*const createFormatArg)()) {
	static_assert(sizeof(createFormatArg) == sizeof(CreateFormatArg));

	constexpr TypeId kArgTypeId = kTypeId<TriviallyCopyable>;
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;
	const LogLine::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding != 0) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_cbSize - m_cbUsed >= size + padding);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding)], &createFormatArg, sizeof(createFormatArg));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(createFormatArg)], &objectSize, sizeof(objectSize));
	std::memcpy(&buffer[kArgSize + padding], ptr, objectSize);

	m_cbUsed += size + padding;
}

__declspec(restrict) std::byte* LogLine::WriteNonTriviallyCopyable(const LogLine::Size objectSize, const LogLine::Align align, _In_ const Copy copy, _In_ const Move move, _In_ const Destruct destruct, _In_ void (*const createFormatArg)()) {
	static_assert(sizeof(createFormatArg) == sizeof(CreateFormatArg));

	constexpr TypeId kArgTypeId = kTypeId<NonTriviallyCopyable>;
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;
	const LogLine::Size size = kArgSize + objectSize;

	std::byte* __restrict buffer = GetWritePosition(size);
	const LogLine::Align padding = GetPadding(&buffer[kArgSize], align);
	if (padding != 0) {
		// check if the buffer has enough space for the type AND the padding
		buffer = GetWritePosition(size + padding);
	}
	assert(m_cbSize - m_cbUsed >= size + padding);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding)], &copy, sizeof(copy));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(copy)], &move, sizeof(move));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(copy) + sizeof(move)], &destruct, sizeof(destruct));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(copy) + sizeof(move) + sizeof(destruct)], &createFormatArg, sizeof(createFormatArg));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(copy) + sizeof(move) + sizeof(destruct) + sizeof(createFormatArg)], &objectSize, sizeof(objectSize));
	std::byte* result = &buffer[kArgSize + padding];

	m_hasNonTriviallyCopyable = true;
	m_cbUsed += size + padding;

	return result;
}

}  // namespace llamalog

#ifdef __clang_analyzer__
#pragma pop_macro("offsetof")
#endif
