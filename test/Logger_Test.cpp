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

#include "llamalog/Logger.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"
#include "llamalog/exception.h"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <detours_gmock.h>
#include <windows.h>

#include <exception>
#include <memory>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>


namespace llamalog::test {

namespace t = testing;

namespace {

#pragma warning(suppress : 4100)
MATCHER_P(MatchesRegex, pattern, "") {
	return std::regex_match(arg, std::regex(pattern));
}

class LoggerTest : public t::Test {
protected:
	std::ostringstream m_out;
	int m_lines = 0;
};


class StringWriter : public LogWriter {
public:
	StringWriter(const Priority logLevel, std::ostringstream& out, int& lines)
		: LogWriter(logLevel)
		, m_out(out)
		, m_lines(lines) {
		// empty
	}

protected:
	void Log(const LogLine& logLine) final {
		fmt::basic_memory_buffer<char, 256> buffer;
		fmt::format_to(buffer, "{} {} [{}] {}:{} {} {}\n",
					   FormatTimestamp(logLine.GetTimestamp()),
					   FormatPriority(logLine.GetPriority()),
					   logLine.GetThreadId(),
					   logLine.GetFile(),
					   logLine.GetLine(),
					   logLine.GetFunction(),
					   logLine.GetLogMessage());
		m_out << std::string_view(buffer.data(), buffer.size());
		++m_lines;
	}

private:
	std::ostringstream& m_out;
	int& m_lines;
};

}  // namespace

#define WIN32_FUNCTIONS(fn_)                                                                                                                                         \
	fn_(2, BOOL, WINAPI, SetThreadPriority,                                                                                                                          \
		(HANDLE hThread, int nPriority),                                                                                                                             \
		(hThread, nPriority),                                                                                                                                        \
		nullptr);                                                                                                                                                    \
	fn_(4, BOOL, WINAPI, SetThreadInformation,                                                                                                                       \
		(HANDLE hThread, THREAD_INFORMATION_CLASS ThreadInformationClass, LPVOID ThreadInformation, DWORD ThreadInformationSize),                                    \
		(hThread, ThreadInformationClass, ThreadInformation, ThreadInformationSize),                                                                                 \
		nullptr);                                                                                                                                                    \
	fn_(8, int, WINAPI, WideCharToMultiByte,                                                                                                                         \
		(UINT codePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar), \
		(codePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar),                                              \
		nullptr);                                                                                                                                                    \
	fn_(1, void, WINAPI, WakeConditionVariable,                                                                                                                      \
		(PCONDITION_VARIABLE ConditionVariable),                                                                                                                     \
		(ConditionVariable),                                                                                                                                         \
		nullptr);                                                                                                                                                    \
	fn_(1, void, WINAPI, OutputDebugStringA,                                                                                                                         \
		(LPCSTR lpOutputString),                                                                                                                                     \
		(lpOutputString),                                                                                                                                            \
		nullptr)


DTGM_DECLARE_API_MOCK(Win32, WIN32_FUNCTIONS);


//
// GetFilename
//

TEST_F(LoggerTest, GetFilename_HasPath_ReturnFilename) {
	constexpr const char* filename = GetFilename(__FILE__);
	EXPECT_STREQ("Logger_Test.cpp", filename);
}

TEST_F(LoggerTest, GetFilename_HasNoPath_ReturnFilename) {
	constexpr const char* filename = GetFilename("Logger_Test.cpp");
	EXPECT_STREQ("Logger_Test.cpp", filename);
}

TEST_F(LoggerTest, GetFilename_IsEmpty_ReturnEmpty) {
	constexpr const char* filename = GetFilename("");
	EXPECT_STREQ("", filename);
}


//
// Creation Errors
//

TEST_F(LoggerTest, Initialize_SetThreadPriorityError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, SetThreadPriority(DTGM_ARG2))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_HANDLE, FALSE));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} WARN [^\\n]*Error configuring thread: [^\\n]+ \\(6\\)\\n[^\\n]+Test\\n"));
}

TEST_F(LoggerTest, Initialize_SetThreadInformationError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, SetThreadInformation(DTGM_ARG4))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_HANDLE, FALSE));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} WARN [^\\n]*Error configuring thread: [^\\n]+ \\(6\\)\\n[^\\n]+Test\\n"));
}

//
// Log
//

TEST_F(LoggerTest, Log_OneLine) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", 7);

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] Logger_Test.cpp:99 TestBody 7\n"));
}

TEST_F(LoggerTest, Log_1000Lines) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		for (int i = 0; i < 1000; ++i) {
			llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", i);
		}

		llamalog::Shutdown();
	}

	EXPECT_EQ(1000, m_lines);
}


//
// Log exception safe
//

TEST_F(LoggerTest, LogNoExcept_OneLineWithNoExcept_NoThrow) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	const std::thread::id threadId = std::this_thread::get_id();

	EXPECT_CALL(mock, WakeConditionVariable(DTGM_ARG1))
		.WillOnce(t::DoDefault())  // initial log call
		.WillRepeatedly(t::Invoke([threadId](PCONDITION_VARIABLE ConditionVariable) -> void {
			if (std::this_thread::get_id() == threadId) {
				// prevent being fired from other threads
				throw std::exception("Logging exception");
			}
			// clang-format off
			DTGM_REAL(Win32, WakeConditionVariable)(ConditionVariable);
			// clang-format on
		}));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")));

	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		EXPECT_NO_THROW(llamalog::LogNoExcept(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", 7));
		EXPECT_NO_THROW(llamalog::LogNoExcept(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", 8));
		llamalog::Flush();

		DTGM_DETACH_API_MOCK(Win32);

		llamalog::Shutdown();
	}
}


//
// Encoding Error
//

TEST_F(LoggerTest, Encoding_WideCharToMultiByteError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_FLAGS, 0));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]*<ERROR>\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length 4: <ERROR> \\(1004\\)\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length \\d+: <ERROR> \\(1004\\)\\n"));
}

TEST_F(LoggerTest, Encoding_WideCharToMultiByteErrorWithoutFlush_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_FLAGS, 0));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Shutdown();  // shutdown instead of flush before detach

	DTGM_DETACH_API_MOCK(Win32);

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]*<ERROR>\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length 4: <ERROR> \\(1004\\)\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length \\d+: <ERROR> \\(1004\\)\\n"));
}


//
// Exception during logging
//

TEST_F(LoggerTest, Exception_ExceptionDuringLogging_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(2)
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test");
		}))
		.WillRepeatedly(t::DoDefault());
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+Pop Error writing log: arg=Test @[^\\n]+\\n"));
}

TEST_F(LoggerTest, Exception_ThrowObjectDuringLogging_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			throw "test error";
		}));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+Pop Error writing log\\n"));
}

TEST_F(LoggerTest, Exception_ExceptionDuringExceptionHandling_LogPanic) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	const std::thread::id threadId = std::this_thread::get_id();

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test");
		}))
		.WillRepeatedly(t::DoDefault());
	EXPECT_CALL(mock, WakeConditionVariable(DTGM_ARG1))
		.WillOnce(t::DoDefault())  // initial log call
		.WillRepeatedly(t::Invoke([threadId](PCONDITION_VARIABLE ConditionVariable) -> void {
			if (std::this_thread::get_id() != threadId) {
				// prevent being fired from flush
				throw std::exception("Logging exception");
			}
			// clang-format off
			DTGM_REAL(Win32, WakeConditionVariable)(ConditionVariable);
			// clang-format on
		}));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	// exception happens AFTER the LogLine is added, so the message is logged despite the exception
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+Pop Error writing log: arg=Test @[^\\n]+\\n"));
}

TEST_F(LoggerTest, Exception_ExceptionDuringExceptionLogging_LogLastError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(3)
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception 1"), "arg={}", L"Test 1");
		}))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception 2"), "arg={}", L"Test 2");
		}))
		.WillRepeatedly(t::DoDefault());
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+Pop Error writing log: arg=Test 2 @[^\\n]+\\n"));
}

TEST_F(LoggerTest, Exception_PermamentExceptionDuringExceptionLogging_LogPanic) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(3)
		.WillRepeatedly(t::Invoke([](t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused, t::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test", "foo");
		}));
	EXPECT_CALL(mock, OutputDebugStringA(t::StartsWith("PANIC: ")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::Shutdown();

	EXPECT_EQ(0, m_lines);
	EXPECT_EQ("", m_out.str());
}
//
// Macros
//

TEST_F(LoggerTest, LOGTRACE_WriterIsDebug_NoOutput) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(0, m_lines);
	EXPECT_EQ("", m_out.str());
}

TEST_F(LoggerTest, LOGTRACE_WriterIsTrace_Output) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGTRACE_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test {}", 1);

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test 1\n"));
}

TEST_F(LoggerTest, LOGDEBUG) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_DEBUG("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGDEBUG_WitArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_DEBUG("Test {}", std::string("Test"));

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test Test\n"));
}

TEST_F(LoggerTest, LOGINFO) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_INFO("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGINFO_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		std::string arg("Test");
		LOG_INFO("Test {}", arg);

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test Test\n"));
}

TEST_F(LoggerTest, LOGWARN) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_WARN("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGWARN_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_WARN("Test {}{}", 1, 's');

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test 1s\n"));
}

TEST_F(LoggerTest, LOGERROR) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_ERROR("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGERROR_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_ERROR("Test {}{}{}", 1, "Test", 2);

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test 1Test2\n"));
}

TEST_F(LoggerTest, LOGFATAL) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_FATAL("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LoggerTest, LOGFATAL_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_FATAL("Test {}", 1.1);

		llamalog::Shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody Test 1.1\n"));
}

TEST_F(LoggerTest, LOGTRACERESULT) {
	int result = 0;
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		int i = 1;
		result = LOG_TRACE_RESULT(++i, "{}");

		llamalog::Shutdown();
	}

	EXPECT_EQ(2, result);
	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody 2\n"));
}

TEST_F(LoggerTest, LOGTRACERESULT_WithArgs) {
	int result = 0;
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		int i = 1;
		result = LOG_TRACE_RESULT(++i, "{} {}", "arg");

		llamalog::Shutdown();
	}

	EXPECT_EQ(2, result);
	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] Logger_Test.cpp:\\d+ TestBody 2 arg\n"));
}

}  // namespace llamalog::test
