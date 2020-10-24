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

#include "marker_format.h"

#include "buffer_management.h"

#include "llamalog/LogLine.h"
#include "llamalog/Logger.h"
#include "llamalog/winapi_log.h"

#include <fmt/core.h>

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace llamalog::internal {

fmt::format_parse_context::iterator InlineCharBaseFormatter::parse(const fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto start = ctx.begin();
	const auto last = ctx.end();
	if (start != last && *start == ':') {
		++start;
	}
	auto end = start;
	while (end != last && *end != '}' && *end != '?') {
		++end;
	}

	m_format.reserve(end - start + 3);
	m_format.assign("{:");
	m_format.append(start, end);
	m_format.push_back('}');

	// read until closing bracket if ? was matched
	while (end != last && *end != '}') {
		++end;
	}
	return end;
}

}  // namespace llamalog::internal


//
// Specializations of fmt::formatter
//

// NullValue

fmt::format_parse_context::iterator fmt::formatter<llamalog::marker::NullValue>::parse(const fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto it = ctx.begin();
	const auto last = ctx.end();
	while (it != last && (*it == ':' || (*it != '?' && *it != '}'))) {
		++it;
	}
	if (it != last && *it == '?') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}
	if (it == end) {
		m_value.assign("(null)");
	} else {
		m_value.assign(it, end);
	}
	return end;
}

fmt::format_context::iterator fmt::formatter<llamalog::marker::NullValue>::format(const llamalog::marker::NullValue& /* arg */, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	return std::copy(m_value.cbegin(), m_value.cend(), ctx.out());
}


// InlineChar

fmt::format_context::iterator fmt::formatter<llamalog::marker::InlineChar>::format(const llamalog::marker::InlineChar& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	// address of buffer is at the address of the length field
	const std::byte* __restrict const buffer = reinterpret_cast<const std::byte*>(&arg);

	const llamalog::LogLine::Length length = llamalog::buffer::GetValue<llamalog::LogLine::Length>(buffer);
	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");
	const char* const str = reinterpret_cast<const char*>(&buffer[sizeof(length)]);

	return fmt::vformat_to(ctx.out(), fmt::to_string_view(GetFormat()), fmt::basic_format_args(fmt::make_format_args(std::string_view(str, length))));
}


// InlineWideChar

fmt::format_context::iterator fmt::formatter<llamalog::marker::InlineWideChar>::format(const llamalog::marker::InlineWideChar& arg, fmt::format_context& ctx) const {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	// address of buffer is at the address of the length field
	const std::byte* __restrict const buffer = reinterpret_cast<const std::byte*>(&arg);

	const llamalog::LogLine::Length length = llamalog::buffer::GetValue<llamalog::LogLine::Length>(buffer);
	if (!length) {
		// calling WideCharToMultiByte with input length 0 is an error
		return ctx.out();
	}
	const llamalog::LogLine::Align padding = llamalog::buffer::GetPadding<wchar_t>(&buffer[sizeof(length)]);
	const wchar_t* const wstr = reinterpret_cast<const wchar_t*>(&buffer[sizeof(length) + padding]);

	DWORD lastError;  // NOLINT(cppcoreguidelines-init-variables): Guaranteed to be initialized before first read.
	if (constexpr llamalog::LogLine::Length kFixedBufferSize = 256; length <= kFixedBufferSize) {
		// try with a fixed size buffer
		char sz[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), sz, sizeof(sz), nullptr, nullptr);
		if (sizeInBytes) {
			return fmt::vformat_to(ctx.out(), fmt::to_string_view(GetFormat()), fmt::basic_format_args(fmt::make_format_args(std::string_view(sz, sizeInBytes / sizeof(char)))));
		}
		lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			goto error;  // NOLINT(cppcoreguidelines-avoid-goto, hicpp-avoid-goto): Yes, I DO want a goto here
		}
	}
	{
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
		if (sizeInBytes) {
			std::unique_ptr<char[]> str = std::make_unique<char[]>(sizeInBytes / sizeof(char));
			if (WideCharToMultiByte(CP_UTF8, 0, wstr, static_cast<int>(length), str.get(), sizeInBytes, nullptr, nullptr)) {
				return fmt::vformat_to(ctx.out(), fmt::to_string_view(GetFormat()), fmt::basic_format_args(fmt::make_format_args(std::string_view(str.get(), sizeInBytes / sizeof(char)))));
			}
		}
		lastError = GetLastError();
	}

error:
	LLAMALOG_INTERNAL_ERROR("WideCharToMultiByte for length {}: {}", length, llamalog::error_code{lastError});
	const std::string_view sv("<ERROR>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}
