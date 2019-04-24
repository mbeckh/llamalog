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

#include "llamalog/Exceptions.h"

#include "llamalog/LogLine.h"
#include "llamalog/WindowsTypes.h"
#include "llamalog/finally.h"
#include "llamalog/llamalog.h"

#include <fmt/format.h>

#include <windows.h>

#include <exception>
#include <memory>
#include <string>

namespace llamalog {

BaseException::BaseException(_In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_opt_z_ const char* __restrict const message) noexcept
	: m_logLine(Priority::kNone /* unused */, file, line, function, message) {
	m_logLine.generateTimestamp();
}

_Ret_z_ const char* BaseException::what(_In_opt_ const std::error_code* const pCode) const noexcept {
	if (m_what) {
		return m_what.get();
	}
	try {
		std::vector<fmt::format_context::format_arg> args;
		m_logLine.copyArgumentsTo(args);

		fmt::basic_memory_buffer<char, 256> buf;
		fmt::vformat_to(buf, fmt::to_string_view(m_logLine.pattern()),
						fmt::basic_format_args<fmt::format_context>(args.data(), static_cast<fmt::format_args::size_type>(args.size())));

		if (pCode) {
			if (buf.size()) {
				buf.push_back(':');
				buf.push_back(' ');
			}
			const std::string message = pCode->message();
			buf.append(message.data(), message.data() + message.size());
		}

		const std::size_t len = buf.size();
		m_what = std::shared_ptr<char[]>(new char[len + 1]);
		char* const ptr = m_what.get();
		std::memcpy(ptr, buf.data(), len * sizeof(char));
		ptr[len] = '\0';
		return ptr;
	} catch (std::exception& e) {
		LLAMALOG_INTERNAL_ERROR("Error creating exception message: {}", e);
	} catch (...) {
		LLAMALOG_INTERNAL_ERROR("Error creating exception message");
	}
	return "<ERROR>";
}

SystemError::SystemError(int code, const std::error_category& category, _In_z_ const char* __restrict message) noexcept
	: std::runtime_error(nullptr)
	, m_code(code, category)
	, m_message(message) {
	// empty
}

_Ret_z_ const char* SystemError::what() const noexcept {
	if (m_what) {
		return m_what.get();
	}
	try {
		const std::size_t messageLen = m_message ? strlen(m_message) : 0;

		const std::string errorMessage = m_code.message();
		const std::size_t errorMessageLen = errorMessage.length();

		const std::size_t offset = messageLen + (messageLen ? 2 : 0);
		const std::size_t len = offset + errorMessageLen;
		m_what = std::shared_ptr<char[]>(new char[len + 1]);
		char* const ptr = m_what.get();
		if (m_message && messageLen) {
			std::memcpy(ptr, m_message, messageLen * sizeof(char));
			ptr[messageLen] = ':';
			ptr[messageLen + 1] = ' ';
		}
		std::memcpy(&ptr[offset], errorMessage.data(), errorMessageLen * sizeof(char));
		ptr[len] = '\0';
		return ptr;
	} catch (std::exception& e) {
		LLAMALOG_INTERNAL_ERROR("Error creating exception message: {}", e);
	} catch (...) {
		LLAMALOG_INTERNAL_ERROR("Error creating exception message");
	}
	return "<ERROR>";
}

_Ret_maybenull_ const BaseException* getCurrentExceptionAsBaseException() noexcept {
	try {
		throw;
	} catch (const BaseException& e) {
		return &e;
	} catch (...) {
		return nullptr;
	}
}

}  // namespace llamalog
