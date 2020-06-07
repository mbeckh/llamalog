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

#include "llamalog/exception.h"

#include "llamalog/LogLine.h"
#include "llamalog/custom_types.h"

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace llamalog::test {

namespace t = testing;

struct TracingArg {
public:
	TracingArg(bool* p) noexcept
		: pCalled(p) {
		// empty
	}
	TracingArg(const TracingArg&) noexcept = default;
	TracingArg(TracingArg&&) noexcept = default;
	~TracingArg() noexcept = default;
	bool* pCalled;
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
		*arg.pCalled = true;
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
		return "TestError";
	}

	std::string message(const int code) const final {
		if (code == 7) {
			return "This is an error message";
		}
		return "This is a different error message";
	}
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

}  // namespace


//
// system_error
//

TEST(exceptionTest, systemerror_what_GetMessage) {
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "testmsg: This is an error message");
	}
}

TEST(exceptionTest, systemerror_whatWithNullptrMessage_GetErrorMessageOnly) {
	try {
		throw system_error(7, kTestCategory, nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(exceptionTest, systemerror_whatWithEmptyMessage_GetErrorMessageOnly) {
	try {
		throw system_error(7, kTestCategory, "");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(exceptionTest, systemerror_code_GetErrorCode) {
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

TEST(exceptionTest, ExceptionDetail_what_GetMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", "MyMessage {}", "myarg");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "MyMessage myarg");
	}
}

TEST(exceptionTest, ExceptionDetail_whatWithNullptrMessage_GetExceptionMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "testmsg");
	}
}

TEST(exceptionTest, ExceptionDetail_Formatting_FirstCalledWhenLogging) {
	LogLine logLine = GetLogLine();
	bool called = false;
	try {
		llamalog::Throw(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", "MyMessage {}", TracingArg(&called));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	EXPECT_EQ(false, called);
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ(true, called);

	EXPECT_EQ("Error caused by MyMessage TracingArg\n@ myfile.cpp:15", str);
}


//
// std::current_exception
//

TEST(exceptionTest, stdcurrentexception_IsStdException_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}

TEST(exceptionTest, stdcurrentexception_Issystemerror_HasValue) {
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

TEST(exceptionTest, Throwexception_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwexception_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15 yyy", str);
}

TEST(exceptionTest, Throwexception_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - xx\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwexception_IsHeapAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, Throwstdsystemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwstdsystemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(exceptionTest, Throwstdsystemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwstdsystemerror_IsHeapAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, Throwsystemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwsystemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(exceptionTest, Throwsystemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Throwsystemerror_IsHeapAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, throwexception_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testmsg", str);
}

TEST(exceptionTest, throwexception_IsPlainAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, throwstdsystemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(exceptionTest, throwstdsystemerror_IsPlainAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, throwsystemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(exceptionTest, throwsystemerror_IsPlainAndExtendBuffer_PrintValue) {
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

TEST(exceptionTest, Pattern_IsTWithexceptionStack_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(exceptionTest, Pattern_IsTWithexceptionHeap_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(exceptionTest, Pattern_IsTWithexceptionPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IstWithexceptionStack_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d+"));
}

TEST(exceptionTest, Pattern_IstWithexceptionHeap_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("\\d+"));
}

TEST(exceptionTest, Pattern_IstWithexceptionPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IsFWithexceptionStack_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(exceptionTest, Pattern_IsFWithexceptionHeap_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(exceptionTest, Pattern_IsFWithexceptionPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IsLWithexceptionStack_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(exceptionTest, Pattern_IsLWithexceptionHeap_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(exceptionTest, Pattern_IsLWithexceptionPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IsfWithexceptionStack_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(exceptionTest, Pattern_IsfWithexceptionHeap_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(exceptionTest, Pattern_IsfWithexceptionPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IslWithexceptionStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithexceptionStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IslWithexceptionHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IslWithstdsystemerrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithstdsystemerrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IslWithstdsystemerrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithstdsystemerrorPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IslWithsystemerrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithsystemerrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IslWithsystemerrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IslWithsystemerrorPlain_PrintNothing) {
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

TEST(exceptionTest, Pattern_IswWithexceptionStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IswWithexceptionStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(exceptionTest, Pattern_IswWithexceptionHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(exceptionTest, Pattern_IswWithexceptionPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(exceptionTest, Pattern_IswWithstdsystemerrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}


TEST(exceptionTest, Pattern_IswWithstdsystemerrorStackAndNoMessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithstdsystemerrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithstdsystemerrorPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithsystemerrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithsystemerrorStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithsystemerrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(exceptionTest, Pattern_IswWithsystemerrorPlain_PrintWhat) {
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

TEST(exceptionTest, Pattern_IscWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IscWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IscWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorStackHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorHeapHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithstdsystemerrorPlainHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::system_error(E_INVALIDARG, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorStackHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorHeapHRESULT_PrintCodeValueHex) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(system_error(E_INVALIDARG, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "0x80070057");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(exceptionTest, Pattern_IscWithsystemerrorPlainHRESULT_PrintCodeValueHex) {
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

TEST(exceptionTest, Pattern_IsCWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsCWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsCWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsCWithstdsystemerrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exceptionTest, Pattern_IsCWithstdsystemerrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exceptionTest, Pattern_IsCWithstdsystemerrorPlain_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exceptionTest, Pattern_IsCWithsystemerrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exceptionTest, Pattern_IsCWithsystemerrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(exceptionTest, Pattern_IsCWithsystemerrorPlain_PrintCategory) {
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

TEST(exceptionTest, Pattern_IsmWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsmWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsmWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(exceptionTest, Pattern_IsmWithstdsystemerrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exceptionTest, Pattern_IsmWithstdsystemerrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exceptionTest, Pattern_IsmWithstdsystemerrorPlain_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exceptionTest, Pattern_IsmWithsystemerrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exceptionTest, Pattern_IsmWithsystemerrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(exceptionTest, Pattern_IsmWithsystemerrorPlain_PrintErrorMessage) {
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

TEST(exceptionTest, DefaultPattern_Isexception_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_IsexceptionAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_IsexceptionPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg", str);
}

TEST(exceptionTest, DefaultPattern_Isstdsystemerror_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_IsstdsystemerrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_IsstdsystemerrorPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}

TEST(exceptionTest, DefaultPattern_Issystemerror_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_IssystemerrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(exceptionTest, DefaultPattern_systemerrorPlain_PrintDefault) {
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

TEST(exceptionTest, Encoding_exceptionHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg\xE2\t"), "myfile.cpp", 15, "exfunc", "Exception\xE5 {} - {}", 1.8, L"test\xE3\t");
	} catch (const std::exception& e) {
		logLine << "Error\xE4\n"
				<< e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n caused by Exception\xE5 1.8 - test\xC3\xA3\\t\n@ myfile.cpp:15", str);
}

TEST(exceptionTest, Encoding_exceptionPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::invalid_argument("testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << "Error\xE4\n"
				<< e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n caused by testmsg\xE2\\t", str);
}

TEST(exceptionTest, Encoding_stdsystemerrorPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::system_error(7, kTestCategoryEscape, "testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << "Error\xE4\n"
				<< e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n TestError\xE0 (7=Error\xE4\\n\xE6) caused by testmsg\xE2\\t: This is an error message\\r\xE1", str);
}

TEST(exceptionTest, Encoding_systemerrorPlainHasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw system_error(7, kTestCategoryEscape, "testmsg\xE2\t");
	} catch (const std::exception& e) {
		logLine << "Error\xE4\n"
				<< e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\xE4\\n TestError\xE0 (7=Error\xE4\\n\xE6) caused by testmsg\xE2\\t: This is an error message\\r\xE1", str);
}

//
// Sub-Sub-Format
//

TEST(exceptionTest, SubFormat_Isexception_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("myfile.cpp Exception 1.8 - test", str);
}

TEST(exceptionTest, SubFormat_IsexceptionPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("testmsg", str);
}

TEST(exceptionTest, SubFormat_Isstdsystemerror_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(exceptionTest, SubFormat_IsstdsystemerrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

TEST(exceptionTest, SubFormat_Issystemerror_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(exceptionTest, SubFormat_IssystemerrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

}  // namespace llamalog::test
