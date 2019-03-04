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

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"
#include "llamalog/llamalog.h"

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <memory>
#include <sstream>
#include <utility>

namespace llamalog::test {

namespace {

class LogTest : public testing::Test {
protected:
	std::ostringstream m_out;
	int m_lines = 0;
};


namespace {
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

}  // namespace

TEST_F(LogTest, Log) {
	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	llamalog::LogX(LogLevel::kDebug, "file.cpp", 99, "myfunction()", "{}", 7);
	llamalog::Shutdown();

	EXPECT_THAT(m_out.str(), testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d DEBUG \\[\\d+\\] file.cpp:99 myfunction\\(\\) 7\n"));
	EXPECT_EQ(1, m_lines);
}

TEST_F(LogTest, Log2) {
	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(LogLevel::kDebug, m_out, m_lines);
	llamalog::Initialize(std::move(writer));

	for (int i = 0; i < 1000; ++i) {
		llamalog::LogX(LogLevel::kDebug, "file.cpp", 99, "myfunction()", "{}", i);
	}
	llamalog::Shutdown();

	EXPECT_EQ(1000, m_lines);
}

}  // namespace llamalog::test
