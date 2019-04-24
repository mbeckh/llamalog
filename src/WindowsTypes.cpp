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

#include "llamalog/WindowsTypes.h"

#include "llamalog/CustomTypes.h"
#include "llamalog/LogLine.h"
#include "llamalog/Logger.h"
#include "llamalog/finally.h"

#include <fmt/core.h>

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>


llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::ErrorCode arg) {
	return logLine.addCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT& arg) {
	return logLine.addCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT& arg) {
	return logLine.addCustomArgument(arg);
}


//
// Helpers
//

namespace llamalog {
namespace {

/// @brief Remove trailing linefeeds from an error message, encode as UTF-8 and print.
/// @remarks This function is a helper for `#formatSystemErrorCodeTo()`.
/// @param message The error message.
/// @param length The length of the error message NOT including a terminating null character.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator postProcessErrorMessage(_In_reads_(length) wchar_t* __restrict const message, std::size_t length, fmt::format_context& ctx) {
	if (length >= 2) {
		if (message[length - 2] == L'\r' || message[length - 2] == L'\n') {
			message[length -= 2] = L'\0';
		} else if (message[length - 1] == L'\n' || message[length - 1] == L'\r') {
			message[length -= 1] = L'\0';
		} else {
			// leave message as it is
		}
	}

	DWORD lastError;
	if (constexpr std::uint_fast16_t kFixedBufferSize = 256; length <= kFixedBufferSize) {
		// try with a fixed size buffer
		char sz[kFixedBufferSize];
		const int sizeInBytes = WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(length), sz, sizeof(sz), nullptr, nullptr);
		if (sizeInBytes) {
			return std::copy(sz, sz + sizeInBytes / sizeof(char), ctx.out());
		}
		lastError = GetLastError();
		if (lastError != ERROR_INSUFFICIENT_BUFFER) {
			goto error;  // NOLINT(cppcoreguidelines-avoid-goto): yes, I DO want a goto here
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
	LLAMALOG_INTERNAL_ERROR("WideCharToMultiByte for length {}: {}", length, ErrorCode{lastError});
	const std::string_view sv("<ERROR>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}

/// @brief Create an error message from a system error code and print.
/// @param errorCode The system error code.
/// @param ctx The output target.
/// @result The output iterator.
fmt::format_context::iterator formatSystemErrorCodeTo(const std::uint32_t errorCode, fmt::format_context& ctx) {
	wchar_t buffer[256];  // NOLINT(readability-magic-numbers): one-time buffer size
	// NOLINTNEXTLINE(hicpp-signed-bitwise): required by Windows API
	DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), nullptr);
	if (length) {
		return postProcessErrorMessage(buffer, length, ctx);
	}

	DWORD lastError = GetLastError();
	if (lastError == ERROR_INSUFFICIENT_BUFFER) {
		wchar_t* pBuffer = nullptr;

		auto finally = llamalog::finally([&pBuffer]() noexcept {
			LocalFree(pBuffer);
		});
		// NOLINTNEXTLINE(hicpp-signed-bitwise): required by Windows API
		length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, 0, reinterpret_cast<wchar_t*>(&pBuffer), 0, nullptr);
		if (length) {
			return postProcessErrorMessage(pBuffer, length, ctx);
		}
		lastError = GetLastError();
	}
	LLAMALOG_INTERNAL_ERROR("FormatMessageW for code {}: {}", errorCode, ErrorCode{lastError});
	const std::string_view sv("<ERROR>");
	return std::copy(sv.cbegin(), sv.cend(), ctx.out());
}

}  // namespace
}  // namespace llamalog


//
// Specializations of fmt::formatter
//

fmt::format_parse_context::iterator fmt::formatter<llamalog::ErrorCode>::parse(const fmt::format_parse_context& ctx) noexcept {
	auto it = ctx.begin();
	if (it != ctx.end() && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != ctx.end() && *end != '}') {
		++end;
	}
	return end;
}

fmt::format_context::iterator fmt::formatter<llamalog::ErrorCode>::format(const llamalog::ErrorCode& arg, fmt::format_context& ctx) {
	llamalog::formatSystemErrorCodeTo(arg.code, ctx);
	return fmt::format_to(ctx.out(), " ({})", arg.code);
}


fmt::format_parse_context::iterator fmt::formatter<POINT>::parse(const fmt::format_parse_context& ctx) {
	auto it = ctx.begin();
	if (it != ctx.end() && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != ctx.end() && *end != '}') {
		++end;
	}
	m_format.reserve((end - it + 3) * 2 + 4);
	m_format.append("({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("})");
	return end;
}

fmt::format_context::iterator fmt::formatter<POINT>::format(const POINT& arg, fmt::format_context& ctx) {
	return fmt::format_to(ctx.out(), m_format, arg.x, arg.y);
}


fmt::format_parse_context::iterator fmt::formatter<RECT>::parse(const fmt::format_parse_context& ctx) {
	auto it = ctx.begin();
	if (it != ctx.end() && *it == ':') {
		++it;
	}
	auto end = it;
	while (end != ctx.end() && *end != '}') {
		++end;
	}
	m_format.reserve((end - it + 3) * 4 + 12);  // NOLINT(readability-magic-numbers): length calculated from strings
	m_format.append("(({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("}), ({:");
	m_format.append(it, end);
	m_format.append("}, {:");
	m_format.append(it, end);
	m_format.append("}))");
	return end;
}

fmt::format_context::iterator fmt::formatter<RECT>::format(const RECT& arg, fmt::format_context& ctx) {
	return fmt::format_to(ctx.out(), m_format, arg.left, arg.top, arg.right, arg.bottom);
}
