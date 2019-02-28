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

#include <llamalog/LogLine.h>

#include <gmock/gmock.h>

#include <cinttypes>

#if 0
#include "Foo.h"
#include "stdafx.h"

#include <m3c/ArgumentFormatter.h>
#include <m3c/ArgumentFormatterCOM.h>
#include <m3c/ArgumentFormatterWIC.h>
#include <m3c/com_ptr.h>
#include <m3c/finally.h>
#include <m3c4t/IStream_Mock.h>
#endif

namespace llamalog::test {

using std::string;
using std::wstring;

#if 0
using m3c4t::IStream_Mock;
using m3c4t::IStream_Stat;
using m3c4t::MatchesRegex;

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Test;
using testing::Unused;
#endif

static LogLine GetLogLine(const char* const szPattern = "{}") {
	return LogLine(LogLevel::kDebug, __FILE__, __FUNCTION__, __LINE__, szPattern);
}
//
// bool
//

TEST(LogLineTest, bool_IsTrue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = true;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("true", str);
}

TEST(LogLineTest, bool_IsFalse_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = false;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("false", str);
}

TEST(LogLineTest, bool_IsTrueAsNumber_PrintOne) {
	LogLine logLine = GetLogLine("{:d}");
	{
		const bool arg = true;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("1", str);
}


//
// char
//

TEST(LogLineTest, char_IsCharacter_PrintCharacter) {
	LogLine logLine = GetLogLine();
	{
		const char arg = 'a';
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("a", str);
}

TEST(LogLineTest, char_IsCharacterAsHex_PrintHex) {
	LogLine logLine = GetLogLine("{:x}");
	{
		const char arg = 'M';
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("4d", str);
}

TEST(LogLineTest, char_IsEscaped_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const char arg = '\n';
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("\\n", str);
}

TEST(LogLineTest, char_IsEscapedDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:c}");
	{
		const char arg = '\n';
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("\n", str);
}


//
// unsigned char
//

TEST(LogLineTest, unsignedchar_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned char arg = 8u;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}


//
// short
//

TEST(LogLineTest, short_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = 8;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}

TEST(LogLineTest, short_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = -8;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("-8", str);
}


//
// unsigned short
//

TEST(LogLineTest, unsignedshort_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned short arg = 8u;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}


//
// int
//

TEST(LogLineTest, int_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = 8;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}

TEST(LogLineTest, int_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = -8;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("-8", str);
}


//
// unsigned int
//

TEST(LogLineTest, unsignedint_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned int arg = 8u;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}


//
// long
//

TEST(LogLineTest, long_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = 8L;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}

TEST(LogLineTest, long_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = -8L;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("-8", str);
}


//
// unsigned long
//

TEST(LogLineTest, unsignedlong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long arg = 8UL;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}


//
// long long
//

TEST(LogLineTest, longlong_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = 8LL;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}

TEST(LogLineTest, longlong_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = -8LL;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("-8", str);
}


//
// unsigned long long
//

TEST(LogLineTest, unsignedlonglong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long long arg = 8ULL;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8", str);
}


//
// float
//

TEST(LogLineTest, float_IsValue_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const float arg = 8.8f;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, float_IsFltMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const float arg = FLT_MIN;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", FLT_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLineTest, float_IsFltMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const float arg = -FLT_MAX;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", -FLT_MAX);
	EXPECT_EQ(sz, str);
}


//
// double
//

TEST(LogLineTest, double_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const double arg = 8.8;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, double_IsDblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const double arg = DBL_MIN;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", DBL_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLineTest, double_IsDblMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const double arg = -DBL_MAX;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", -DBL_MAX);
	EXPECT_EQ(sz, str);
}


//
// long double
//

TEST(LogLineTest, longdouble_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long double arg = 8.8l;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, longdouble_IsLdblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const long double arg = LDBL_MIN;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", LDBL_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLineTest, longdouble_IsLdblMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const long double arg = LDBL_MAX;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	char sz[1024];
	sprintf_s(sz, "%g", LDBL_MAX);
	EXPECT_EQ(sz, str);
}


//
// void*
//

TEST(LogLineTest, voidptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	char sz[1024];
	{
		int value = 9;
		void* arg = &value;
		sprintf_s(sz, "0x%" PRIxPTR, (uintptr_t) arg);
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ(sz, str);
}

TEST(LogLineTest, voidptr_IsNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const void* const arg = nullptr;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("0x0", str);
}


//
// nullptr
//

TEST(LogLineTest, nullptr_IsValue_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const nullptr_t arg = nullptr;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("0x0", str);
}


//
// char*
//

TEST(LogLineTest, charptr_IsLiteral_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << "Test";
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, charptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test", str);
}

// Tests that string in buffer is still accessible after grow
TEST(LogLineTest, charptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine("{} {}");
	{
		const char* const arg0 = "Test";
		const string arg1 = string(1024, 'x');
		logLine << arg0 << arg1;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test " + string(1024, 'x'), str);
}

TEST(LogLineTest, charptr_HasEscapedChar_PrintEscapeChar) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test\nNext Line";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\\nNext Line", str);
}

TEST(LogLineTest, charptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const char* const arg = "Test\nNext Line";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\nNext Line", str);
}

TEST(LogLineTest, charptr_HasHexChar_PrintEscapedValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Te\xE4st";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Te\\xE4st", str);
}

TEST(LogLineTest, charptr_HasHexCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const char* const arg = "Te\xE4st";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Te\xE4st", str);
}

TEST(LogLineTest, charptr_IsNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = nullptr;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("0x0", str);
}


//
// wchar_t*
//

TEST(LogLineTest, wcharptr_IsLiteral_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << L"Test";
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, wcharptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, wcharptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const wstring arg(257, L'x');
		logLine << arg.c_str();
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ(string(257, 'x'), str);
}

TEST(LogLineTest, wcharptr_IsLongValueAfterConversion_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		wstring arg(255, L'x');
		arg.push_back(L'\xE4');
		logLine << arg.c_str();
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ(string(255, 'x') + "\\xC3\\xA4", str);
}

TEST(LogLineTest, wcharptr_HasEscapedChar_PrintEscapedValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\nNext Line";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\\nNext Line", str);
}

TEST(LogLineTest, wcharptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\nNext Line";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\nNext Line", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEnd_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\\xC3\\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEndDoNotEscape_PrintUtf8Unescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test\xC3\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtStart_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"\xE4Test";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("\\xC3\\xA4Test", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharInMiddle_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Te\xE4st";
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Te\\xC3\\xA4st", str);
}

TEST(LogLineTest, wcharptr_IsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = nullptr;
		logLine << arg;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("0x0", str);
}


//
// two arguments
//

TEST(LogLineTest, Multiple_TwoArguments_PrintValues) {
	LogLine logLine = GetLogLine("{} {} {}");
	{
		const char* arg0 = "Test";
		const wchar_t* arg1 = L"Test";
		const int arg2 = 7;
		logLine << arg0 << arg1 << arg2;
	}
	const string str = logLine.GetMessage();

	EXPECT_EQ("Test Test 7", str);
}

#if 0

//
// return value
//

TEST(LogLineTest, ReturnValue_NoArguments_PrintEmpty) {
	const string str = M3C_ARGUMENTS_TO_STRING(true);

	EXPECT_EQ("", str);
}

TEST(ArgumentFormatterTest, ReturnValue_ReturnValueOnly_PrintReturnValue) {
	const int arg = 3;
	const string str = M3C_ARGUMENTS_TO_STRING(true, arg);

	EXPECT_EQ("{_ret:3}", str);
}

TEST(ArgumentFormatterTest, ReturnValue_ReturnValueAndArgument_PrintReturnValueAndArgument) {
	const int arg0 = 3;
	const int arg1 = 7;
	const string str = M3C_ARGUMENTS_TO_STRING(true, arg0, arg1);

	EXPECT_EQ("{_ret:3,arg1:7}", str);
}


//
// Custom formatting
//

struct TestValue {
	int a = 3;
	string b = "Test";
};

class TestFormat : public AbstractArgumentFormat {
public:
	constexpr TestFormat(const char* szName, const TestValue& x) noexcept
		: AbstractArgumentFormat(szName)
		, m_x(x) {
		// empty
	}
public:
	virtual void Append(ArgumentAppender appender) const final {
		appender("{a:%d,b:\"%s\"}", m_x.a, m_x.b.c_str());
	}
private:
	const TestValue& m_x;
};

TEST(ArgumentFormatterTest, CustomFormat_AsOnlyArgument_PrintValue) {
	TestValue x;
	const string str = M3C_ARGUMENTS_TO_STRING(false, TestFormat("x", x));

	EXPECT_EQ("{x:{a:3,b:\"Test\"}}", str);
}

TEST(ArgumentFormatterTest, CustomFormat_AsFirstArgument_PrintValue) {
	TestValue x;
	const int arg = 3;
	const string str = M3C_ARGUMENTS_TO_STRING(false, TestFormat("x", x), arg);

	EXPECT_EQ("{x:{a:3,b:\"Test\"},arg:3}", str);
}

TEST(ArgumentFormatterTest, CustomFormat_AsLastArgument_PrintValue) {
	const int arg = 3;
	TestValue x;
	const string str = M3C_ARGUMENTS_TO_STRING(false, arg, TestFormat("x", x));

	EXPECT_EQ("{arg:3,x:{a:3,b:\"Test\"}}", str);
}

TEST(ArgumentFormatterTest, CustomFormat_AsReturnValue_PrintValue) {
	TestValue x;
	const string str = M3C_ARGUMENTS_TO_STRING(true, TestFormat("x", x));

	EXPECT_EQ("{_ret:{a:3,b:\"Test\"}}", str);
}

TEST(ArgumentFormatterTest, CustomFormat_AsReturnValueWithArgument_PrintValue) {
	TestValue x;
	const int arg = 3;
	const string str = M3C_ARGUMENTS_TO_STRING(true, TestFormat("x", x), arg);
	
	EXPECT_EQ("{_ret:{a:3,b:\"Test\"},arg:3}", str);
}

//
// ArgumentCustomNameFormat
//

TEST(ArgumentFormatterTest, ArgumentCustomNameFormat_AsOnlyArgument_PrintValue) {
	const int value = 7;
	const string str = M3C_ARGUMENTS_TO_STRING(false, WithCustomName("x", value));

	EXPECT_EQ("{x:7}", str);
}

TEST(ArgumentFormatterTest, ArgumentCustomNameFormat_AsReturnValueWithArgument_PrintValue) {
	const int value = 7;
	const int arg = 3;
	const string str = M3C_ARGUMENTS_TO_STRING(true, WithCustomName(nullptr, value), arg);

	EXPECT_EQ("{_ret:7,arg:3}", str);
}


//
// Format HRESULT
//

TEST(ArgumentFormatterTest, FormatHRESULT_AsOnlyArgument_PrintValue) {
	const string str = M3C_ARGUMENTS_TO_STRING(false, FormatHRESULT("hr", S_FALSE));

	EXPECT_EQ("{hr:0x00000001}", str);
}

TEST(ArgumentFormatterTest, FormatHRESULT_AsReturnValueWithArgument_PrintValue) {
	const int arg = 3;
	const string str = M3C_ARGUMENTS_TO_STRING(true, FormatHRESULT(nullptr, E_INVALIDARG), arg);
	
	EXPECT_EQ("{_ret:0x80070057,arg:3}", str);
}


//
// Exception handling
//

class ThrowingFormat : public AbstractArgumentFormat {
public:
	constexpr ThrowingFormat(const char* szName, int& line) noexcept
		: AbstractArgumentFormat(szName)
		, m_line(line) {
		// empty
	}
public:
	virtual void Append(ArgumentAppender appender) const final {
		m_line = __LINE__ + 1;
		M3C_THROW(ComException(E_INVALIDARG, "check"));
	}
public:
	int& m_line;
};

TEST(ArgumentFormatterTest, CustomFormat_ThrowsException_PrintException) {
	int line;

	const string str = M3C_ARGUMENTS_TO_STRING(false, ThrowingFormat("x", line));
	char sz[1024];
	sprintf_s(sz, "^\\{_e:\\{msg:\\\"check: .+\\\",file:\\\".+\\\",line:%d,fn:\\\"m3c::test::ThrowingFormat::Append\\\"\\}\\}$", line);

	EXPECT_THAT(str.c_str(), MatchesRegex(sz));
}
#endif

}  // namespace llamalog::test
