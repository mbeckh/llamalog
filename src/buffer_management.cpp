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

#include "buffer_management.h"

#include "exception_format.h"  // IWYU pragma: keep
#include "exception_types.h"
#include "marker_format.h"  // IWYU pragma: keep

#include "llamalog/LogLine.h"
#include "llamalog/custom_types.h"
#include "llamalog/modifier_format.h"  // IWYU pragma: keep
#include "llamalog/modifier_types.h"   // IWYU pragma: keep

#include <fmt/core.h>

#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

namespace llamalog {

using marker::InlineChar;
using marker::InlineWideChar;
using marker::NonTriviallyCopyable;
using marker::NullValue;
using marker::TriviallyCopyable;

using exception::ExceptionInformation;
using exception::HeapBasedException;
using exception::HeapBasedSystemError;
using exception::PlainException;
using exception::PlainSystemError;
using exception::StackBasedException;
using exception::StackBasedSystemError;

namespace buffer {

__declspec(noalias) LogLine::Align GetPadding(_In_ const std::byte* __restrict const ptr, const LogLine::Align align) noexcept {
	assert(align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
	const LogLine::Align mask = align - 1u;
	return static_cast<LogLine::Align>(align - (reinterpret_cast<std::uintptr_t>(ptr) & mask)) & mask;
}

namespace {

//
// Decoding Arguments
//

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// @tparam T The type of the argument.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T>
void DecodeArgument(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const std::byte* __restrict const pData = &buffer[position + sizeof(TypeId)];
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<T>*>(pData)));
	} else {
		const T arg = GetValue<T>(pData);
		args.push_back(fmt::internal::make_arg<fmt::format_context>(arg));
	}
	position += kTypeSize<T>;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `nullptr` values stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<marker::NullValue>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<marker::NullValue>*>(buffer)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const marker::NullValue*>(buffer)));
	}
	position += kTypeSize<marker::NullValue>;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const char*>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId)]);

	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<const InlineChar>*>(&buffer[position + sizeof(TypeId)])));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(std::string_view(reinterpret_cast<const char*>(&buffer[position + kTypeSize<const char*>]), length)));
	}
	position += kTypeSize<const char*> + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for wide character strings stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<const wchar_t*>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId)]);
	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[position + kTypeSize<const wchar_t*>]);

	const std::byte* __restrict const pData = &buffer[position + sizeof(TypeId)];
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<const InlineWideChar>*>(pData)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const InlineWideChar*>(pData)));
	}
	position += kTypeSize<const wchar_t*> + padding + length * static_cast<LogLine::Size>(sizeof(wchar_t));
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding. This function handles `ptr` pointers stored inline.
/// @tparam T The type of the argument.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <typename T>
void DecodePointer(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<T>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;

	const std::byte* __restrict const pData = &buffer[offset];
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<internal::PointerArgument<T>>*>(pData)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::PointerArgument<T>*>(pData)));
	}
	position += kTypeSize<T> + padding;
}

/// @brief Decode a stack based exception from the buffer. @details The value of @p cbPosition is advanced after decoding.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @return The address of the exception object.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
[[nodiscard]] _Ret_notnull_ const StackBasedException* DecodeStackBasedException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pException = reinterpret_cast<const StackBasedException*>(&buffer[offset]);
	const LogLine::Size messageOffset = offset + offsetof(ExceptionInformation /* StackBasedException */, padding);
	const LogLine::Size bufferPos = messageOffset + pException->exceptionInformation.length * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	position = bufferPos + bufferPadding + pException->exceptionInformation.used;
	return pException;
}

/// @brief Decode a heap based exception from the buffer. @details The value of @p position is advanced after decoding.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @return The address of the exception object.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
[[nodiscard]] _Ret_notnull_ const HeapBasedException* DecodeHeapBasedException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pException = reinterpret_cast<const HeapBasedException*>(&buffer[offset]);

	position = offset + sizeof(HeapBasedException) + pException->exceptionInformation.length * sizeof(char);
	return pException;
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for `StackBasedException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<StackBasedException>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const StackBasedException* const pException = DecodeStackBasedException(buffer, position);
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<StackBasedException>*>(pException)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*pException));
	}
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p cbPosition is advanced after decoding.
/// This is the specialization used for `StackBasedSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<StackBasedSystemError>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const StackBasedException* const pException = DecodeStackBasedException(buffer, position);
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<StackBasedSystemError>*>(pException)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const StackBasedSystemError*>(pException)));
	}

	position += sizeof(StackBasedSystemError);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `HeapBasedException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<HeapBasedException>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const HeapBasedException* const pException = DecodeHeapBasedException(buffer, position);
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<HeapBasedException>*>(pException)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*pException));
	}
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `HeapBasedSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<HeapBasedSystemError>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const HeapBasedException* const pException = DecodeHeapBasedException(buffer, position);
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<HeapBasedSystemError>*>(pException)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const HeapBasedSystemError*>(pException)));
	}

	position += sizeof(HeapBasedSystemError);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `PlainException`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<PlainException>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId) + offsetof(PlainException, length)]);

	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<PlainException>*>(&buffer[position + sizeof(TypeId)])));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const PlainException*>(&buffer[position + sizeof(TypeId)])));
	}
	position += kTypeSize<PlainException> + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for `PlainSystemError`s stored as arguments.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<PlainSystemError>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, length)]);

	const std::byte* __restrict const pData = &buffer[position + sizeof(TypeId)];
	if (IsEscaped(static_cast<TypeId>(buffer[position]))) {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const internal::EscapedArgument<PlainSystemError>*>(pData)));
	} else {
		args.push_back(fmt::internal::make_arg<fmt::format_context>(*reinterpret_cast<const PlainSystemError*>(pData)));
	}
	position += kTypeSize<PlainSystemError> + length * sizeof(char);
}

/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<TriviallyCopyable>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable::CreateFormatArg createFormatArg = GetValue<internal::FunctionTable::CreateFormatArg>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(createFormatArg)]);

	args.push_back(createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}


/// @brief Decode an argument from the buffer. @details The argument is made available for formatting by appending it to
/// @p args. The value of @p position is advanced after decoding.
/// This is the specialization used for non-trivially copyable custom types stored inline.
/// @param args The vector of format arguments.
/// @param buffer The argument buffer.
/// @param position The current read position.
/// @copyright This function is based on `decode(std::ostream&, char*, Arg*)` from NanoLog.
template <>
void DecodeArgument<NonTriviallyCopyable>(_Inout_ std::vector<fmt::format_context::format_arg>& args, _In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	args.push_back(pFunctionTable->createFormatArg(&buffer[position + kArgSize + padding]));

	position += kArgSize + padding + size;
}

//
// Skipping Arguments
//

/// @brief Skip a log argument for pointer values.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <typename T>
__declspec(noalias) void SkipPointer(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size offset = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<T>(&buffer[offset]);

	position += kTypeSize<T> + padding;
}

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @tparam T The character type, i.e. either `char` or `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <typename T>
void SkipInlineString(_In_ const std::byte* __restrict buffer, _Inout_ LogLine::Size& position) noexcept;

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `char`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<char>(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId)]);

	// no padding required
	static_assert(alignof(char) == 1, "alignment of char");

	position += kTypeSize<const char*> + length * sizeof(char);
}

/// @brief Skip a log argument of type inline string (either regular or wide character).
/// @details This is the specialization for `wchar_t`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
template <>
__declspec(noalias) void SkipInlineString<wchar_t>(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId)]);
	const LogLine::Size offset = position + kTypeSize<const wchar_t*>;
	const LogLine::Align padding = GetPadding<wchar_t>(&buffer[offset]);

	position += kTypeSize<const wchar_t*> + padding + length * static_cast<LogLine::Size>(sizeof(wchar_t));
}

/// @brief Skip a log argument of type `PlainException`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipPlainException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId) + offsetof(PlainException, length)]);

	position += kTypeSize<PlainException> + length * sizeof(char);
}

/// @brief Skip a log argument of type `PlainSystemError`.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipPlainSystemError(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Length length = GetValue<LogLine::Length>(&buffer[position + sizeof(TypeId) + offsetof(PlainSystemError, length)]);

	position += kTypeSize<PlainSystemError> + length * sizeof(char);
}

/// @brief Skip a log argument of custom type.
/// @param buffer The argument buffer.
/// @param position The current read position. The value is set to the start of the next argument.
__declspec(noalias) void SkipTriviallyCopyable(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<TriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&buffer[position + sizeof(TypeId)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(internal::FunctionTable::CreateFormatArg)]);

	position += kArgSize + padding + size;
}


//
// Copying Arguments
//

/// @brief Copies a `StackBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyStackBasedException(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pSrcException = reinterpret_cast<const StackBasedException*>(&src[offset]);
	const LogLine::Size messageOffset = offset + offsetof(ExceptionInformation /* StackBasedException */, padding);
	const LogLine::Size bufferPos = messageOffset + pSrcException->exceptionInformation.length * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&src[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pSrcException->exceptionInformation.hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		// copy management data
		std::memcpy(&dst[offset], pSrcException, offsetof(ExceptionInformation /* StackBasedException */, padding));

		// copy exception message
		std::memcpy(&dst[messageOffset], &src[messageOffset], pSrcException->exceptionInformation.length * sizeof(char));

		// copy buffers
		CopyObjects(&src[bufferOffset], &dst[bufferOffset], pSrcException->exceptionInformation.used);
	} else {
		// copy everything in one turn
		std::memcpy(&dst[offset], pSrcException, offsetof(ExceptionInformation /* StackBasedException */, padding) + pSrcException->exceptionInformation.length * sizeof(char) + bufferPadding + pSrcException->exceptionInformation.used);
	}

	position = bufferPos + bufferPadding + pSrcException->exceptionInformation.used;
}

/// @brief Copies a `StackBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyStackBasedSystemError(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	CopyStackBasedException(src, dst, position);

	constexpr LogLine::Size size = sizeof(StackBasedSystemError);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Copies a `HeapBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyHeapBasedException(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pSrcException = reinterpret_cast<const HeapBasedException*>(&src[offset]);
	HeapBasedException* const pDstException = reinterpret_cast<HeapBasedException*>(&dst[offset]);

	std::memcpy(pDstException, pSrcException, sizeof(HeapBasedException) + pSrcException->exceptionInformation.length * sizeof(char));

	std::unique_ptr<std::byte[]> heapBuffer = std::make_unique<std::byte[]>(pSrcException->exceptionInformation.used);
	pDstException->pHeapBuffer = heapBuffer.get();

	if (pSrcException->exceptionInformation.hasNonTriviallyCopyable) {
		// copy buffers
		CopyObjects(pSrcException->pHeapBuffer, pDstException->pHeapBuffer, pSrcException->exceptionInformation.used);
	} else {
		// copy everything in one turn
		std::memcpy(pDstException->pHeapBuffer, pSrcException->pHeapBuffer, sizeof(HeapBasedException) + pSrcException->exceptionInformation.used);
	}
	heapBuffer.release();

	position = offset + sizeof(HeapBasedException) + pSrcException->exceptionInformation.length * sizeof(char);
}

/// @brief Copies a `HeapBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyHeapBasedSystemError(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) {
	CopyHeapBasedException(src, dst, position);

	constexpr LogLine::Size size = sizeof(HeapBasedSystemError);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Copies a custom type by calling construct (new) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void CopyNonTriviallyCopyable(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&src[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<internal::FunctionTable*>(&src[position + sizeof(TypeId) + sizeof(padding)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&src[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogLine::Size offset = position + kArgSize + padding;
	pFunctionTable->copy(&src[offset], &dst[offset]);

	position = offset + size;
}


//
// Moving Arguments
//

/// @brief Moves a `StackBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveStackBasedException(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pSrcException = reinterpret_cast<const StackBasedException*>(&src[offset]);
	const LogLine::Size messageOffset = offset + offsetof(ExceptionInformation /* StackBasedException */, padding);
	const LogLine::Size bufferPos = messageOffset + pSrcException->exceptionInformation.length * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&src[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pSrcException->exceptionInformation.hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		// copy management data
		std::memcpy(&dst[offset], pSrcException, offsetof(ExceptionInformation /* StackBasedException */, padding));

		// copy exception message
		std::memcpy(&dst[messageOffset], &src[messageOffset], pSrcException->exceptionInformation.length * sizeof(char));

		// move buffers
		MoveObjects(&src[bufferOffset], &dst[bufferOffset], pSrcException->exceptionInformation.used);
	} else {
		// copy everything in one turn
		std::memcpy(&dst[offset], pSrcException, offsetof(ExceptionInformation /* StackBasedException */, padding) + pSrcException->exceptionInformation.length * sizeof(char) + bufferPadding + pSrcException->exceptionInformation.used);
	}

	position = bufferPos + bufferPadding + pSrcException->exceptionInformation.used;
}

/// @brief Moves a `StackBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveStackBasedSystemError(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	MoveStackBasedException(src, dst, position);

	constexpr LogLine::Size size = sizeof(StackBasedSystemError);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Moves a `HeapBasedException` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
__declspec(noalias) void MoveHeapBasedException(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	// copy type data
	std::memcpy(&dst[position], &src[position], sizeof(TypeId));

	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&src[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pSrcException = reinterpret_cast<const HeapBasedException*>(&src[offset]);

	std::memcpy(&dst[offset], pSrcException, sizeof(HeapBasedException) + pSrcException->exceptionInformation.length * sizeof(char));

	position = offset + sizeof(HeapBasedException) + pSrcException->exceptionInformation.length * sizeof(char);
}

/// @brief Moves a `HeapBasedSystemError` argument and sets @p position to the start of the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
__declspec(noalias) void MoveHeapBasedSystemError(_In_ const std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	MoveHeapBasedException(src, dst, position);

	constexpr LogLine::Size size = sizeof(HeapBasedSystemError);
	std::memcpy(&dst[position], &src[position], size);

	position += size;
}

/// @brief Moves a custom type by calling construct (new) and destruct (old) and moves @p position to the next log argument.
/// @param src The source argument buffer.
/// @param dst The target argument buffer.
/// @param position The current read position.
void MoveNonTriviallyCopyable(_Inout_ std::byte* __restrict const src, _Out_ std::byte* __restrict const dst, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&src[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&src[position + sizeof(TypeId) + sizeof(padding)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&src[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	// copy management data
	std::memcpy(&dst[position], &src[position], kArgSize);

	// create the argument in the new position
	const LogLine::Size offset = position + kArgSize + padding;
	pFunctionTable->move(&src[offset], &dst[offset]);
	// and destruct the copied-from version
	pFunctionTable->destruct(&src[offset]);

	position = offset + size;
}

//
// Calling Argument Destructors
//

/// @brief Call the destructor of all `StackBasedException`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructStackBasedException(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<StackBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const StackBasedException* const pException = reinterpret_cast<const StackBasedException*>(&buffer[offset]);
	const LogLine::Size messageOffset = offset + offsetof(ExceptionInformation /* StackBasedException */, padding);
	const LogLine::Size bufferPos = messageOffset + pException->exceptionInformation.length * sizeof(char);
	const LogLine::Align bufferPadding = GetPadding(&buffer[bufferPos], __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	if (pException->exceptionInformation.hasNonTriviallyCopyable) {
		const LogLine::Size bufferOffset = bufferPos + bufferPadding;

		CallDestructors(&buffer[bufferOffset], pException->exceptionInformation.used);
	}

	position = bufferPos + bufferPadding + pException->exceptionInformation.used;
}

/// @brief Call the destructor of all `StackBasedSystemError`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructStackBasedSystemError(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	DestructStackBasedException(buffer, position);

	position += sizeof(StackBasedSystemError);
}

/// @brief Call the destructor of all `HeapBasedException`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructHeapBasedException(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	const LogLine::Size pos = position + sizeof(TypeId);
	const LogLine::Align padding = GetPadding<HeapBasedException>(&buffer[pos]);
	const LogLine::Size offset = pos + padding;
	const HeapBasedException* const pException = reinterpret_cast<const HeapBasedException*>(&buffer[offset]);

	if (pException->exceptionInformation.hasNonTriviallyCopyable) {
		CallDestructors(pException->pHeapBuffer, pException->exceptionInformation.used);
	}

	delete[] pException->pHeapBuffer;

	position = offset + sizeof(HeapBasedException) + pException->exceptionInformation.length * sizeof(char);
}

/// @brief Call the destructor of all `HeapBasedSystemError`'s arguments and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructHeapBasedSystemError(_In_ const std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	DestructHeapBasedException(buffer, position);

	position += sizeof(HeapBasedSystemError);
}

/// @brief Call the destructor of a custom type and moves @p position to the next log argument.
/// @param buffer The argument buffer.
/// @param position The current read position.
void DestructNonTriviallyCopyable(_In_ std::byte* __restrict const buffer, _Inout_ LogLine::Size& position) noexcept {
	constexpr auto kArgSize = kTypeSize<NonTriviallyCopyable>;

	const LogLine::Align padding = GetValue<LogLine::Align>(&buffer[position + sizeof(TypeId)]);
	const internal::FunctionTable* const pFunctionTable = GetValue<const internal::FunctionTable*>(&buffer[position + sizeof(TypeId) + sizeof(padding)]);
	const LogLine::Size size = GetValue<LogLine::Size>(&buffer[position + sizeof(TypeId) + sizeof(padding) + sizeof(pFunctionTable)]);  // NOLINT(bugprone-sizeof-expression): sizeof pointer is deliberate to automatically follow type changes.

	const LogLine::Size offset = position + kArgSize + padding;
	pFunctionTable->destruct(&buffer[offset]);

	position = offset + size;
}

}  // namespace

//
// Buffer Management
//

// Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
void CopyArgumentsFromBufferTo(_In_reads_bytes_(used) const std::byte* __restrict const buffer, const LogLine::Size used, std::vector<fmt::format_context::format_arg>& args) {
	for (LogLine::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("DECODE_")
#pragma push_macro("DECODE_PTR_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define DECODE_(type_)                                 \
	case kTypeId<type_>:                               \
		DecodeArgument<type_>(args, buffer, position); \
		break
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define DECODE_PTR_(type_)                             \
	case kTypeId<type_>:                               \
		DecodeArgument<type_>(args, buffer, position); \
		break;                                         \
	case kTypeId<type_, true>:                         \
		DecodePointer<type_>(args, buffer, position);  \
		break
		/// @endcond
		switch (typeId & static_cast<TypeId>(~kEscapedFlag)) {
			DECODE_(NullValue);
			DECODE_PTR_(bool);
			DECODE_(char);
			DECODE_PTR_(signed char);
			DECODE_PTR_(unsigned char);
			DECODE_PTR_(signed short);
			DECODE_PTR_(unsigned short);
			DECODE_PTR_(signed int);
			DECODE_PTR_(unsigned int);
			DECODE_PTR_(signed long);
			DECODE_PTR_(unsigned long);
			DECODE_PTR_(signed long long);
			DECODE_PTR_(unsigned long long);
			DECODE_PTR_(float);
			DECODE_PTR_(double);
			DECODE_PTR_(long double);
			DECODE_(const void*);
			DECODE_(const char*);
			DECODE_(const wchar_t*);
			DECODE_(StackBasedException);
			DECODE_(StackBasedSystemError);
			DECODE_(HeapBasedException);
			DECODE_(HeapBasedSystemError);
			DECODE_(PlainException);
			DECODE_(PlainSystemError);
			DECODE_(TriviallyCopyable);     // case for ptr is handled in CreateFormatArg
			DECODE_(NonTriviallyCopyable);  // case for ptr is handled in CreateFormatArg
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("DECODE_")
#pragma pop_macro("DECODE_PTR_")
	}
}

void CopyObjects(_In_reads_bytes_(used) const std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const LogLine::Size used) {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogLine::Size start = 0;
	for (LogLine::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
#pragma push_macro("SKIP_PTR_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_PTR_(type_)                   \
	case kTypeId<type_>:                   \
		position += kTypeSize<type_>;      \
		break;                             \
	case kTypeId<type_, true>:             \
		SkipPointer<type_>(src, position); \
		break
		/// @endcond

		switch (typeId & static_cast<TypeId>(~kEscapedFlag)) {
			SKIP_(NullValue);
			SKIP_PTR_(bool);
			SKIP_(char);
			SKIP_PTR_(signed char);
			SKIP_PTR_(unsigned char);
			SKIP_PTR_(signed short);
			SKIP_PTR_(unsigned short);
			SKIP_PTR_(signed int);
			SKIP_PTR_(unsigned int);
			SKIP_PTR_(signed long);
			SKIP_PTR_(unsigned long);
			SKIP_PTR_(signed long long);
			SKIP_PTR_(unsigned long long);
			SKIP_PTR_(float);
			SKIP_PTR_(double);
			SKIP_PTR_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(src, position);
			break;
		case kTypeId<StackBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyStackBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<StackBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyStackBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyHeapBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyHeapBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<PlainException>:
			SkipPlainException(src, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(src, position);
			break;
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			CopyNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
#pragma pop_macro("SKIP_PTR_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

void MoveObjects(_In_reads_bytes_(used) std::byte* __restrict const src, _Out_writes_bytes_(used) std::byte* __restrict const dst, const LogLine::Size used) noexcept {
	// assert that both buffers are equally aligned so that any offsets and padding values can be simply copied
	assert(reinterpret_cast<std::uintptr_t>(src) % __STDCPP_DEFAULT_NEW_ALIGNMENT__ == reinterpret_cast<std::uintptr_t>(dst) % __STDCPP_DEFAULT_NEW_ALIGNMENT__);

	LogLine::Size start = 0;
	for (LogLine::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&src[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
#pragma push_macro("SKIP_PTR_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_PTR_(type_)                   \
	case kTypeId<type_>:                   \
		position += kTypeSize<type_>;      \
		break;                             \
	case kTypeId<type_, true>:             \
		SkipPointer<type_>(src, position); \
		break
		/// @endcond

		switch (typeId & static_cast<TypeId>(~kEscapedFlag)) {
			SKIP_(NullValue);
			SKIP_PTR_(bool);
			SKIP_(char);
			SKIP_PTR_(signed char);
			SKIP_PTR_(unsigned char);
			SKIP_PTR_(signed short);
			SKIP_PTR_(unsigned short);
			SKIP_PTR_(signed int);
			SKIP_PTR_(unsigned int);
			SKIP_PTR_(signed long);
			SKIP_PTR_(unsigned long);
			SKIP_PTR_(signed long long);
			SKIP_PTR_(unsigned long long);
			SKIP_PTR_(float);
			SKIP_PTR_(double);
			SKIP_PTR_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(src, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(src, position);
			break;
		case kTypeId<StackBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveStackBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<StackBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveStackBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedException>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveHeapBasedException(src, dst, position);
			start = position;
			break;
		case kTypeId<HeapBasedSystemError>:
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveHeapBasedSystemError(src, dst, position);
			start = position;
			break;
		case kTypeId<PlainException>:
			SkipPlainException(src, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(src, position);
			break;
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(src, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			// first copy any trivially copyable objects up to here
			std::memcpy(&dst[start], &src[start], position - start);
			MoveNonTriviallyCopyable(src, dst, position);
			start = position;
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
#pragma pop_macro("SKIP_PTR_")
	}
	// copy any remaining trivially copyable objects
	std::memcpy(&dst[start], &src[start], used - start);
}

void CallDestructors(_Inout_updates_bytes_(used) std::byte* __restrict buffer, LogLine::Size used) noexcept {
	for (LogLine::Size position = 0; position < used;) {
		const TypeId typeId = GetValue<TypeId>(&buffer[position]);

		/// @cond hide
#pragma push_macro("SKIP_")
#pragma push_macro("SKIP_PTR_")
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_(type_)                  \
	case kTypeId<type_>:              \
		position += kTypeSize<type_>; \
		break
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): Not possible without macro.
#define SKIP_PTR_(type_)                      \
	case kTypeId<type_>:                      \
		position += kTypeSize<type_>;         \
		break;                                \
	case kTypeId<type_, true>:                \
		SkipPointer<type_>(buffer, position); \
		break
		/// @endcond

		switch (typeId & static_cast<TypeId>(~kEscapedFlag)) {
			SKIP_(NullValue);
			SKIP_PTR_(bool);
			SKIP_(char);
			SKIP_PTR_(signed char);
			SKIP_PTR_(unsigned char);
			SKIP_PTR_(signed short);
			SKIP_PTR_(unsigned short);
			SKIP_PTR_(signed int);
			SKIP_PTR_(unsigned int);
			SKIP_PTR_(signed long);
			SKIP_PTR_(unsigned long);
			SKIP_PTR_(signed long long);
			SKIP_PTR_(unsigned long long);
			SKIP_PTR_(float);
			SKIP_PTR_(double);
			SKIP_PTR_(long double);
			SKIP_(const void*);
		case kTypeId<const char*>:
			SkipInlineString<char>(buffer, position);
			break;
		case kTypeId<const wchar_t*>:
			SkipInlineString<wchar_t>(buffer, position);
			break;
		case kTypeId<StackBasedException>:
			DestructStackBasedException(buffer, position);
			break;
		case kTypeId<StackBasedSystemError>:
			DestructStackBasedSystemError(buffer, position);
			break;
		case kTypeId<HeapBasedException>:
			DestructHeapBasedException(buffer, position);
			break;
		case kTypeId<HeapBasedSystemError>:
			DestructHeapBasedSystemError(buffer, position);
			break;
		case kTypeId<PlainException>:
			SkipPlainException(buffer, position);
			break;
		case kTypeId<PlainSystemError>:
			SkipPlainSystemError(buffer, position);
			break;
		case kTypeId<TriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			SkipTriviallyCopyable(buffer, position);
			break;
		case kTypeId<NonTriviallyCopyable>:
			// case for ptr is handled in CreateFormatArg
			DestructNonTriviallyCopyable(buffer, position);
			break;
		default:
			assert(false);
			__assume(false);
		}
#pragma pop_macro("SKIP_")
#pragma pop_macro("SKIP_PTR_")
	}
}

}  // namespace buffer
}  // namespace llamalog
