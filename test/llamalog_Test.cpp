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

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace llamalog::test {

namespace {

class LlamalogTest : public testing::Test {
protected:
	std::ostringstream m_out;
	int m_lines = 0;
};


class StringWriter : public LogWriter {
public:
	StringWriter(const LogLevel logLevel, std::ostringstream& out, int& lines)
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
					   FormatLogLevel(logLine.GetLevel()),
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

TEST_F(LlamalogTest, GetFilename) {
	constexpr const char* szFilename = internal::GetFilename(__FILE__);
	EXPECT_STREQ("llamalog_test.cpp", szFilename);
}

TEST_F(LlamalogTest, Log_OneLine) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		llamalog::Log(LogLevel::kDebug, internal::GetFilename(__FILE__), 99, __func__, "{}", 7);

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:99 TestBody 7\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, Log_1000Lines) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		for (int i = 0; i < 1000; ++i) {
			llamalog::Log(LogLevel::kDebug, internal::GetFilename(__FILE__), 99, __func__, "{}", i);
		}

		llamalog::Shutdown();
	}

	EXPECT_EQ(1000, m_lines);
}

TEST_F(LlamalogTest, LOGTRACE_WriterIsDebug_NoOutput) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::Shutdown();
	}

	EXPECT_EQ("", m_out.str());
	EXPECT_EQ(0, m_lines);
}

TEST_F(LlamalogTest, LOGTRACE_WriterIsTrace_Output) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGTRACE_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kTrace, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_TRACE("Test {}", 1);

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d TRACE \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGDEBUG) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_DEBUG("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGDEBUG_WitArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_DEBUG("Test {}", std::string("Test"));

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGINFO) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_INFO("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGINFO_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		std::string arg("Test");
		LOG_INFO("Test {}", arg);

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d INFO \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGWARN) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_WARN("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGWARN_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_WARN("Test {}{}", 1, 's');

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d WARN \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1s\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGERROR) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_ERROR("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGERROR_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_ERROR("Test {}{}{}", 1, "Test", 2);

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d ERROR \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1Test2\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGFATAL) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_FATAL("Test");

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LlamalogTest, LOGFATAL_WithArgs) {
	{
		std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
		llamalog::Initialize(std::move(writer));

		LOG_FATAL("Test {}", 1.1);

		llamalog::Shutdown();
	}

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d FATAL \\[\\d+\\] llamalog_test.cpp:\\d+ TestBody Test 1.1\n"));
	EXPECT_EQ(1, m_lines);
}

}  // namespace llamalog::test
