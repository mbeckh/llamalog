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

#include "llamalog/modifier_format.h"

#include "llamalog/LogLine.h"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <utility>

namespace llamalog {

namespace {

/// @brief The current nesting level (to prevent double escaping).
thread_local int g_nested = 0;

/// @brief Escape a string according to C escaping rules.
/// @warning If no escaping is needed, an empty string is returned for performance reasons.
/// @param sv The input value.
/// @return The escaped result or an empty string.
[[nodiscard]] std::string EscapeC(const std::string_view& sv) {
	constexpr const char kHexDigits[] = "0123456789ABCDEF";
	constexpr char kAsciiSpace = 0x20;
	constexpr char kRadix = 16;

	std::string result;
	auto begin = sv.cbegin();
	const auto end = sv.cend();

	for (auto it = begin; it != end; ++it) {
		const std::uint8_t c = *it;  // MUST be std::uint8_t, NOT char
		if (c == '\\' || c < kAsciiSpace) {
			if (result.empty()) {
				const LogLine::Length len = static_cast<LogLine::Length>(sv.length());
				const LogLine::Length extra = static_cast<LogLine::Length>(std::distance(it, end)) >> 2u;
				result.reserve(static_cast<std::string::size_type>(len) + std::max<LogLine::Length>(extra, 2u));
			}
			result.append(begin, it);
			result.push_back('\\');
			switch (c) {
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
				result.push_back(kHexDigits[c / kRadix]);
				result.push_back(kHexDigits[c % kRadix]);
			}
			begin = it + 1;
		}
	}
	if (!result.empty()) {
		result.append(begin, end);
	}
	return result;
}

std::vector<fmt::format_context::format_arg> GetArguments(fmt::format_context::format_arg& arg, const fmt::format_context& ctx) {
	std::vector<fmt::format_context::format_arg> args;

	// use current argument as first, incrementing all offsets by 1
	args.push_back(arg);

	int index = 0;
	while (true) {
		fmt::format_context::format_arg formatArg = ctx.arg(index++);
		if (!static_cast<bool>(formatArg.type())) {
			// last argument
			break;
		}
		args.push_back(formatArg);
	}

	return args;
}

}  // namespace

namespace internal {

fmt::format_parse_context::iterator ModifierBaseFormatter::parse(fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto start = ctx.begin();
	const auto last = ctx.end();
	if (start != last && *start == ':') {
		++start;
	}

	int open = 1;
	auto current = start;
	auto end = start;

	m_format.assign("{:");
	while (end != last && ((*end != '}' && *end != '?') || open > 1)) {
		if (*end == '\\') {
			if (++end == last) {
				ctx.on_error("invalid escape sequence");
				break;
			}
		} else if (*end == '{') {
			m_format.append(current, ++end);
			current = end;
			++open;
			continue;  // already incremented
		} else if (*end == '}' || *end == '?' || *end == ':') {
			int argId = -1;
			if (current == end) {
				argId = ctx.next_arg_id();
			} else if (std::all_of(current, end, [](const char c) noexcept {
						   return c >= '0' && c <= '9';
					   })) {
				if (std::from_chars(current, end, argId).ptr != end) {
					ctx.on_error("invalid argument specifier");
				}
			} else {
				// use string "as-is"
			}
			if (argId == -1) {
				// use named argument
				m_format.append(current, end);
			} else {
				m_format.append(std::to_string(argId + 1));
			}
			current = end;
			--open;
		} else {
			// just increment
		}
		++end;
	}
	if (open > 1) {
		ctx.on_error("missing '}' in format specifier");
	}
	m_format.append(current, end);
	// read until closing bracket if ? was matched
	while (end != last && *end != '}') {
		++end;
	}
	m_format.push_back('}');

	return end;
}

fmt::format_context::iterator PointerBaseFormatter::Format(fmt::format_context::format_arg&& arg, fmt::format_context& ctx) const {
	const std::vector<fmt::format_context::format_arg> args = GetArguments(arg, ctx);
	return fmt::vformat_to(ctx.out(), fmt::to_string_view(GetFormat()),
						   fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
}

fmt::format_context::iterator EscapeBaseFormatter::Format(fmt::format_context::format_arg&& arg, fmt::format_context& ctx) const {
	const std::vector<fmt::format_context::format_arg> args = GetArguments(arg, ctx);

	if (g_nested > 0) {
		return fmt::vformat_to(ctx.out(), fmt::to_string_view(GetFormat()),
							   fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	}

	constexpr std::size_t kDefaultBufferSize = 128;
	fmt::basic_memory_buffer<char, kDefaultBufferSize> buf;
	++g_nested;
	fmt::vformat_to(buf, fmt::to_string_view(GetFormat()),
					fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));
	--g_nested;

	const std::string_view sv(buf.data(), buf.size());
	const std::string escaped = EscapeC(sv);
	if (escaped.empty()) {
		return std::copy(sv.cbegin(), sv.cend(), ctx.out());
	}
	return std::copy(escaped.cbegin(), escaped.cend(), ctx.out());
}

}  // namespace internal
}  // namespace llamalog
