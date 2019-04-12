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
#include "llamalog/llamalog.h"

#include <gtest/gtest.h>

#include <exception>
#include <string>
#include <system_error>

namespace llamalog::test {

namespace {

LogLine GetLogLine(const char* const pattern = "{0} {1:%[%C (%c={0}) ]}caused by {1:%e}{1:%[: %m\n@ %F:%L]}{2:.4}") {
	return LogLine(Priority::kDebug, "file.cpp", 99, "myfunction", pattern);
}

class TestCategory : public std::error_category {
public:
	_Ret_z_ const char* name() const noexcept final {
		return "TestError";
	}

	std::string message(int) const final {
		return "This is an error message";
	}
};

class TestCategoryEscape : public std::error_category {
public:
	_Ret_z_ const char* name() const noexcept final {
		return "TestError\xE0";
	}

	std::string message(int) const final {
		return "This is an error message\xE1";
	}
};

static const TestCategory kTestCategory;
static const TestCategoryEscape kTestCategoryEscape;

class TestError : public std::system_error {
public:
	TestError(const int errorCode, _In_z_ const char* const message)
		: std::system_error(errorCode, kTestCategory, message) {
		// empty
	}
};

class TestErrorEscape : public std::system_error {
public:
	TestErrorEscape(const int errorCode, _In_z_ const char* const message)
		: std::system_error(errorCode, kTestCategoryEscape, message) {
		// empty
	}
};

}  // namespace


TEST(ExceptionsTest, stdcurrentexception_Save_HasValue) {
	std::exception_ptr eptr;
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception&) {
		eptr = std::current_exception();
	}
	EXPECT_NE(nullptr, eptr);
}


//
// std::exception
//

TEST(ExceptionsTest, exception_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg: Exception 1.8 - test\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, exception_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg: Exception 1.8 - test\n@ myfile.cpp:15 yyy", str);
}

TEST(ExceptionsTest, exception_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg: Exception 1.8 - xx\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, exception_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg: Exception 1.8 - xx\n@ myfile.cpp:15 yyy", str);
}


//
// std::system_error
//

TEST(ExceptionsTest, systemerror_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::system_error& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message: Exception 1.8 - test\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, systemerror_IsInlineAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::system_error& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message: Exception 1.8 - test\n@ myfile.cpp:15 yyy", str);
}

TEST(ExceptionsTest, systemerror_IsHeap_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::system_error& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message: Exception 1.8 - xx\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, systemerror_IsHeapAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {:.2}", 1.8, std::string(256, 'x'));
	} catch (const std::system_error& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message: Exception 1.8 - xx\n@ myfile.cpp:15 yyy", str);
}


//
// std::system_error caught as std::exception
//

TEST(ExceptionsTest, systemerror_AsException_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message: Exception 1.8 - test\n@ myfile.cpp:15", str);
}


//
// std::exception thrown as plain C++ exception
//

TEST(ExceptionsTest, exception_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testarg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg", str);
}

TEST(ExceptionsTest, exception_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw std::invalid_argument("testarg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}

	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error caused by testarg yyy", str);
}


//
// std::system_error thrown as plain C++ exception
//

TEST(ExceptionsTest, systemerror_IsPlain_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw TestError(7, "testmsg");
	} catch (const std::system_error& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

TEST(ExceptionsTest, systemerror_IsPlainAndExtendBuffer_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw TestError(7, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << " " + std::string(1024, 'y');
	}

	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message yyy", str);
}


//
// std::system_error thrown as plain C++ exception caught as std::exception
//

TEST(ExceptionsTest, systemerror_IsPlainAsException_PrintValue) {
	LogLine logLine = GetLogLine();
	try {
		throw TestError(7, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error TestError (7=Error) caused by testmsg: This is an error message", str);
}

//
// Default Pattern
//

TEST(ExceptionsTest, exception_WithDefaultPattern_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error testarg; Exception 1\\.8 - test @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, systemerror_WithDefaultPattern_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, testing::MatchesRegex("Error testmsg: This is an error message \\(TestError 7\\); Exception 1\\.8 - test @\\{\\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d \\[\\d+\\] myfile.cpp:15 exfunc\\}"));
}

TEST(ExceptionsTest, exception_IsPlainWithDefaultPattern_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw std::invalid_argument("testarg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testarg", str);
}

TEST(ExceptionsTest, systemerror_IsPlainWithDefaultPattern_PrintDefault) {
	LogLine logLine = GetLogLine("{} {}");
	try {
		throw TestError(7, "testmsg");
	} catch (const std::exception& e) {
		logLine << "Error" << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error testmsg: This is an error message (TestError 7)", str);
}


//
// Encoding
//

TEST(ExceptionsTest, exception_HasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%e}{1:%[: %m\n@ %F:%L]}{2:.4}");
	try {
		llamalog::Throw(std::invalid_argument("testarg\xE2"), "myfile.cpp", 15, "exfunc", "Exception\xE5 {} - {}", 1.8, L"test\xE3");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 caused by testarg\\xE2: Exception\xE5 1.8 - test\\xC3\\xA3\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, systemerror_HasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%e}{1:%[: %m\n@ %F:%L]}{2:.4}");
	try {
		llamalog::Throw(TestErrorEscape(7, "testmsg\xE2"), "myfile.cpp", 15, "exfunc", "Exception\xE5 {} - {}", 1.8, L"test\xE3");
	} catch (const std::system_error& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 TestError\\xE0 (7=Error\\xE4\xE6) caused by testmsg\\xE2: This is an error message\\xE1: Exception\xE5 1.8 - test\\xC3\\xA3\n@ myfile.cpp:15", str);
}

TEST(ExceptionsTest, exception_IsPlainHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%e}{1:%[: %m\n@ %F:%L]}{2:.4}");
	try {
		throw std::invalid_argument("testarg\xE2");
	} catch (const std::exception& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 caused by testarg\\xE2", str);
}

TEST(ExceptionsTest, systemerror_IsPlainHasHexChar_PrintEscaped) {
	LogLine logLine = GetLogLine("{0} {1:%[%C (%c={0}\xE6) ]}caused by {1:%e}{1:%[: %m\n@ %F:%L]}{2:.4}");
	try {
		throw TestErrorEscape(7, "testmsg\xE2");
	} catch (const std::system_error& e) {
		logLine << "Error\xE4" << e << "";
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Error\\xE4 TestError\\xE0 (7=Error\\xE4\xE6) caused by testmsg\\xE2: This is an error message\\xE1", str);
}

//
// Sub-Sub-Format
//

TEST(ExceptionsTest, exception_WithSubSubFormat_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%e}");
	try {
		llamalog::Throw(std::invalid_argument("testarg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("myfile.cpp testarg", str);
}

TEST(ExceptionsTest, systemerror_HasSubSubFormat_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%e}");
	try {
		llamalog::Throw(TestError(7, "testmsg"), "myfile.cpp", 15, "exfunc", "Exception {} - {}", 1.8, "test");
	} catch (const std::system_error& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError myfile.cpp testmsg: This is an error message", str);
}

TEST(ExceptionsTest, exception_IsPlainWithSubSubFormat_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%e}");
	try {
		throw std::invalid_argument("testarg");
	} catch (const std::exception& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("testarg", str);
}

TEST(ExceptionsTest, systemerror_IsPlainHasSubSubFormat_PrintValue) {
	LogLine logLine = GetLogLine("{0:%[%[%C ]%[%F ]]%e}");
	try {
		throw TestError(7, "testmsg");
	} catch (const std::system_error& e) {
		logLine << e;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("TestError testmsg: This is an error message", str);
}

}  // namespace llamalog::test
