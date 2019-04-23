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

#include "llamalog/llamalog.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <detours.h>
#include <detours_gmock.h>
#include <windows.h>

#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>


namespace llamalog::test {

namespace {

#pragma warning(suppress : 4100)
MATCHER_P(MatchesRegex, pattern, "") {
	return std::regex_match(arg, std::regex(pattern));
}

class LlamalogTest : public testing::Test {
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
	void log(const LogLine& logLine) final {
		fmt::basic_memory_buffer<char, 256> buffer;
		fmt::format_to(buffer, "{} {} [{}] {}:{} {} {}\n",
					   formatTimestamp(logLine.timestamp()),
					   formatPriority(logLine.priority()),
					   logLine.threadId(),
					   logLine.file(),
					   logLine.line(),
					   logLine.function(),
					   logLine.message());
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

TEST_F(LlamalogTest, getFilename_HasPath_ReturnFilename) {
	constexpr const char* filename = getFilename(__FILE__);
	EXPECT_STREQ("llamalog_test.cpp", filename);
}

TEST_F(LlamalogTest, getFilename_HasNoPath_ReturnFilename) {
	constexpr const char* filename = getFilename("llamalog_test.cpp");
	EXPECT_STREQ("llamalog_test.cpp", filename);
}

TEST_F(LlamalogTest, getFilename_IsEmpty_ReturnEmpty) {
	constexpr const char* filename = getFilename("");
	EXPECT_STREQ("", filename);
}


//
// Creation Errors
//

TEST_F(LlamalogTest, initialize_SetThreadPriorityError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, SetThreadPriority(DTGM_ARG2))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_HANDLE, FALSE));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} WARN [^\\n]*Error configuring thread: [^\\n]+ \\(6\\)\\n[^\\n]+Test\\n"));
}

TEST_F(LlamalogTest, initialize_SetThreadInformationError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, SetThreadInformation(DTGM_ARG4))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_HANDLE, FALSE));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} WARN [^\\n]*Error configuring thread: [^\\n]+ \\(6\\)\\n[^\\n]+Test\\n"));
}

//
// Log
//

TEST_F(LlamalogTest, Log_OneLine) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", 7);

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:99 TestBody 7\n"));
}

TEST_F(LlamalogTest, Log_1000Lines) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		for (int i = 0; i < 1000; ++i) {
			llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", i);
		}

		llamalog::shutdown();
	}

	EXPECT_EQ(1000, m_lines);
}


//
// Encoding Error
//

TEST_F(LlamalogTest, Encoding_WideCharToMultiByteError_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_FLAGS, 0));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]*<ERROR>\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length 4: <ERROR> \\(1004\\)\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length \\d+: <ERROR> \\(1004\\)\\n"));
}

TEST_F(LlamalogTest, Encoding_WideCharToMultiByteErrorWithoutFlush_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_FLAGS, 0));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::shutdown();  // shutdown instead of flush before detach

	DTGM_DETACH_API_MOCK(Win32);

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]*<ERROR>\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length 4: <ERROR> \\(1004\\)\\n[0-9:. -]{23} ERROR [^\\n]*WideCharToMultiByte for length \\d+: <ERROR> \\(1004\\)\\n"));
}


//
// Exception during logging
//

TEST_F(LlamalogTest, Exception_ExceptionDuringLogging_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(2)
		.WillOnce(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test");
		}))
		.WillRepeatedly(testing::DoDefault());
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+pop Error writing log: arg=Test @[^\\n]+\\n"));
}

TEST_F(LlamalogTest, Exception_ThrowObjectDuringLogging_LogError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillOnce(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			throw "test error";
		}));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+pop Error writing log\\n"));
}

TEST_F(LlamalogTest, Exception_ExceptionDuringExceptionHandling_LogPanic) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	const std::thread::id threadId = std::this_thread::get_id();

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.WillOnce(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test");
		}))
		.WillRepeatedly(testing::DoDefault());
	EXPECT_CALL(mock, WakeConditionVariable(DTGM_ARG1))
		.WillOnce(testing::DoDefault())  // initial log call
		.WillRepeatedly(testing::Invoke([threadId](PCONDITION_VARIABLE ConditionVariable) -> void {
			if (std::this_thread::get_id() != threadId) {
				// prevent being fired from flush
				throw std::exception("Logging exception");
			}
			// clang-format off
		DTGM_REAL(Win32, WakeConditionVariable)(ConditionVariable);
			// clang-format on
		}));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(1, m_lines);
	// exception happens AFTER the LogLine is added, so the message is logged despite the exception
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+pop Error writing log: arg=Test @[^\\n]+\\n"));
}

TEST_F(LlamalogTest, Exception_ExceptionDuringExceptionLogging_LogLastError) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(3)
		.WillOnce(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception 1"), "arg={}", L"Test 1");
		}))
		.WillOnce(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception 2"), "arg={}", L"Test 2");
		}))
		.WillRepeatedly(testing::DoDefault());
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} ERROR [^\\n]+pop Error writing log: arg=Test 2 @[^\\n]+\\n"));
}

TEST_F(LlamalogTest, Exception_PermamentExceptionDuringExceptionLogging_LogPanic) {
	DTGM_DEFINE_API_MOCK(Win32, mock);

	EXPECT_CALL(mock, WideCharToMultiByte(DTGM_ARG8))
		.Times(3)
		.WillRepeatedly(testing::Invoke([](testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused, testing::Unused) -> BOOL {
			LLAMALOG_THROW(std::exception("Testing exception"), "arg={}", L"Test", "foo");
		}));
	EXPECT_CALL(mock, OutputDebugStringA(testing::StartsWith("PANIC: ")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	llamalog::initialize(std::move(writer));

	llamalog::log(Priority::kDebug, getFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::flush();

	DTGM_DETACH_API_MOCK(Win32);

	llamalog::shutdown();

	EXPECT_EQ(0, m_lines);
	EXPECT_EQ("", m_out.str());
}
//
// Macros
//

TEST_F(LlamalogTest, LOGTRACE_WriterIsDebug_NoOutput) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(0, m_lines);
	EXPECT_EQ("", m_out.str());
}

TEST_F(LlamalogTest, LOGTRACE_WriterIsTrace_Output) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGTRACE_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kTrace, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_TRACE("Test {}", 1);

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1\n"));
}

TEST_F(LlamalogTest, LOGDEBUG) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_DEBUG("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGDEBUG_WitArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_DEBUG("Test {}", std::string("Test"));

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test Test\n"));
}

TEST_F(LlamalogTest, LOGINFO) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_INFO("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGINFO_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		std::string arg("Test");
		LOG_INFO("Test {}", arg);

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test Test\n"));
}

TEST_F(LlamalogTest, LOGWARN) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_WARN("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGWARN_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_WARN("Test {}{}", 1, 's');

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1s\n"));
}

TEST_F(LlamalogTest, LOGERROR) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_ERROR("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGERROR_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_ERROR("Test {}{}{}", 1, "Test", 2);

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1Test2\n"));
}

TEST_F(LlamalogTest, LOGFATAL) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_FATAL("Test");

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
}

TEST_F(LlamalogTest, LOGFATAL_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
		llamalog::initialize(std::move(writer));

		LOG_FATAL("Test {}", 1.1);

		llamalog::shutdown();
	}

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1.1\n"));
}

}  // namespace llamalog::test
