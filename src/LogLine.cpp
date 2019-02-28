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

#include <fmt/format.h>

#include <windows.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace llamalog {

namespace {

/// @brief The number of bytes to add to the argument buffer after it became too small.
constexpr std::size_t kGrowBytes = 512;


/// @brief The types supported by the logger as arguments. Use `#kTypeId` to get the `#TypeId`.
/// @copyright This type is derived from `SupportedTypes` from NanoLog.
using Types = std::tuple<
	bool,
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
	const void*,                                // MUST NOT cast this back to any object because the object might no longer exist when the message is logged
	const char*,                                // string is stored WITHOUT a terminating null character
	const wchar_t*,                             // string is stored WITHOUT a terminating null character
	fmt::basic_format_arg<fmt::format_context>  // length DOES include padding because raw data is opaque to logger
	>;

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
static_assert(std::tuple_size_v<Types> <= UINT8_MAX, "too many types for uint8_t");

/// @brief A constant to get the `#TypeId` of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
constexpr TypeId kTypeId = TypeIndex<T, Types>::kValue;


/// @brief Pre-calculated array of sizes required to store values in the buffer. Use `#kSize` to get the size in code.
/// @hideinitializer
constexpr std::uint8_t kSizes[] = {
	sizeof(TypeId) + sizeof(bool),
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
	sizeof(TypeId) + sizeof(std::size_t) /* + padding + std::strlen(str) */,
	sizeof(TypeId) + sizeof(std::size_t) /* + padding + std::wcslen(str) */,
	sizeof(TypeId) + sizeof(std::uint8_t) + sizeof(fmt::basic_format_arg<fmt::format_context> (*)(const std::byte* const)) + sizeof(std::size_t) /* + padding + sizeof(arg) */
};
static_assert(sizeof(TypeId) + sizeof(std::uint8_t) + sizeof(fmt::basic_format_arg<fmt::format_context>(*)(const std::byte* const)) + sizeof(std::size_t) <= UINT8_MAX, "type for sizes is too small");
static_assert(std::tuple_size_v<Types> == sizeof(kSizes) / sizeof(kSizes[0]), "length of array of sizes does not match number of types");

/// @brief A constant to get the (basic) buffer size of a type at compile time.
/// @tparam T The type to get the id for.
template <typename T>
constexpr std::uint8_t kSize = kSizes[kTypeId<T>];

/// @brief Get the required padding for a type starting at the next possible offset.
/// @tparam T The type.
/// @param ptr The target address.
/// @return The padding to account for a properly aligned type.
template <typename T>
std::size_t GetPadding(_In_ const std::byte* __restrict const ptr) noexcept {
	static constexpr std::size_t kMask = alignof(T) - 1;
	return (alignof(T) - static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr) & kMask)) & kMask;
}

/// @brief Get the required padding for a type starting at the next possible offset.
/// @param ptr The target address.
/// @param cbAlign The required alignment in number of bytes.
/// @return The padding to account for a properly aligned type.
std::size_t GetPadding(_In_ const std::byte* __restrict const ptr, const std::size_t cbAlign) noexcept {
	const std::size_t mask = cbAlign - 1;
	return (cbAlign - static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(ptr) & mask)) & mask;
}


/// @brief Get the next power of 2 value equal to or greater than value. @details The function does not handle the cases
/// for @p value equal to either `0` or `1` correctly, but this is not required in this class.
/// @param value The value.
/// @return The next power of 2.
std::size_t GetNextPowerOf2(const std::size_t value) noexcept {
	// but we don't mind because this won't happen, saves some cycles and a check of result of _BitScanReverse
	assert(value > 1);

	DWORD index;
#if UINTPTR_MAX == UINT64_MAX
	_BitScanReverse64(&index, value - 1);
	return 1ULL << (index + 1);
#elif UINTPTR_MAX == UINT32_MAX
	_BitScanReverse(&index, value - 1);
	return 1U << (index + 1);
#elif
	static_assert(false, "next power of 2 not defined");
#endif
}


/// @brief Get the id of the current thread.
/// @return The id of the current thread.
/// @copyright The function is based on `this_thread_id` from NanoLog.
DWORD GetCurrentThreadId() noexcept {
	static const thread_local DWORD kId = ::GetCurrentThreadId();
	return kId;
}


/// @brief Escape a string according to C escaping rules.
/// @warning If no escaping is needed, an empty string is returned for performance reasons.
/// @param sv The input value.
/// @return The escaped result or an empty string.
std::string EscapeC(const std::string_view& sv) {
	static constexpr const char kHexDigits[] = "0123456789ABCDEF";

	std::string result;
	auto begin = sv.cbegin();
	const auto end = sv.cend();

	for (auto it = begin; it != end; ++it) {
		const std::uint8_t c = *it;
		if (c == '"' || c == '\\' || c < 0x20 || c > 0x7f) {
			if (result.empty()) {
				const std::size_t len = sv.length();
				const std::size_t extra = std::distance(end, it) / 4;
				result.reserve(len + max(extra, 2));
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
using ArgFormatter = fmt::arg_formatter<fmt::back_insert_range<fmt::internal::buffer>>;

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
	auto operator()(char value) {
		const fmt::format_specs* const specs = spec();
		if ((specs == nullptr || specs->type == '\0') && (value < 0x20 || value > 0x7f)) {
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
	auto operator()(const char* value) {
		const fmt::format_specs* const specs = spec();
		if (specs == nullptr || specs->type == '\0') {
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
	auto operator()(fmt::basic_string_view<arg_formatter_base::char_type> value) {
		const fmt::format_specs* const specs = spec();
		if (specs == nullptr || specs->type == '\0') {
			std::string_view sv(value.data(), value.size());
			std::string str = EscapeC(sv);
			if (!str.empty()) {
				return ArgFormatter::operator()(str);
			}
		}
		return ArgFormatter::operator()(value);
	}
};

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// @tparam T The type of the argument.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param cbPosition The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T>
void DecodeArgument(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ std::size_t& cbPosition) noexcept {
	// use std::memcpy to comply with strict aliasing
	T arg;
	std::memcpy(&arg, &buffer[cbPosition + sizeof(TypeId)], sizeof(arg));

	args.push_back(fmt::internal::make_arg<fmt::format_context>(arg));
	cbPosition += kSize<T>;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param cbPosition The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const char*>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ std::size_t& cbPosition) noexcept {
	std::size_t cchLength;
	std::memcpy(&cchLength, &buffer[cbPosition + sizeof(TypeId)], sizeof(cchLength));

	const std::size_t cbOffset = cbPosition + kSize<const char*>;
	const std::size_t cbPadding = GetPadding<const char*>(&buffer[cbOffset]);
	const std::byte* const data = &buffer[cbOffset + cbPadding];
	assert((reinterpret_cast<std::uintptr_t>(data) & (alignof(const char*) - 1)) == 0);

	args.push_back(fmt::internal::make_arg<fmt::format_context>(std::string_view(reinterpret_cast<const char*>(data), cchLength)));
	cbPosition += kSize<const char*> + cbPadding + cchLength * sizeof(char);
}


/// @brief Helper class to pass a wide character string stored inline in the buffer to the formatter.
struct InlineWideChar {
	std::byte pos;  ///< The address of this variable is placed above the address of the first character.
};

}  // anonymous namespace
}  // namespace llamalog

/// @brief Specialization of a `fmt::formatter` to perform automatic conversion of wide characters to UTF-8.
/// @remark This class MUST exist in the `fmt` namespace.
template <>
struct fmt::formatter<llamalog::InlineWideChar> {
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
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
	template <typename FormatContext>
	auto format(const llamalog::InlineWideChar& arg, FormatContext& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		const std::byte* const buffer = &arg.pos;

		std::size_t cchLength;
		std::memcpy(&cchLength, buffer, sizeof(cchLength));
		assert(cchLength <= INT_MAX);

		const std::size_t cbPadding = llamalog::GetPadding<const wchar_t*>(&buffer[sizeof(std::size_t)]);
		assert((reinterpret_cast<std::uintptr_t>(&buffer[sizeof(std::size_t) + cbPadding]) & (alignof(const wchar_t*) - 1)) == 0);
		const wchar_t* const wstr = reinterpret_cast<const wchar_t*>(&buffer[sizeof(std::size_t) + cbPadding]);

		DWORD errorCode;
		if (cchLength <= 256) {
			// try with a fixed size buffer
			char sz[256];
			const int cbCount = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(cchLength), sz, sizeof(sz), nullptr, nullptr);
			if (cbCount) {
				return fmt::vformat_to<llamalog::EscapingFormatter>(ctx.out(), fmt::to_string_view(m_format), fmt::basic_format_args(fmt::make_format_args(std::string_view(sz, cbCount / sizeof(char)))));
			}
			errorCode = GetLastError();
		}
		if (cchLength > 256 || errorCode == ERROR_INSUFFICIENT_BUFFER) {
			const int cbSize = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(cchLength), nullptr, 0, nullptr, nullptr);
			if (cbSize) {
				std::unique_ptr<char[]> str = std::make_unique<char[]>(cbSize);
				if (WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(cchLength), str.get(), cbSize * sizeof(char), nullptr, nullptr)) {
					return fmt::vformat_to<llamalog::EscapingFormatter>(ctx.out(), fmt::to_string_view(m_format), fmt::basic_format_args(fmt::make_format_args(std::string_view(str.get(), cbSize / sizeof(char)))));
				}
			}
			errorCode = GetLastError();
		}

		return format_to(ctx.out(), "(ERROR {:#010x})", GetLastError());
	}

private:
	std::string m_format;  ///< The original format pattern with the argument number (if any) removed.
};


namespace llamalog {
namespace {

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for wide character strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param cbPosition The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const wchar_t*>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ std::size_t& cbPosition) noexcept {
	std::size_t cchLength;
	std::memcpy(&cchLength, &buffer[cbPosition + sizeof(TypeId)], sizeof(cchLength));
	assert(cchLength <= INT_MAX);

	const std::size_t cbOffset = cbPosition + kSize<const wchar_t*>;
	const std::size_t cbPadding = GetPadding<const wchar_t*>(&buffer[cbOffset]);
	assert((reinterpret_cast<std::uintptr_t>(&buffer[cbOffset + cbPadding]) & (alignof(const wchar_t*) - 1)) == 0);

	args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const InlineWideChar*>(&buffer[cbPosition + sizeof(TypeId)])));
	cbPosition += kSize<const wchar_t*> + cbPadding + cchLength * sizeof(wchar_t);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param cbPosition The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<fmt::basic_format_arg<fmt::format_context>>(_Inout_ std::vector<fmt::basic_format_arg<fmt::format_context>>& args, _In_ const std::byte* __restrict const buffer, _Inout_ std::size_t& cbPosition) noexcept {
	static constexpr std::size_t kCustomArgumentSize = kSize<fmt::basic_format_arg<fmt::format_context>>;

	std::uint8_t cbPadding;
	std::memcpy(&cbPadding, &buffer[cbPosition + sizeof(TypeId)], sizeof(cbPadding));

	fmt::basic_format_arg<fmt::format_context> (*createFormatArg)(_In_ const std::byte* const) noexcept;
	std::memcpy(&createFormatArg, &buffer[cbPosition + sizeof(cbPadding) + sizeof(TypeId)], sizeof(createFormatArg));

	std::size_t cbLength;
	std::memcpy(&cbLength, &buffer[cbPosition + sizeof(TypeId) + sizeof(cbPadding) + sizeof(createFormatArg)], sizeof(cbLength));

	args.push_back(createFormatArg(&buffer[cbPosition + kCustomArgumentSize + cbPadding]));

	cbPosition += kCustomArgumentSize + cbPadding + cbLength;
}

}  // anonymous namespace


// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): m_stackBuffer and m_timestamp need no initialization.
LogLine::LogLine(const LogLevel logLevel, _In_z_ const char* __restrict const szFile, _In_z_ const char* __restrict const szFunction, const std::uint32_t line, const char* __restrict const szMessage) noexcept
	: m_logLevel(logLevel)
	, m_threadId(llamalog::GetCurrentThreadId())
	, m_line(line)
	, m_szFile(szFile)
	, m_szFunction(szFunction)
	, m_szMessage(szMessage) {
	// ensure proper memory layout
	static_assert(sizeof(LogLine) == 256, "size of LogLine != 256");
#ifdef __clang_analyzer__
// MSVC does not yet have __builtin_offsetof which gives false errors in the clang-tidy
#pragma push_macro("offsetof")
#define offsetof __builtin_offsetof
#endif
	static_assert(offsetof(LogLine, m_cbUsed) == 0, "offset of m_cbUsed != 0");
#if UINTPTR_MAX == UINT64_MAX
	static_assert(offsetof(LogLine, m_cbSize) == 8, "offset of m_cbSize != 8");
	static_assert(offsetof(LogLine, m_heapBuffer) == 16, "offset of m_heapBuffer != 16");
	static_assert(offsetof(LogLine, m_stackBuffer) == 24, "offset of m_stackBuffer != 24");
	static_assert(offsetof(LogLine, m_logLevel) == 215, "offset of m_logLevel != 215");
	static_assert(offsetof(LogLine, m_threadId) == 216, "offset of m_threadId != 216");
	static_assert(offsetof(LogLine, m_line) == 220, "offset of m_line != 220");
	static_assert(offsetof(LogLine, m_timestamp) == 224, "offset of m_timestamp != 224");
	static_assert(offsetof(LogLine, m_szFile) == 232, "offset of m_szFile != 232");
	static_assert(offsetof(LogLine, m_szFunction) == 240, "offset of m_szFunction != 240");
	static_assert(offsetof(LogLine, m_szMessage) == 248, "offset of m_szMessage != 248");
#elif UINTPTR_MAX == UINT32_MAX
	static_assert(offsetof(LogLine, m_cbSize) == 4, "offset of m_cbSize != 4");
	static_assert(offsetof(LogLine, m_heapBuffer) == 8, "offset of m_heapBuffer != 8");
	static_assert(offsetof(LogLine, m_stackBuffer) == 12, "offset of m_stackBuffer != 12");
	static_assert(offsetof(LogLine, m_logLevel) == 227, "offset of m_logLevel != 227");
	static_assert(offsetof(LogLine, m_threadId) == 228, "offset of m_threadId != 228");
	static_assert(offsetof(LogLine, m_line) == 232, "offset of m_line != 232");
	static_assert(offsetof(LogLine, m_timestamp) == 236, "offset of m_timestamp != 236");
	static_assert(offsetof(LogLine, m_szFile) == 244, "offset of m_szFile != 244");
	static_assert(offsetof(LogLine, m_szFunction) == 248, "offset of m_szFunction != 248");
	static_assert(offsetof(LogLine, m_szMessage) == 252, "offset of m_szMessage != 252");
#elif
	static_assert(false, "layout assertions not defined");
#endif
#ifdef __clang_analyzer__
#pragma pop_macro("offsetof")
#endif
}

// Based on `NanoLogLine::operator<<(int32_t)` from NanoLog.
LogLine& LogLine::operator<<(const bool arg) {
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
LogLine& LogLine::operator<<(_In_opt_ const void* const arg) {
	Write(arg);
	return *this;
}

// Based on `NanoLogLine::operator<<(uint64_t)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_ const std::nullptr_t arg) {
	Write(static_cast<const void*>(arg));
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const char* const arg) {
	if (arg != nullptr) {
		WriteString<char>(arg, std::strlen(arg));
	} else {
		Write<const void*>(nullptr);
	}
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(_In_opt_z_ const wchar_t* const arg) {
	if (arg != nullptr) {
		WriteString<wchar_t>(arg, std::wcslen(arg));
	} else {
		Write<const void*>(nullptr);
	}
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::string_view& arg) {
	WriteString<char>(arg.data(), arg.length());
	return *this;
}

// Based on `NanoLogLine::operator<<(const char*)` from NanoLog.
LogLine& LogLine::operator<<(const std::wstring_view& arg) {
	WriteString<wchar_t>(arg.data(), arg.length());
	return *this;
}

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
std::string LogLine::GetMessage() const {
	std::vector<fmt::basic_format_arg<fmt::format_context>> args;

	const std::byte* __restrict const buffer = GetBuffer();
	for (std::size_t cbPosition = 0; cbPosition < m_cbUsed;) {
		TypeId typeId;
		std::memcpy(&typeId, &buffer[cbPosition], sizeof(typeId));

#pragma push_macro("DECODE_")
#define DECODE_(type_)                                   \
	case kTypeId<type_>:                                 \
		DecodeArgument<type_>(args, buffer, cbPosition); \
		break

		switch (typeId) {
			DECODE_(bool);
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
			DECODE_(fmt::basic_format_arg<fmt::format_context>);
		default:
			assert(false);
			__assume(0);
		}
#pragma pop_macro("DECODE_")
	}

	fmt::basic_format_args<fmt::format_context> formatArgs(args.data(), static_cast<fmt::basic_format_args<fmt::format_context>::size_type>(args.size()));
	fmt::memory_buffer buf;
	fmt::vformat_to<EscapingFormatter>(buf, fmt::to_string_view(m_szMessage), formatArgs);
	return fmt::to_string(buf);
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) const std::byte* LogLine::GetBuffer() const noexcept {
	return !m_heapBuffer ? m_stackBuffer : m_heapBuffer.get();
}

// Derived from `NanoLogLine::buffer` from NanoLog.
_Ret_notnull_ __declspec(restrict) std::byte* LogLine::GetWritePosition(const std::size_t cbAdditionalBytes) {
	const std::size_t cbRequiredSize = m_cbUsed + cbAdditionalBytes;

	if (cbRequiredSize <= m_cbSize) {
		return !m_heapBuffer ? &m_stackBuffer[m_cbUsed] : &(m_heapBuffer.get())[m_cbUsed];
	}

	if (!m_heapBuffer) {
		m_cbSize = max(kGrowBytes, GetNextPowerOf2(cbRequiredSize));
		m_heapBuffer = std::make_unique<std::byte[]>(m_cbSize);
		std::memcpy(m_heapBuffer.get(), m_stackBuffer, m_cbUsed);
	} else {
		m_cbSize = max(m_cbSize + kGrowBytes, GetNextPowerOf2(cbRequiredSize));
		std::unique_ptr<std::byte[]> newHeapBuffer(std::make_unique<std::byte[]>(m_cbSize));
		std::memcpy(newHeapBuffer.get(), m_heapBuffer.get(), m_cbUsed);
		m_heapBuffer.swap(newHeapBuffer);
	}
	return &(m_heapBuffer.get())[m_cbUsed];
}

// Derived from both methods `NanoLogLine::encode` from NanoLog.
template <typename T>
void LogLine::Write(T arg) {
	static constexpr TypeId kArgTypeId = kTypeId<T>;
	static constexpr std::uint8_t kArgSize = kSize<T>;
	std::byte* __restrict const buffer = GetWritePosition(kArgSize);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &arg, sizeof(arg));
	m_cbUsed += kArgSize;
}

/// Derived from `NanoLogLine::encode_c_string` from NanoLog.
template <typename T, typename std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, wchar_t>, int>>
void LogLine::WriteString(_In_z_ const T* const arg, const std::size_t cchLength) {
	static constexpr TypeId kArgTypeId = kTypeId<const T*>;
	static constexpr std::uint8_t kArgSize = kSize<const T*>;
	const std::size_t cbLength = cchLength * sizeof(T);
	const std::size_t cbSize = kArgSize + cbLength;

	std::byte* __restrict buffer = GetWritePosition(cbSize);
	const std::size_t cbPadding = GetPadding<const T*>(&buffer[kArgSize]);
	if (cbPadding) {
		// check if the buffer has enough space for the type AND the with padding
		std::byte* __restrict const newBuffer = GetWritePosition(cbSize + cbPadding);
		if (newBuffer != buffer) {
			buffer = newBuffer;
		}
	}
	assert(m_cbSize - m_cbUsed >= cbSize + cbPadding);
	assert((reinterpret_cast<std::uintptr_t>(&buffer[kArgSize + cbPadding]) & (alignof(const T*) - 1)) == 0);
	static_assert(alignof(const T*) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	std::memcpy(&buffer[sizeof(kArgTypeId)], &cchLength, sizeof(cchLength));
	std::memcpy(&buffer[kArgSize + cbPadding], arg, cbLength);
	m_cbUsed += cbSize + cbPadding;
}

void LogLine::WriteCustomArgument(_In_reads_bytes_(cbSize) const std::byte* __restrict const ptr, const std::size_t cbObjectSize, const std::size_t cbAlign, const void* const createFormatArg) {
	static constexpr TypeId kArgTypeId = kTypeId<fmt::basic_format_arg<fmt::format_context>>;
	static constexpr std::uint8_t kArgSize = kSize<fmt::basic_format_arg<fmt::format_context>>;
	const std::size_t cbSize = kArgSize + cbObjectSize;

	std::byte* __restrict buffer = GetWritePosition(cbSize);
	const std::size_t cbPadding = GetPadding(&buffer[kArgSize], cbAlign);
	if (cbPadding != 0) {
		// check if the buffer has enough space for the type AND the with padding
		std::byte* __restrict const newBuffer = GetWritePosition(cbSize + cbPadding);
		if (newBuffer != buffer) {
			buffer = newBuffer;
		}
	}
	assert(m_cbSize - m_cbUsed >= cbSize + cbPadding);
	assert((reinterpret_cast<std::uintptr_t>(&buffer[kArgSize + cbPadding]) & (cbAlign - 1)) == 0);
	assert(cbAlign <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	std::memcpy(buffer, &kArgTypeId, sizeof(kArgTypeId));
	assert(cbPadding <= UINT8_MAX);
	const std::uint8_t padding = cbPadding & 0xFFu;
	std::memcpy(&buffer[sizeof(kArgTypeId)], &padding, sizeof(padding));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding)], &createFormatArg, sizeof(createFormatArg));
	std::memcpy(&buffer[sizeof(kArgTypeId) + sizeof(padding) + sizeof(createFormatArg)], &cbObjectSize, sizeof(cbObjectSize));
	std::memcpy(&buffer[kArgSize + cbPadding], ptr, cbObjectSize);

	m_cbUsed += cbSize + cbPadding;
}

}  // namespace llamalog
