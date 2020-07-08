/*
Copyright 2019 Michael Beckh

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

#include "llamalog/exception.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"
#include "llamalog/Logger.h"
#include "llamalog/custom_types.h"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace llamalog::test {

namespace t = testing;

struct TracingArg {
public:
	TracingArg(int* p) noexcept
		: pCalled(p) {
		// empty
	}
	TracingArg(const TracingArg&) noexcept = default;
	TracingArg(TracingArg&&) noexcept = default;
	~TracingArg() noexcept = default;
	int* pCalled;
};

#ifdef __clang_analyzer__
// make clang happy and define in namespace for ADL. MSVC can't find correct overload when the declaration is present.
LogLine& operator<<(LogLine& logLine, const TracingArg& arg) {
	return logLine.AddCustomArgument(arg);
}
#endif

}  // namespace llamalog::test

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::TracingArg& arg) {
	return logLine.AddCustomArgument(arg);
}

template <>
struct fmt::formatter<llamalog::test::TracingArg> {
public:
	/// @brief Parse the format string.
	/// @param ctx see `fmt::formatter::parse`.
	/// @return see `fmt::formatter::parse`.
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		return ctx.begin();
	}

	/// @brief Format the `llamalog::error_code`.
	/// @param arg A `llamalog::error_code`.
	/// @param ctx see `fmt::formatter::format`.
	/// @return see `fmt::formatter::format`.
	fmt::format_context::iterator format(const llamalog::test::TracingArg& arg, fmt::format_context& ctx) {
		std::string_view sv("TracingArg");
		(*arg.pCalled)++;
		return std::copy(sv.cbegin(), sv.cend(), ctx.out());
	}
};

namespace llamalog::test {
namespace {

LogLine GetLogLine(const char* const pattern = "{0} {1:%[%C (%c={0}) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}") {
	return LogLine(Priority::kDebug, "file.cpp", 99, "myfunction", pattern);
}

class TestCategory : public std::error_category {
public:
	_Ret_z_ const char* name() const noexcept final {
		callsToName++;
		return "TestError";
	}

	std::string message(const int code) const final {
		callsToMessage++;
		if (code == 7) {
			return "This is an error message";
		}
		if (code == 666) {
			throw std::runtime_error("runtime error in message()");
		}
		if (code == 667) {
			throw "anything";
		}
		return "This is a different error message";
	}

	mutable int callsToName = 0;
	mutable int callsToMessage = 0;
};

class TestCategoryEscape : public std::error_category {
public:
	_Ret_z_ const char* name() const noexcept final {
		return "TestError\xE0";
	}

	std::string message(const int code) const final {
		if (code == 7) {
			return "This is an error message\r\xE1";
		}
		return "This is a different error message\r\xE1";
	}
};

static const TestCategory kTestCategory;
static const TestCategoryEscape kTestCategoryEscape;

#pragma warning(suppress : 4100)
MATCHER_P(MatchesRegex, pattern, "") {
	return std::regex_match(arg, std::regex(pattern));
}

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


//
// system_error
//

TEST(exception_Test, systemerror_what_GetMessage) {
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;

		EXPECT_STREQ(e.what(), "testmsg: This is an error message");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// uses cached message
		EXPECT_STREQ(e.what(), "testmsg: This is an error message");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);
	}
}

TEST(exception_Test, systemerror_whatWithMessage_GetMessage) {
	try {
		LLAMALOG_THROW(system_error(7, kTestCategory, "testmsg"), "MyMessage {}", 7);
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;

		EXPECT_STREQ(e.what(), "MyMessage 7: This is an error message");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// uses cached message
		EXPECT_STREQ(e.what(), "MyMessage 7: This is an error message");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);
	}
}

TEST(exception_Test, systemerror_whatThrowsException_GetError) {
	std::ostringstream out;
	int lines = 0;

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, out, lines);
	llamalog::Initialize(std::move(writer));

	try {
		throw system_error(666, kTestCategory, "testmsg");
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// no caching
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 2, kTestCategory.callsToMessage);
	}

	llamalog::Shutdown();

	EXPECT_EQ(2, lines);
	EXPECT_THAT(out.str(), MatchesRegex("([\\d .:-]{23} ERROR \\[\\d+\\] exception.cpp:\\d+ what Error creating exception message: runtime error in message\\(\\)\\n){2}"));
}

TEST(exception_Test, systemerror_whatWithMessageThrowsException_GetError) {
	std::ostringstream out;
	int lines = 0;

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, out, lines);
	llamalog::Initialize(std::move(writer));

	try {
		LLAMALOG_THROW(system_error(666, kTestCategory, "testmsg"), "MyMessage {}", 7);
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// no caching
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 2, kTestCategory.callsToMessage);
	}

	llamalog::Shutdown();

	EXPECT_EQ(2, lines);
	EXPECT_THAT(out.str(), MatchesRegex("([\\d .:-]{23} ERROR \\[\\d+\\] exception.cpp:\\d+ What Error creating exception message: runtime error in message\\(\\)\\n){2}"));
}
TEST(exception_Test, systemerror_whatThrowsPointer_GetError) {
	std::ostringstream out;
	int lines = 0;

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, out, lines);
	llamalog::Initialize(std::move(writer));

	try {
		throw system_error(667, kTestCategory, "testmsg");
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;

		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// no caching
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 2, kTestCategory.callsToMessage);
	}

	llamalog::Shutdown();

	EXPECT_EQ(2, lines);
	EXPECT_THAT(out.str(), MatchesRegex("([\\d .:-]{23} ERROR \\[\\d+\\] exception.cpp:\\d+ what Error creating exception message\\n){2}"));
}

TEST(exception_Test, systemerror_whatWithMessageThrowsPointer_GetError) {
	std::ostringstream out;
	int lines = 0;

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, out, lines);
	llamalog::Initialize(std::move(writer));

	try {
		LLAMALOG_THROW(system_error(667, kTestCategory, "testmsg"), "MyMessage {}", 7);
	} catch (std::exception& e) {
		const int callsBefore = kTestCategory.callsToMessage;

		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 1, kTestCategory.callsToMessage);

		// no caching
		EXPECT_STREQ(e.what(), "<ERROR>");
		EXPECT_EQ(callsBefore + 2, kTestCategory.callsToMessage);
	}

	llamalog::Shutdown();

	EXPECT_EQ(2, lines);
	EXPECT_THAT(out.str(), MatchesRegex("([\\d .:-]{23} ERROR \\[\\d+\\] exception.cpp:\\d+ What Error creating exception message\\n){2}"));
}

TEST(exception_Test, systemerror_whatWithNullptrMessage_GetErrorMessageOnly) {
	try {
		throw system_error(7, kTestCategory, nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(exception_Test, systemerror_whatWithEmptyMessage_GetErrorMessageOnly) {
	try {
		throw system_error(7, kTestCategory, "");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(exception_Test, systemerror_code_GetErrorCode) {
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (system_error& e) {
		EXPECT_EQ(e.code().value(), 7);
		EXPECT_EQ(&e.code().category(), &kTestCategory);
	}
}


//
// ExceptionDetail
//

TEST(exception_Test, ExceptionDetail_what_GetMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", "MyMessage {}", "myarg");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "MyMessage myarg");
	}
}

TEST(exception_Test, ExceptionDetail_whatWithNullptrMessage_GetExceptionMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "testmsg");
	}
}

TEST(exception_Test, ExceptionDetail_Formatting_FirstCalledWhenLogging) {
	LogLine logLine = GetLogLine();
	int called = 0;
	try {
		llamalog::Throw(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", "MyMessage {}", TracingArg(&called));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	EXPECT_EQ(0, called);
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ(1, called);

	EXPECT_EQ("Error caused by MyMessage TracingArg\n@ myfile.cpp:15", str);
}


//
// std::current_exception
//

TEST(exception_Test, stdcurrentexception_IsStdException_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}

TEST(exception_Test, stdcurrentexception_Issystemerror_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(system_error(5, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}


//
// std::exception
//

TEST(exception_Test, Throwexception_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwexception_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15 yyy", str);
}

TEST(exception_Test, Throwexception_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - xx\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwexception_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - xx\n@ myfile.cpp:15 yyy", str);
}


//
// std::system_error
//

TEST(exception_Test, Throwstdsystemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwstdsystemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(exception_Test, Throwstdsystemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwstdsystemerror_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15 yyy", str);
}


//
// system_error
//

TEST(exception_Test, Throwsystemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwsystemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(exception_Test, Throwsystemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Throwsystemerror_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15 yyy", str);
}


//
// std::exception thrown as plain C++ exception
//

TEST(exception_Test, throwexception_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testmsg", str);
}

TEST(exception_Test, throwexception_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}

	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testmsg yyy", str);
}


//
// std::system_error thrown as plain C++ exception
//

TEST(exception_Test, throwstdsystemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(exception_Test, throwstdsystemerror_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}

	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message yyy", str);
}


//
// system_error thrown as plain C++ exception
//

TEST(exception_Test, throwsystemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(exception_Test, throwsystemerror_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}

	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message yyy", str);
}


//
// Pattern
//

// Timestamp

TEST(exception_Test, Pattern_IsTWithexceptionStack_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(exception_Test, Pattern_IsTWithexceptionHeap_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(exception_Test, Pattern_IsTWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// thread

TEST(exception_Test, Pattern_IstWithexceptionStack_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d+"));
}

TEST(exception_Test, Pattern_IstWithexceptionHeap_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d+"));
}

TEST(exception_Test, Pattern_IstWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// File

TEST(exception_Test, Pattern_IsFWithexceptionStack_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(exception_Test, Pattern_IsFWithexceptionHeap_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(exception_Test, Pattern_IsFWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// Line

TEST(exception_Test, Pattern_IsLWithexceptionStack_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(exception_Test, Pattern_IsLWithexceptionHeap_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(exception_Test, Pattern_IsLWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// Function

TEST(exception_Test, Pattern_IsfWithexceptionStack_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(exception_Test, Pattern_IsfWithexceptionHeap_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(exception_Test, Pattern_IsfWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// Log Message

TEST(exception_Test, Pattern_IslWithexceptionStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithexceptionStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IslWithexceptionHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IslWithstdsystemerrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithstdsystemerrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IslWithstdsystemerrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithstdsystemerrorPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IslWithsystemerrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithsystemerrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IslWithsystemerrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IslWithsystemerrorPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// what()

TEST(exception_Test, Pattern_IswWithexceptionStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IswWithexceptionStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(exception_Test, Pattern_IswWithexceptionHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exception_Test, Pattern_IswWithexceptionPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(exception_Test, Pattern_IswWithstdsystemerrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}


TEST(exception_Test, Pattern_IswWithstdsystemerrorStackAndNoMessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exception_Test, Pattern_IswWithstdsystemerrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exception_Test, Pattern_IswWithstdsystemerrorPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exception_Test, Pattern_IswWithsystemerrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exception_Test, Pattern_IswWithsystemerrorStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exception_Test, Pattern_IswWithsystemerrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exception_Test, Pattern_IswWithsystemerrorPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}


// Error Code

TEST(exception_Test, Pattern_IscWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IscWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IscWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorStackHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorHeapHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithstdsystemerrorPlainHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::system_error(E_INVALIDARG, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exception_Test, Pattern_IscWithsystemerrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithsystemerrorStackHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exception_Test, Pattern_IscWithsystemerrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithsystemerrorHeapHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exception_Test, Pattern_IscWithsystemerrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exception_Test, Pattern_IscWithsystemerrorPlainHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw system_error(E_INVALIDARG, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}


// Error Category

TEST(exception_Test, Pattern_IsCWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsCWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsCWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsCWithstdsystemerrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exception_Test, Pattern_IsCWithstdsystemerrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exception_Test, Pattern_IsCWithstdsystemerrorPlain_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exception_Test, Pattern_IsCWithsystemerrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exception_Test, Pattern_IsCWithsystemerrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exception_Test, Pattern_IsCWithsystemerrorPlain_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}


// Error Message

TEST(exception_Test, Pattern_IsmWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsmWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsmWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exception_Test, Pattern_IsmWithstdsystemerrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exception_Test, Pattern_IsmWithstdsystemerrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exception_Test, Pattern_IsmWithstdsystemerrorPlain_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exception_Test, Pattern_IsmWithsystemerrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exception_Test, Pattern_IsmWithsystemerrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exception_Test, Pattern_IsmWithsystemerrorPlain_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

//
// Default Pattern
//

TEST(exception_Test, DefaultPattern_Isexception_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_IsexceptionAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_IsexceptionPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg", str);
}

TEST(exception_Test, DefaultPattern_Isstdsystemerror_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_IsstdsystemerrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_IsstdsystemerrorPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}

TEST(exception_Test, DefaultPattern_Issystemerror_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_IssystemerrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exception_Test, DefaultPattern_systemerrorPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}


//
// Encoding
//

TEST(exception_Test, Encoding_exceptionHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg\xE2\t"), "myfile.cpp", 15, "exfunc", "Exception\xE5 {} - {}", 1.8, escape(L"test\xE3\t"));
	} catch (const std::exception& e) {
		logLine << escape("Error\xE4\n")
				<< escape(e) << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n caused by Exception\xE5 1.8 - test\xC3\xA3\\t\n@ myfile.cpp:15", str);
}

TEST(exception_Test, Encoding_exceptionPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::invalid_argument("testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << escape("Error\xE4\n")
				<< escape(e) << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n caused by testmsg\xE2\\t", str);
}

TEST(exception_Test, Encoding_stdsystemerrorPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::system_error(7, kTestCategoryEscape, "testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << escape("Error\xE4\n")
				<< escape(e) << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n TestError\xE0 (7=Error\xE4\\n\xE6) caused by testmsg\xE2\\t: This is an error message\\r\xE1", str);
}

TEST(exception_Test, Encoding_systemerrorPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw system_error(7, kTestCategoryEscape, "testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << escape("Error\xE4\n")
				<< escape(e) << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n TestError\xE0 (7=Error\xE4\\n\xE6) caused by testmsg\xE2\\t: This is an error message\\r\xE1", str);
}

//
// Sub-Sub-Format
//

TEST(exception_Test, SubFormat_Isexception_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("myfile.cpp Exception 1.8 - test", str);
}

TEST(exception_Test, SubFormat_IsexceptionPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("testmsg", str);
}

TEST(exception_Test, SubFormat_Isstdsystemerror_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(exception_Test, SubFormat_IsstdsystemerrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

TEST(exception_Test, SubFormat_Issystemerror_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(exception_Test, SubFormat_IssystemerrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

TEST(exception_Test, GetCurrentExceptionAsBaseException_IsBaseException_ReturnPointer) {
	try {
		LLAMALOG_THROW(system_error(7, kTestCategory, "testmsg"));
	} catch (const std::exception&) {
		EXPECT_NE(nullptr, GetCurrentExceptionAsBaseException());
	}
}

TEST(exception_Test, GetCurrentExceptionAsBaseException_IsPlainException_ReturnNullptr) {
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception&) {
		EXPECT_EQ(nullptr, GetCurrentExceptionAsBaseException());
	}
}

}  // namespace llamalog::test
