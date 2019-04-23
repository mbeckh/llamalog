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

#include "llamalog/LogLine.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <system_error>

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
	_Ret_z_ const char* what(_In_opt_ const std::error_code* pCode) const noexcept;

protected:
	LogLine m_logLine;                       ///< @brief Additional information for logging.
	mutable std::shared_ptr<char[]> m_what;  ///< @brief The error message. @details Uses a `std::shared_ptr` because exceptions must be copyable.

	friend class LogLine;  ///< @brief Allow more straight forward code for copying the data.
};


/// @brief A helper class to transfer system errros.
/// @details Other than `std::system_error` the message is not formatted until the time the `LogLine` is written or `WindowsError::what()` is called.
class SystemError : public std::runtime_error {
public:
	/// @brief Creates a new instance for the provided error code and category.
	/// @param code The error code.
	/// @param category The error category.
	/// @param message The error message.
	SystemError(int code, const std::error_category& category, _In_z_ const char* __restrict message) noexcept;
	SystemError(SystemError&) noexcept = default;   ///< @defaultconstructor
	SystemError(SystemError&&) noexcept = default;  ///< @defaultconstructor
	~SystemError() noexcept = default;

public:
	SystemError& operator=(const SystemError&) noexcept = default;  ///< @defaultoperator
	SystemError& operator=(SystemError&&) noexcept = default;       ///< @defaultoperator

public:
	/// @brief Get the system error code (as in `std::system_error::code()`).
	/// @result The error code.
	const std::error_code& code() const noexcept {
		return m_code;
	}

	/// @brief Create the formatted error message.
	/// @return The formatted error mesasge.
	_Ret_z_ const char* what() const noexcept override;

private:
	std::error_code m_code;                  ///< @brief The error code
	const char* __restrict m_message;        ///< @brief The log message.
	mutable std::shared_ptr<char[]> m_what;  ///< @brief The error message. @details Uses a `std::shared_ptr` because exceptions must be copyable.

	//	friend class LogLine;        ///< @brief Allow more straight forward code for copying the data.
	//	friend class BaseException;  ///< @brief Allow access to `CreateErrorMessage` to prevent additional copy.
};


/// @brief A namespace for functions which must exist in the headers but which users of the library SHALL NOT call directly.
namespace internal {

/// @brief The actual exception class thrown by `#throwException`.
/// @tparam The type of the exception.
template <typename E>
class ExceptionDetail final : public E
	, public virtual BaseException {
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
		(m_logLine << ... << std::forward<T>(args));
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
	_Ret_z_ const char* what() const noexcept override {
		return m_logLine.pattern() ? doWhat() : E::what();
	}

private:
	/// @brief Helper for `#what()` because virtual functions cannot be templates.
	/// @details This is the version used for regular exceptions.
	/// @tparam X Required for SFINAE.
	/// @return The formatted error message.
	template <typename X = E, std::enable_if_t<!std::is_base_of_v<std::system_error, X> && !std::is_base_of_v<SystemError, X>, int> = 0>
	_Ret_z_ const char* doWhat() const noexcept {
		return BaseException::what(nullptr);
	}

	/// @brief Helper for `#what()` because virtual functions cannot be templates.
	/// @details This is the version used for exceptions having a `std::error_code`.
	/// @tparam X Required for SFINAE.
	/// @return The formatted error message.
	template <typename X = E, std::enable_if_t<std::is_base_of_v<std::system_error, X> || std::is_base_of_v<SystemError, X>, int> = 0>
	_Ret_z_ const char* doWhat() const noexcept {
		return BaseException::what(&E::code());
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
[[noreturn]] void throwException(E&& exception, _In_z_ const char* __restrict const file, std::uint32_t line, _In_z_ const char* __restrict const function) {
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
[[noreturn]] void throwException(E&& exception, _In_z_ const char* __restrict const file, std::uint32_t line, _In_z_ const char* __restrict const function, _In_z_ const char* __restrict const message, T&&... args) {
	throw internal::ExceptionDetail<E>(std::forward<E>(exception), file, line, function, message, std::forward<T>(args)...);
}

/// @brief Get the additional logging context of an exception if it exists.
/// @note The function MUST be called from within a catch block to get the object, elso `nullptr` is returned.
/// @return The logging context if it exists, else `nullptr`.
_Ret_maybenull_ const BaseException* getCurrentExceptionAsBaseException() noexcept;

}  // namespace llamalog

//
// Macros
//

/// @brief Throw a new exception with additional logging context.
/// @details The variable arguments MAY provide a literal message string and optional arguments.
/// @param exception_ The exception to throw.
#define LLAMALOG_THROW(exception_, ...)                                               \
	do {                                                                              \
		constexpr const char* __restrict file_ = llamalog::getFilename(__FILE__);     \
		llamalog::throwException(exception_, file_, __LINE__, __func__, __VA_ARGS__); \
	} while (0)

/// @brief Throw a new exception with additional logging context (alias for `LLAMALOG_THROW`).
/// @details The variable arguments MAY provide a literal message string and optional arguments.
/// @param exception_ The exception to throw.
#define THROW(exception_, ...) LLAMALOG_THROW(exception_, __VA_ARGS__)
