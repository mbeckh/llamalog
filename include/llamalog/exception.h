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

#pragma once

#include <llamalog/LogLine.h>

#include <sal.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace llamalog {

/// @brief A helper class to carry additional logging context for exceptions.
class __declspec(novtable) BaseException {
protected:
	/// @brief Creates a new instance.
	/// @param file The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param function The function where the exception happened.
	/// @param message An additional logging message which MAY use {fmt} pattern syntax.
	BaseException(_In_z_ const char* __restrict file, std::uint32_t line, _In_z_ const char* __restrict function, _In_opt_z_ const char* __restrict message) noexcept;

	BaseException(const BaseException&) = default;      ///< @defaultconstructor
	BaseException(BaseException&&) noexcept = default;  ///< @defaultconstructor
	~BaseException() noexcept = default;

public:
	BaseException& operator=(const BaseException&) = default;      ///< @defaultoperator
	BaseException& operator=(BaseException&&) noexcept = default;  ///< @defaultoperator

protected:
	/// @brief Create the formatted error message with placeholder replacement.
	/// @param pCode An optional error code used to add an error message to the result.
	/// @return The formatted error mesasge.
	_Ret_z_ const char* What(_In_opt_ const std::error_code* pCode) const noexcept;

	/// @brief Allow access to the `LogLine` by base classes.
	/// @return The log line.
	[[nodiscard]] LogLine& GetLogLine() noexcept {
		return m_logLine;
	}

	/// @brief Allow access to the `LogLine` by base classes.
	/// @return The log line.
	[[nodiscard]] const LogLine& GetLogLine() const noexcept {
		return m_logLine;
	}

private:
	LogLine m_logLine;                       ///< @brief Additional information for logging.
	mutable std::shared_ptr<char[]> m_what;  ///< @brief The error message. @details Uses a `std::shared_ptr` because exceptions must be copyable.

	friend class LogLine;  ///< @brief Allow more straight forward code for copying the data.
};


/// @brief A helper class to transfer system errros.
/// @details Other than `std::system_error` the message is not formatted until the time the `LogLine` is written or `#system_error::what()` is called.
class system_error : public std::runtime_error {
public:
	/// @brief Creates a new instance for the provided error code and category.
	/// @param code The error code.
	/// @param category The error category.
	/// @param message The error message.
	system_error(int code, const std::error_category& category, _In_opt_z_ const char* __restrict message) noexcept;
	system_error(system_error&) noexcept = default;   ///< @defaultconstructor
	system_error(system_error&&) noexcept = default;  ///< @defaultconstructor
	~system_error() noexcept = default;

public:
	system_error& operator=(const system_error&) noexcept = default;  ///< @defaultoperator
	system_error& operator=(system_error&&) noexcept = default;       ///< @defaultoperator

public:
	/// @brief Get the system error code (as in `std::system_error::code()`).
	/// @result The error code.
	[[nodiscard]] const std::error_code& code() const noexcept {  // NOLINT(readability-identifier-naming): like code() from std::system_error.
		return m_code;
	}

	/// @brief Create the formatted error message.
	/// @return The formatted error mesasge.
	[[nodiscard]] _Ret_z_ const char* what() const noexcept override;

private:
	std::error_code m_code;                  ///< @brief The error code
	const char* __restrict m_message;        ///< @brief The log message.
	mutable std::shared_ptr<char[]> m_what;  ///< @brief The error message. @details Uses a `std::shared_ptr` because exceptions must be copyable.
};


/// @brief A namespace for functions which must exist in the headers but which users of the library SHALL NOT call directly.
namespace internal {

/// @brief The actual exception class thrown by `#Throw`.
/// @tparam The type of the exception.
template <typename E>
class ExceptionDetail final : public E
	, public BaseException {
public:
	/// @brief Creates a new exception that carries additional logging context.
	/// @tparam T The type of the arguments for the message.
	/// @param exception The actual exception thrown from the code.
	/// @param file The source code file where the exception happened.
	/// @param line The source code line where the exception happened.
	/// @param function The function where the exception happened.
	/// @param message An additional logging message.
	/// @param args Arguments for the logging message.
	template <typename... T>
	ExceptionDetail(E&& exception, _In_z_ const char* __restrict const file, const std::uint32_t line, _In_z_ const char* __restrict const function, _In_opt_z_ const char* __restrict const message, T&&... args)
		: E(std::forward<E>(exception))
		, BaseException(file, line, function, message) {
#pragma warning(suppress : 4834)  // value MAY BE discarded if there are no args
		(GetLogLine() << ... << std::forward<T>(args));
	}
	ExceptionDetail(ExceptionDetail&) = default;            ///< @defaultconstructor
	ExceptionDetail(ExceptionDetail&&) noexcept = default;  ///< @defaultconstructor
	~ExceptionDetail() noexcept = default;

public:
	ExceptionDetail& operator=(const ExceptionDetail&) = default;      ///< @defaultoperator
	ExceptionDetail& operator=(ExceptionDetail&&) noexcept = default;  ///< @defaultoperator

public:
	/// @brief Override `std::exception::what()` to allow formatted output.
	/// @details Delegates to `BaseException::what()` for placeholder replacement if a message is present.
	/// @return The formatted error mesasge.
#pragma warning(suppress : 4702)
	[[nodiscard]] _Ret_z_ const char* what() const noexcept override {  // NOLINT(readability-identifier-naming): override must use same name.
		if (GetLogLine().GetPattern()) {
			if constexpr (std::is_base_of_v<std::system_error, E> || std::is_base_of_v<system_error, E>) {
				return BaseException::What(&E::code());
			}
			return BaseException::What(nullptr);
		}
		// the error message for the error code is already part of what() for std::system_error and system_error
		return __super::what();
	}
};

}  // namespace internal


/// @brief Throws an exception adding logging context.
/// @remarks The only purpose of this function is to allow template argument deduction of `internal::ExceptionDetail`.
/// @tparam E The type of the exception.
/// @tparam T The type of the arguments for the message.
/// @param exception The actual exception thrown from the code.
/// @param file The source code file where the exception happened.
/// @param line The source code line where the exception happened.
/// @param function The function where the exception happened.
template <typename E, typename... T>
[[noreturn]] void Throw(E&& exception, _In_z_ const char* __restrict const file, std::uint32_t line, _In_z_ const char* __restrict const function) {
	throw internal::ExceptionDetail<E>(std::forward<E>(exception), file, line, function, nullptr);
}

/// @brief Throws an exception adding logging context.
/// @remarks The only purpose of this function is to allow template argument deduction of `internal::ExceptionDetail`.
/// @tparam E The type of the exception.
/// @tparam T The type of the arguments for the message.
/// @param exception The actual exception thrown from the code.
/// @param file The source code file where the exception happened.
/// @param line The source code line where the exception happened.
/// @param function The function where the exception happened.
/// @param message An additional logging message which MAY use {fmt} pattern syntax.
/// @param args Arguments for the logging message.
template <typename E, typename... T>
[[noreturn]] void Throw(E&& exception, _In_z_ const char* __restrict const file, std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	throw internal::ExceptionDetail<E>(std::forward<E>(exception), file, line, function, message, std::forward<T>(args)...);
}

/// @brief Get the additional logging context of an exception if it exists.
/// @note The function MUST be called from within a catch block to get the object, elso `nullptr` is returned.
/// @return The logging context if it exists, else `nullptr`.
[[nodiscard]] _Ret_maybenull_ const BaseException* GetCurrentExceptionAsBaseException() noexcept;

}  // namespace llamalog

//
// Macros
//

/// @brief Throw a new exception with additional logging context.
/// @details The variable arguments MAY provide a literal message string and optional arguments.
/// @param exception_ The exception to throw.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define LLAMALOG_THROW(exception_, ...)                                           \
	do {                                                                          \
		constexpr const char* __restrict file_ = llamalog::GetFilename(__FILE__); \
		llamalog::Throw(exception_, file_, __LINE__, __func__, __VA_ARGS__);      \
	} while (0)

/// @brief Throw a new exception with additional logging context (alias for `LLAMALOG_THROW`).
/// @details The variable arguments MAY provide a literal message string and optional arguments.
/// @param exception_ The exception to throw.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): require access to __FILE__, __LINE__ and __func__.
#define THROW(exception_, ...) LLAMALOG_THROW(exception_, __VA_ARGS__)
