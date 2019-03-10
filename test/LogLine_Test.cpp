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

#include <gtest/gtest.h>

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace llamalog::test {

namespace {

LogLine GetLogLine(const char* const szPattern = "{}") {
	return LogLine(LogLevel::kDebug, "file.cpp", 99, "myfunction()", szPattern);
}

}  // namespace

//
// bool
//

TEST(LogLineTest, bool_IsTrue_PrintTrue) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = true;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true", str);
}

TEST(LogLineTest, bool_IsFalse_PrintFalse) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = false;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("false", str);
}

TEST(LogLineTest, bool_IsTrueAsNumber_PrintOne) {
	LogLine logLine = GetLogLine("{:d}");
	{
		const bool arg = true;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("a", str);
}

TEST(LogLineTest, char_IsCharacterAsNumber_PrintNumber) {
	LogLine logLine = GetLogLine("{:d}");
	{
		const char arg = 'a';
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("97", str);
}

TEST(LogLineTest, char_IsCharacterAsHex_PrintHex) {
	LogLine logLine = GetLogLine("{:x}");
	{
		const char arg = 'M';
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("4d", str);
}

TEST(LogLineTest, char_IsEscaped_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const char arg = '\n';
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\\n", str);
}

TEST(LogLineTest, char_IsEscapedDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:c}");
	{
		const char arg = '\n';
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\n", str);
}


//
// unsigned char
//

TEST(LogLineTest, unsignedchar_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned char arg = 179u;
		static_assert(arg > SCHAR_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("179", str);
}


//
// short
//

TEST(LogLineTest, short_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = 2790;
		static_assert(arg > INT8_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("2790", str);
}

TEST(LogLineTest, short_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = -2790;
		static_assert(arg < INT8_MIN);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-2790", str);
}


//
// unsigned short
//

TEST(LogLineTest, unsignedshort_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned short arg = 37900u;
		static_assert(arg > SHRT_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900", str);
}


//
// int
//

TEST(LogLineTest, int_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = 27900;
		static_assert(arg > INT8_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("27900", str);
}

TEST(LogLineTest, int_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = -27900;
		static_assert(arg < INT8_MIN);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900", str);
}


//
// unsigned int
//

TEST(LogLineTest, unsignedint_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned int arg = 37900u;
		static_assert(arg > INT16_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900", str);
}


//
// long
//

TEST(LogLineTest, long_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = 379000L;
		static_assert(arg > INT16_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("379000", str);
}

TEST(LogLineTest, long_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = -379000L;
		static_assert(arg < INT16_MIN);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000", str);
}


//
// unsigned long
//

TEST(LogLineTest, unsignedlong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long arg = 3790000000UL;
		static_assert(arg > LONG_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("3790000000", str);
}


//
// long long
//

TEST(LogLineTest, longlong_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = 379000000000LL;
		static_assert(arg > LONG_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("379000000000", str);
}

TEST(LogLineTest, longlong_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = -379000000000LL;
		static_assert(arg < LONG_MIN);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000000000", str);
}


//
// unsigned long long
//

TEST(LogLineTest, unsignedlonglong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long long arg = 10790000000000000000ULL;
		static_assert(arg > LLONG_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("10790000000000000000", str);
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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, float_IsFltMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const float arg = FLT_MIN;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, double_IsDblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const double arg = DBL_MIN;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
}

TEST(LogLineTest, longdouble_IsLdblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g}");
	{
		const long double arg = LDBL_MIN;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g", LDBL_MAX);
	EXPECT_EQ(sz, str);
}


//
// void*
//

TEST(LogLineTest, voidptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		void* arg = reinterpret_cast<void*>(0x123);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x123", str);
}

TEST(LogLineTest, voidptr_IsNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const void* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, charptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}

// Tests that string in buffer is still accessible after grow
TEST(LogLineTest, charptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine("{} {}");
	{
		const char* const arg0 = "Test";
		const std::string arg1(1024, 'x');
		logLine << arg0 << arg1.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test " + std::string(1024, 'x'), str);
}

TEST(LogLineTest, charptr_HasEscapedChar_PrintEscapeChar) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test\nNext Line";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\nNext Line", str);
}

TEST(LogLineTest, charptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const char* const arg = "Test\nNext Line";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext Line", str);
}

TEST(LogLineTest, charptr_HasHexChar_PrintEscapedValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Te\xE4st";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Te\\xE4st", str);
}

TEST(LogLineTest, charptr_HasHexCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const char* const arg = "Te\xE4st";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Te\xE4st", str);
}

TEST(LogLineTest, charptr_IsNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

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
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, wcharptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, wcharptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(257, L'x');
		logLine << arg.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(std::string(257, 'x'), str);
}

TEST(LogLineTest, wcharptr_IsLongValueAfterConversion_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		std::wstring arg(255, L'x');
		arg.push_back(L'\xE4');
		logLine << arg.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(std::string(255, 'x') + "\\xC3\\xA4", str);
}

TEST(LogLineTest, wcharptr_HasEscapedChar_PrintEscapedValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\nNext Line";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\nNext Line", str);
}

TEST(LogLineTest, wcharptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\nNext Line";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext Line", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEnd_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\xC3\\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEndDoNotEscape_PrintUtf8Unescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\xC3\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtStart_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"\xE4Test";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\\xC3\\xA4Test", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharInMiddle_PrintUtf8Value) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Te\xE4st";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Te\\xC3\\xA4st", str);
}

TEST(LogLineTest, wcharptr_IsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}


//
// string
//

TEST(LogLineTest, string_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << std::string("Test");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}

TEST(LogLineTest, string_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::string arg("Test");
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test", str);
}


//
// wstring
//

TEST(LogLineTest, wstring_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << std::wstring(L"Test\xE4");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\xC3\\xA4", str);
}

TEST(LogLineTest, wstring_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(L"Test\xE4");
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\xC3\\xA4", str);
}


//
// two arguments
//

TEST(LogLineTest, Multiple_ThreeArguments_PrintValues) {
	LogLine logLine = GetLogLine("{} {} {}");
	{
		const char* arg0 = "Test";
		const wchar_t* arg1 = L"Test";
		const int arg2 = 7;
		logLine << arg0 << arg1 << arg2;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test 7", str);
}

#if 0
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
