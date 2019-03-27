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

#include <fmt/core.h>

#include <windows.h>

#include <string>


llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT& arg) {
	return logLine.AddCustomArgument(arg);
}

/// @brief Specialization of `fmt::formatter` for a `POINT`.
template <>
struct fmt::formatter<POINT> {
public:
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
		m_format.reserve((end - it + 3) * 2 + 4);
		m_format.append("({:");
		m_format.append(it, end);
		m_format.append("}, {:");
		m_format.append(it, end);
		m_format.append("})");
		return end;
	}

	/// @brief Format the `POINT`.
	/// @param arg A `POINT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	auto format(const POINT& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		return fmt::format_to(ctx.out(), m_format, arg.x, arg.y);
	}

private:
	std::string m_format;  ///< The format pattern for all four values.
};


llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT& arg) {
	return logLine.AddCustomArgument(arg);
}

/// @brief Specialization of `fmt::formatter` for a `RECT`.
template <>
struct fmt::formatter<RECT> {
public:
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
		m_format.reserve((end - it + 3) * 4 + 12);
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

	/// @brief Format the `RECT`.
	/// @param arg A `RECT`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	auto format(const RECT& arg, fmt::format_context& ctx) {  // NOLINT(readability-identifier-naming): MUST use naming from fmt.
		return fmt::format_to(ctx.out(), m_format, arg.left, arg.top, arg.right, arg.bottom);
	}

private:
	std::string m_format;  ///< The format pattern for all four values.
};
