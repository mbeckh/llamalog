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
#pragma once

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

#include <llamalog/LogLine.h>

#include <cstdint>
#include <memory>
#include <utility>

/// @brief The main namespace.
namespace llamalog {

//
// Logging
//

class LogWriter;

/// @brief Initialize the logger.
/// @note `Initialize` MUST be called before any logging takes place.
/// @copyright Derived from `initialize` from NanoLog.
void Initialize();

/// @brief Initialize the logger.
/// @note `Initialize` MUST be called before any logging takes place.
/// @tparam LogWriter MUST be of type `LogWriter`.
/// @param writers One or more `LogWriter` objects.
template <typename... LogWriter>
void Initialize(std::unique_ptr<LogWriter>&&... writers) {
	Initialize();
	(..., AddWriter(std::move(writers)));
}

/// @brief Add a log writer.
/// @param writer The `LogWriter` to add.
void AddWriter(std::unique_ptr<LogWriter>&& writer);

/// @brief Get the filename after the last slash or backslash character.
/// @param szPath A start of the path to shorten.
/// @param szCheck The next character to check.
/// @return Filename and extension after the last path separator.
constexpr __declspec(noalias) _Ret_z_ const char* GetFilename(_In_z_ const char* const szPath, _In_opt_z_ const char* const szCheck = nullptr) noexcept {
	if (!szCheck) {
		// first call
		return GetFilename(szPath, szPath);
	}
	if (*szCheck) {
		if (*szCheck == '/' || *szCheck == '\\') {
			return GetFilename(szCheck + 1, szCheck + 1);
		}
		return GetFilename(szPath, szCheck + 1);
	}
	return szPath;
}


/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void Log(LogLine& logLine);

/// @brief Log a `LogLine`.
/// @param logLine The `LogLine` to send to the logger.
/// @copyright Derived from `Log::operator==` from NanoLog.
void Log(LogLine&& logLine);

/// @brief Logs a new `LogLine`.
/// @tparam T The types of the message arguments.
/// @param priority The `#Priority`.
/// @param szFile The logged file name. This MUST be a literal string, typically from `__FILE__`, i.e. the value is not copied but always referenced by the pointer.
/// @param line The logged line number, typically from `__LINE__`.
/// @param szFunction The logged function, typically from `__func__`. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param szMessage The logged message. This MUST be a literal string, i.e. the value is not copied but always referenced by the pointer.
/// @param args Any args for @p szMessage.
template <typename... T>
void Log(const Priority priority, _In_z_ const char* __restrict const szFile, const std::uint32_t line, _In_z_ const char* __restrict const szFunction, _In_z_ const char* __restrict const szMessage, T&&... args) {
	LogLine logLine(priority, szFile, line, szFunction, szMessage);
	Log((logLine << ... << std::forward<T>(args)));
}

/// @brief End all logging. This MUST be the last function called.
void Shutdown() noexcept;


//
// Exception Handling
//

/// @brief A helper class to carry additional logging context for exceptions.
class __declspec(novtable) BaseException {
protected:
	/// @brief Creates a new instance.
	/// @param szFile The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param szFunction The function where the exception happened.
	/// @param szMessage An additional logging message which MAY use {fmt} pattern syntax.
	BaseException(_In_z_ const char* __restrict const szFile, const std::uint32_t line, _In_z_ const char* __restrict const szFunction, _In_z_ const char* __restrict const szMessage) noexcept
		: m_logLine(Priority::kNone /* unused */, szFile, line, szFunction, szMessage) {
		m_logLine.GenerateTimestamp();
	}

	BaseException(const BaseException&) = default;  ///< @defaultconstructor
	BaseException(BaseException&&) = default;       ///< @defaultconstructor
	virtual ~BaseException() noexcept = default;

public:
	BaseException& operator=(const BaseException&) = delete;  ///< @noassignmentoperator
	BaseException& operator=(BaseException&&) = delete;       ///< @nomoveoperator

protected:
	LogLine m_logLine;  ///< @brief Additional information for logging.

	friend class LogLine;  ///< @brief Allow more straight forward code for copying the data.
};

/// @brief A namespace for functions which must exist in the headers but which users of the library SHALL NOT call directly.
namespace internal {

/// @brief The actual exception class thrown by `#Throw`.
/// @tparam The type of the exception.
template <typename E>
class ExceptionDetail final : public E
	, public virtual BaseException {
public:
	/// @brief Creates a new exception that carries additional logging context.
	/// @tparam T The type of the arguments for the message.
	/// @param exception The actual exception thrown from the code.
	/// @param szFile The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param szFunction The function where the exception happened.
	/// @param szMessage An additional logging message.
	/// @param args Arguments for the logging message.
	template <typename... T>
	ExceptionDetail(E&& exception, _In_z_ const char* __restrict const szFile, const std::uint32_t line, _In_z_ const char* __restrict const szFunction, _In_z_ const char* __restrict const szMessage, T&&... args)
		: E(std::forward<E>(exception))
		, BaseException(szFile, line, szFunction, szMessage) {
		(m_logLine << ... << std::forward<T>(args));
	}
};

}  // namespace internal


/// @brief Throws an exception adding logging context.
/// @remarks The only purpose of this function is to allow template argument deduction of `internal::ExceptionDetail`.
/// @tparam E The type of the exception.
/// @tparam T The type of the arguments for the message.
/// @param exception The actual exception thrown from the code.
/// @param szFile The source code file where the exception happened.
/// @param line The source code line where the exception happened.
/// @param szFunction The function where the exception happened.
/// @param szMessage An additional logging message which MAY use {fmt} pattern syntax.
/// @param args Arguments for the logging message.
template <typename E, typename... T>
[[noreturn]] void Throw(E&& exception, _In_z_ const char* const szFile, std::uint32_t line, _In_z_ const char* const szFunction, const char* const szMessage = "", T&&... args) {
	throw internal::ExceptionDetail<E>(std::forward<E>(exception), szFile, line, szFunction, szMessage, std::forward<T>(args)...);
}

/// @brief Get the additional logging context of an exception if it exists.
/// @note The function MUST be called from within a catch block to get the object, elso `nullptr` is returned.
/// @return The logging context if it exists, else `nullptr`.
_Ret_maybenull_ const BaseException* GetCurrentExceptionAsBaseException() noexcept;

}  // namespace llamalog


//
// Macros
//

/// @brief Emit a log line. @details Without the explicit variable `szFile_` the compiler does not reliably evaluate
/// `#llamalog::GetFilename` at compile time. Add a `do-while`-loop to force a semicolon after the macro.
/// @param priority_ The `Priority`.
/// @param szMessage_ The log message which MAY contain {fmt} placeholders. This MUST be a literal string
#define LLAMALOG_LOG(priority_, szMessage_, ...)                                        \
	do {                                                                                \
		constexpr const char* szFile_ = llamalog::GetFilename(__FILE__);                \
		llamalog::Log(priority_, szFile_, __LINE__, __func__, szMessage_, __VA_ARGS__); \
	} while (0)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_TRACE(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kTrace, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kTrace` for function return values.
/// @details This macro returns the value of @p result_ and can be used to log any value by just wrapping it inside this macro.
/// @param message_ The message pattern which MAY use the syntax of {fmt}.
/// @return The value of @p result_.
#define LOG_TRACE_RESULT(result_, message_, ...)                                                             \
	[&](decltype(result_) result, const char* const function) -> decltype(result_) {                         \
		constexpr const char* file_ = llamalog::GetFilename(__FILE__);                                       \
		llamalog::Log(llamalog::Priority::kTrace, file_, __LINE__, function, message_, result, __VA_ARGS__); \
		return result;                                                                                       \
	}(result_, __func__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kDebug`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_DEBUG(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kDebug, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kInfo`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_INFO(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kInfo, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_WARN(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kWarn, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kError`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_ERROR(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kError, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kFatal`.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_FATAL(szMessage, ...) LLAMALOG_LOG(llamalog::Priority::kFatal, szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_WARN_INTERNAL(szMessage, ...) LLAMALOG_LOG(static_cast<llamalog::Priority>(static_cast<std::uint8_t>(llamalog::Priority::kWarn) + 1), szMessage, __VA_ARGS__)

/// @brief Log a message at `#llamalog::Priority` `#llamalog::Priority::kWarn + 1`.
/// @note This macro SHALL be used by the logger itself and any `LogWriter`s.
/// @param szMessage The message pattern which MAY use the syntax of {fmt}.
#define LOG_ERROR_INTERNAL(szMessage, ...) LLAMALOG_LOG(static_cast<llamalog::Priority>(static_cast<std::uint8_t>(llamalog::Priority::kError) + 1), szMessage, __VA_ARGS__)

/// @brief Throw a new exception with additional logging context.
/// @param exception_ The exception to throw.
/// @param szMessage_ An additional message for logging.
#define LLAMALOG_THROW(exception_, szMessage_, ...)                                        \
	do {                                                                                   \
		constexpr const char* szFile_ = llamalog::GetFilename(__FILE__);                   \
		llamalog::Throw(exception_, szFile_, __LINE__, __func__, szMessage_, __VA_ARGS__); \
	} while (0)

/// @brief Throw a new exception with additional logging context (alias for `LLAMALOG_THROW`).
/// @param exception_ The exception to throw.
/// @param szMessage_ An additional message for logging.
#define THROW(exception_, szMessage_, ...) LLAMALOG_THROW(exception_, szMessage, __VA_ARGS__)
