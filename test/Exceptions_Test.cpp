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

#include "llamalog/Exceptions.h"

#include "llamalog/CustomTypes.h"
#include "llamalog/LogLine.h"

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sal.h>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace llamalog::test {

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

	/// @brief Format the `llamalog::ErrorCode`.
	/// @param arg A `llamalog::ErrorCode`.
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
			return "This is an error message\xE1";
		}
		return "This is a different error message\xE1";
	}
};

static const TestCategory kTestCategory;
static const TestCategoryEscape kTestCategoryEscape;

}  // namespace


//
// SystemError
//

TEST(ExceptionsTest, SystemError_what_GetMessage) {
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "testmsg: This is an error message");
	}
}

TEST(ExceptionsTest, SystemError_whatWithNullptrMessage_GetErrorMessageOnly) {
	try {
		throw SystemError(7, kTestCategory, nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(ExceptionsTest, SystemError_whatWithEmptyMessage_GetErrorMessageOnly) {
	try {
		throw SystemError(7, kTestCategory, "");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "This is an error message");
	}
}

TEST(ExceptionsTest, SystemError_code_GetErrorCode) {
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (SystemError& e) {
		EXPECT_EQ(e.code().value(), 7);
		EXPECT_EQ(&e.code().category(), &kTestCategory);
	}
}


//
// ExceptionDetail
//

TEST(ExceptionsTest, ExceptionDetail_what_GetMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", "MyMessage {}", "myarg");
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "MyMessage myarg");
	}
}

TEST(ExceptionsTest, ExceptionDetail_whatWithNullptrMessage_GetExceptionMessage) {
	try {
		throw internal::ExceptionDetail(std::range_error("testmsg"), "myfile.cpp", 15, "exfunc", nullptr);
	} catch (std::exception& e) {
		EXPECT_STREQ(e.what(), "testmsg");
	}
}

TEST(ExceptionsTest, ExceptionDetail_Formatting_FirstCalledWhenLogging) {
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

TEST(ExceptionsTest, stdcurrentexception_IsStdException_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}

TEST(ExceptionsTest, stdcurrentexception_IsSystemError_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(SystemError(5, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}


//
// std::exception
//

TEST(ExceptionsTest, Throwexception_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, Throwexception_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - test\n@ myfile.cpp:15 yyy", str);
}

TEST(ExceptionsTest, Throwexception_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by Exception 1.8 - xx\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, Throwexception_IsHeapAndExtendBuffer_PrintValue) {
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

TEST(ExceptionsTest, Throwsystemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, Throwsystemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(ExceptionsTest, Throwsystemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, Throwsystemerror_IsHeapAndExtendBuffer_PrintValue) {
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
// SystemError
//

TEST(ExceptionsTest, ThrowSystemError_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, ThrowSystemError_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - test: This is an error message\n@ myfile.cpp:15 yyy", str);
}

TEST(ExceptionsTest, ThrowSystemError_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, ThrowSystemError_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by Exception 1.8 - xx: This is an error message\n@ myfile.cpp:15 yyy", str);
}


//
// std::exception thrown as plain C++ exception
//

TEST(ExceptionsTest, throwexception_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testmsg", str);
}

TEST(ExceptionsTest, throwexception_IsPlainAndExtendBuffer_PrintValue) {
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

TEST(ExceptionsTest, throwsystemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(ExceptionsTest, throwsystemerror_IsPlainAndExtendBuffer_PrintValue) {
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
// SystemError thrown as plain C++ exception
//

TEST(ExceptionsTest, throwSystemError_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(ExceptionsTest, throwSystemError_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw SystemError(7, kTestCategory, "testmsg");
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

TEST(ExceptionsTest, Pattern_IsTWithexceptionStack_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(ExceptionsTest, Pattern_IsTWithexceptionHeap_PrintTimestamp) {
	LogLine logLine = GetLogLine("{:%T}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d"));
}

TEST(ExceptionsTest, Pattern_IsTWithexceptionPlain_PrintNothing) {
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

TEST(ExceptionsTest, Pattern_IstWithexceptionStack_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("\\d+"));
}

TEST(ExceptionsTest, Pattern_IstWithexceptionHeap_PrintThread) {
	LogLine logLine = GetLogLine("{:%t}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("\\d+"));
}

TEST(ExceptionsTest, Pattern_IstWithexceptionPlain_PrintNothing) {
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

TEST(ExceptionsTest, Pattern_IsFWithexceptionStack_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(ExceptionsTest, Pattern_IsFWithexceptionHeap_PrintFile) {
	LogLine logLine = GetLogLine("{:%F}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "myfile.cpp");
}

TEST(ExceptionsTest, Pattern_IsFWithexceptionPlain_PrintNothing) {
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

TEST(ExceptionsTest, Pattern_IsLWithexceptionStack_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(ExceptionsTest, Pattern_IsLWithexceptionHeap_PrintLine) {
	LogLine logLine = GetLogLine("{:%L}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "15");
}

TEST(ExceptionsTest, Pattern_IsLWithexceptionPlain_PrintNothing) {
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

TEST(ExceptionsTest, Pattern_IsfWithexceptionStack_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(ExceptionsTest, Pattern_IsfWithexceptionHeap_PrintFunction) {
	LogLine logLine = GetLogLine("{:%f}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "exfunc");
}

TEST(ExceptionsTest, Pattern_IsfWithexceptionPlain_PrintNothing) {
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

TEST(ExceptionsTest, Pattern_IslWithexceptionStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithexceptionStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IslWithexceptionHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IslWithsystemerrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithsystemerrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IslWithsystemerrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithsystemerrorPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IslWithSystemErrorStack_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithSystemErrorStackAndNoMessage_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IslWithSystemErrorHeap_PrintLogMessge) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IslWithSystemErrorPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%l}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}


// what()

TEST(ExceptionsTest, Pattern_IswWithexceptionStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IswWithexceptionStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(ExceptionsTest, Pattern_IswWithexceptionHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test");
}

TEST(ExceptionsTest, Pattern_IswWithexceptionPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg");
}

TEST(ExceptionsTest, Pattern_IswWithsystemerrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}


TEST(ExceptionsTest, Pattern_IswWithsystemerrorStackAndNoMessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithsystemerrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithsystemerrorPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithSystemErrorStack_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithSystemErrorStackAndNomessage_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithSystemErrorHeap_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "Exception 1.8 - test: This is an error message");
}

TEST(ExceptionsTest, Pattern_IswWithSystemErrorPlain_PrintWhat) {
	LogLine logLine = GetLogLine("{:%w}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "testmsg: This is an error message");
}


// Error Code

TEST(ExceptionsTest, Pattern_IscWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IscWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IscWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IscWithsystemerrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(ExceptionsTest, Pattern_IscWithsystemerrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(ExceptionsTest, Pattern_IscWithsystemerrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(ExceptionsTest, Pattern_IscWithSystemErrorStack_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(ExceptionsTest, Pattern_IscWithSystemErrorHeap_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}

TEST(ExceptionsTest, Pattern_IscWithSystemErrorPlain_PrintCodeValue) {
	LogLine logLine = GetLogLine("{:%c}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "7");
}


// Error Category

TEST(ExceptionsTest, Pattern_IsCWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsCWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsCWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsCWithsystemerrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(ExceptionsTest, Pattern_IsCWithsystemerrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(ExceptionsTest, Pattern_IsCWithsystemerrorPlain_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(ExceptionsTest, Pattern_IsCWithSystemErrorStack_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(ExceptionsTest, Pattern_IsCWithSystemErrorHeap_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}

TEST(ExceptionsTest, Pattern_IsCWithSystemErrorPlain_PrintCategory) {
	LogLine logLine = GetLogLine("{:%C}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "TestError");
}


// Error Message

TEST(ExceptionsTest, Pattern_IsmWithexceptionStack_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsmWithexceptionHeap_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsmWithexceptionPlain_PrintNothing) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "");
}

TEST(ExceptionsTest, Pattern_IsmWithsystemerrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(ExceptionsTest, Pattern_IsmWithsystemerrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(ExceptionsTest, Pattern_IsmWithsystemerrorPlain_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(ExceptionsTest, Pattern_IsmWithSystemErrorStack_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(ExceptionsTest, Pattern_IsmWithSystemErrorHeap_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test", std::string(512, 'x'));
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

TEST(ExceptionsTest, Pattern_IsmWithSystemErrorPlain_PrintErrorMessage) {
	LogLine logLine = GetLogLine("{:%m}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(str, "This is an error message");
}

//
// Default Pattern
//

TEST(ExceptionsTest, DefaultPattern_Isexception_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error Exception 1\\.8 - test @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_IsexceptionAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error testmsg @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_IsexceptionPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg", str);
}

TEST(ExceptionsTest, DefaultPattern_Issystemerror_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_IssystemerrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_IssystemerrorPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}

TEST(ExceptionsTest, DefaultPattern_IsSystemError_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error Exception 1\\.8 - test: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_IsSystemErrorAndNoMessage_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\) @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, DefaultPattern_SystemErrorPlain_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}


//
// Encoding
//

TEST(ExceptionsTest, Encoding_exceptionHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg\xE2"), "myfile.cpp", 15, "exfunc", "Exception\xE5 {} - {}", 1.8, L"test\xE3");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 caused by Exception\xE5 1.8 - test\\xC3\\xA3\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, Encoding_exceptionPlainHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::invalid_argument("testmsg\xE2");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 caused by testmsg\\xE2", str);
}

TEST(ExceptionsTest, Encoding_systemerrorPlainHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw std::system_error(7, kTestCategoryEscape, "testmsg\xE2");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 TestError\\xE0 (7=Error\\xE4\xE6) caused by testmsg\\xE2: This is an error message\\xE1", str);
}

TEST(ExceptionsTest, Encoding_SystemErrorPlainHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%w}{1:%[\n@ %F:%L]}{2:.4}");
	try {
		throw SystemError(7, kTestCategoryEscape, "testmsg\xE2");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 TestError\\xE0 (7=Error\\xE4\xE6) caused by testmsg\\xE2: This is an error message\\xE1", str);
}

//
// Sub-Sub-Format
//

TEST(ExceptionsTest, SubFormat_Isexception_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::invalid_argument("testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("myfile.cpp Exception 1.8 - test", str);
}

TEST(ExceptionsTest, SubFormat_IsexceptionPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::invalid_argument("testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("testmsg", str);
}

TEST(ExceptionsTest, SubFormat_Issystemerror_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(std::system_error(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(ExceptionsTest, SubFormat_IssystemerrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw std::system_error(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

TEST(ExceptionsTest, SubFormat_IsSystemError_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		llamalog::Throw(SystemError(7, kTestCategory, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp Exception 1.8 - test: This is an error message", str);
}

TEST(ExceptionsTest, SubFormat_IsSystemErrorPlain_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%w}");
	try {
		throw SystemError(7, kTestCategory, "testmsg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

}  // namespace llamalog::test
