/*
Copyright 2019 Michael Beckh

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

#include "llamalog/winapi_format.h"

#include "llamalog/Logger.h"
#include "llamalog/finally.h"
#include "llamalog/winapi_log.h"

#include <fmt/core.h>

#include <windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>


//
// Helpers
//

namespace llamalog {

namespace {

/// @brief Remove trailing line feeds from an error message, encode as UTF-8 and print.
/// @remarks This function is a helper for `#FormatSystemErrorCodeTo()`.
/// @param message The error message.
/// @param length The length of the error message NOT including a terminating null character.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator PostProcessErrorMessage(_In_reads_(length) wchar_t* __restrict const message, std::size_t length, fmt::format_context& ctx) {
	if (length >= 2) {
		if (message[length - 2] == L'\r' || message[length - 2] == L'\n') {
			message[length -= 2] = L'\0';
		} else if (message[length - 1] == L' ' || message[length - 1] == L'\n' || message[length - 1] == L'\r') {
			message[length -= 1] = L'\0';
		} else {
			// leave message as it is
		}
	}

	DWORD lastError;  // NOLINT(cppcoreguidelines-init-variables): Guaranteed to be initialized before first read.
	if (constexpr std::uint_fast16_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		// try with a fixed size buffer
		char sz[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(length), sz, sizeof(sz), nullptr, nullptr);
		if (sizeInBytes) {
			return std::copy(sz, sz + sizeInBytes / sizeof(char), ctx.out());
		}
		lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			goto error;  // NOLINT(cppcoreguidelines-avoid-goto, hicpp-avoid-goto): Yes, I DO want a goto here.
		}
	}
	{
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
		if (sizeInBytes) {
			std::unique_ptr<char[]> str = std::make_unique<char[]>(sizeInBytes / sizeof(char));
			if (WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(length), str.get(), sizeInBytes, nullptr, nullptr)) {
				return std::copy(str.get(), str.get() + sizeInBytes / sizeof(char), ctx.out());
			}
		}
		lastError = GetLastError();
	}

error:
	LLAMALOG_INTERNAL_ERROR("WideCharToMultiByte for length {}: {}", length, error_code{lastError});
	const std::string_view sv("<ERROR>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}

/// @brief Create an error message from a system error code and print.
/// @param errorCode The system error code.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator FormatSystemErrorCodeTo(const std::uint32_t errorCode, fmt::format_context& ctx) {
	constexpr std::size_t kDefaultBufferSize = 256;
	wchar_t buffer[kDefaultBufferSize];
	DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), nullptr);
	if (length) {
		return PostProcessErrorMessage(buffer, length, ctx);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		wchar_t* pBuffer = nullptr;

		auto finally = llamalog::finally([&pBuffer]() noexcept {
			LocalFree(pBuffer);
		});
		length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, nullptr, errorCode, 0, reinterpret_cast<wchar_t*>(&pBuffer), 0, nullptr);
		if (length) {
			return PostProcessErrorMessage(pBuffer, length, ctx);
		}
		lastError = GetLastError();
	}
	LLAMALOG_INTERNAL_ERROR("FormatMessageW for code {}: {}", errorCode, error_code{lastError});
	const std::string_view sv("<ERROR>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}

constexpr char kSuppressErrorCode = '%';  ///< @brief Special character used in the format string to suppress printing the error code.

}  // namespace
}  // namespace llamalog


//
// Specializations of fmt::formatter
//

fmt::format_parse_context::iterator fmt::formatter<llamalog::error_code>::parse(const fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto it = ctx.begin();
	const auto last = ctx.end();
	if (it != last && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}
	if (end - it == 1 && *it == llamalog::kSuppressErrorCode) {
		// a pattern consisting of only a percent is used to suppress the error code
		m_format.push_back(llamalog::kSuppressErrorCode);
	} else if (end != it) {
		constexpr std::size_t kGroupingCharacters = 3;
		m_format.reserve(end - it + 3 + kGroupingCharacters);
		m_format.append(" ({:");
		m_format.append(it, end);
		m_format.append("})");
		assert(m_format.length() == static_cast<std::size_t>(end - it) + 3 + kGroupingCharacters);
	} else {
		// use an empty format
	}
	return end;
}

fmt::format_context::iterator fmt::formatter<llamalog::error_code>::format(const llamalog::error_code& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto out = llamalog::FormatSystemErrorCodeTo(arg.code, ctx);
	if (m_format.empty()) {
		// system error codes lie in the range <= 0xFFFF
		return fmt::format_to(ctx.out(), arg.code <= std::numeric_limits<std::uint16_t>::max() ? " ({})" : " ({:#x})", arg.code);
	}
	if (m_format[0] == llamalog::kSuppressErrorCode) {
		return out;
	}
	return fmt::format_to(out, m_format, arg.code);
}


fmt::format_parse_context::iterator fmt::formatter<POINT>::parse(const fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto it = ctx.begin();
	const auto last = ctx.end();
	if (it != last && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}
	constexpr std::size_t kGroupingCharacters = 4;
	m_format.reserve((end - it + 3) * 2 + kGroupingCharacters);
	m_format.append("({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("})");
	assert(m_format.length() == (static_cast<std::size_t>(end - it) + 3) * 2 + kGroupingCharacters);
	return end;
}

fmt::format_context::iterator fmt::formatter<POINT>::format(const POINT& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	return fmt::format_to(ctx.out(), m_format, arg.x, arg.y);
}


fmt::format_parse_context::iterator fmt::formatter<RECT>::parse(const fmt::format_parse_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	auto it = ctx.begin();
	const auto last = ctx.end();
	if (it != last && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != last && *end != '}') {
		++end;
	}
	constexpr std::size_t kGroupingCharacters = 13;
	m_format.reserve((end - it + 3) * 4 + kGroupingCharacters);
	m_format.append("(({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("}) - ({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("}))");
	assert(m_format.length() == (static_cast<std::size_t>(end - it) + 3) * 4 + kGroupingCharacters);
	return end;
}

fmt::format_context::iterator fmt::formatter<RECT>::format(const RECT& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use name as in fmt::formatter.
	return fmt::format_to(ctx.out(), m_format, arg.left, arg.top, arg.right, arg.bottom);
}
