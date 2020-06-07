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

#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>

namespace llamalog::test {

namespace {

LogLine GetLogLine(const char* const pattern = "{}") {
	return LogLine(Priority::kDebug, "file.cpp", 99, "myfunction()", pattern);
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

TEST(LogLineTest, bool_PointerIsTrue_PrintTrue) {
	LogLine logLine = GetLogLine();
	{
		const bool value = true;
		const bool* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true", str);
}

TEST(LogLineTest, bool_PointerIsTrueWithCustomFormat_PrintTrue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const bool value = true;
		const bool* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true", str);
}

TEST(LogLineTest, bool_PointerIsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const bool* arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(LogLineTest, bool_PointerIsNullptrWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const bool* arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
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
// signed char
//

TEST(LogLineTest, signedchar_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const signed char arg = 64u;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("64", str);
}

TEST(LogLineTest, signedchar_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const signed char value = 64u;
		const signed char* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("64", str);
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

TEST(LogLineTest, unsignedchar_PointerIsValue_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const unsigned char value = 179u;
		static_assert(value > SCHAR_MAX);
		const unsigned char* arg = &value;
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

TEST(LogLineTest, short_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const short value = 2790;
		const short* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("2790", str);
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

TEST(LogLineTest, unsignedshort_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const unsigned short value = 37900u;
		const unsigned short* arg = &value;
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

TEST(LogLineTest, int_PointerIsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int value = -27900;
		const int* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900", str);
}

TEST(LogLineTest, int_PointerIsNullptr_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int* arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(LogLineTest, int_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const int value = -27900;
		const int* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900", str);
}

TEST(LogLineTest, int_PointerIsNullptrWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const int* arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
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

TEST(LogLineTest, unsignedint_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const unsigned int value = 37900u;
		const unsigned int* arg = &value;
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

TEST(LogLineTest, long_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const long value = -379000L;
		const long* arg = &value;
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
		const unsigned long arg = 3790000000ul;
		static_assert(arg > LONG_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("3790000000", str);
}

TEST(LogLineTest, unsignedlong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const unsigned long value = 3790000000;
		const unsigned long* arg = &value;
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

TEST(LogLineTest, longlong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const long long value = -379000000000LL;
		const long long* arg = &value;
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
		const unsigned long long arg = 10790000000000000000ull;
		static_assert(arg > LLONG_MAX);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("10790000000000000000", str);
}

TEST(LogLineTest, unsignedlonglong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const unsigned long long value = 10790000000000000000ull;
		const unsigned long long* arg = &value;
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

TEST(LogLineTest, float_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?nullptr}");
	{
		const float value = 8.8f;
		const float* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
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

TEST(LogLineTest, double_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?nullptr}");
	{
		const double value = 8.8f;
		const double* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
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

TEST(LogLineTest, longdouble_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?nullptr}");
	{
		const long double value = 8.8f;
		const long double* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8", str);
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
	LogLine logLine = GetLogLine("{} {:.3}");
	{
		const char* const arg0 = "Test";
		const std::string arg1(1024, 'x');
		logLine << arg0 << arg1.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test xxx", str);
}

TEST(LogLineTest, charptr_HasEscapedChar_PrintEscapeChar) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test\nNext Line\\";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\nNext Line\\\\", str);
}

TEST(LogLineTest, charptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const char* const arg = "Test\nNext Line\\";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext Line\\", str);
}

TEST(LogLineTest, charptr_HasHexChar_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Te\xE4st";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Te\xE4st", str);
}

TEST(LogLineTest, charptr_HasHexCharDoNotEscape_PrintUtf8) {
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
	LogLine logLine = GetLogLine("{:.3}");
	{
		const std::wstring arg(257, L'x');
		logLine << arg.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("xxx", str);
}

TEST(LogLineTest, wcharptr_IsLongValueAfterConversion_PrintUtf8) {
	LogLine logLine = GetLogLine("{:.5}");
	{
		std::wstring arg(256, L'x');
		arg[0] = L'\xE4';
		logLine << arg.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\xC3\xA4xxx", str);
}

TEST(LogLineTest, wcharptr_HasEscapedChar_PrintEscapedValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\nNext Line\\";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\\nNext Line\\\\", str);
}

TEST(LogLineTest, wcharptr_HasEscapedCharDoNotEscape_PrintUnescaped) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\nNext Line\\";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext Line\\", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEnd_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\xC3\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtEndDoNotEscape_PrintUtf8) {
	LogLine logLine = GetLogLine("{:s}");
	{
		const wchar_t* const arg = L"Test\xE4";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\xC3\xA4", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharAtStart_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"\xE4Test";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\xC3\xA4Test", str);
}

TEST(LogLineTest, wcharptr_HasSpecialCharInMiddle_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Te\xE4st";
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Te\xC3\xA4st", str);
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

TEST(LogLineTest, wstring_IsValue_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		logLine << std::wstring(L"Test\xE4\n");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\xC3\xA4\\n", str);
}

TEST(LogLineTest, wstring_IsReference_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(L"Test\xE4\n");
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\xC3\xA4\\n", str);
}


//
// std::align_val_t
//

TEST(LogLineTest, stdalignvalt_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::align_val_t arg = static_cast<std::align_val_t>(4096);
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("4096", str);
}

TEST(LogLineTest, stdalignvalt_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:x?nullptr}");
	{
		const std::align_val_t value = static_cast<std::align_val_t>(4096);
		const std::align_val_t* arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("1000", str);
}

//
// Multiple arguments
//

TEST(LogLineTest, Multiple_ThreeArguments_PrintValues) {
	LogLine logLine = GetLogLine("{} {} {}");
	{
		const char* arg0 = "Test";
		const int arg1 = 7;
		const wchar_t* arg2 = L"test";
		logLine << arg0 << arg1 << arg2;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test 7 test", str);
}

TEST(LogLineTest, Multiple_ThreeArgumentsWithLong_PrintValues) {
	LogLine logLine = GetLogLine("{} {:.3} {:.3}");
	{
		const char* arg0 = "Test";
		const std::string arg1 = std::string(256, 'x');
		const std::wstring arg2 = std::wstring(256, L'y');
		logLine << arg0 << arg1 << arg2;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test xxx yyy", str);
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
		M3C_THROW(com_exception(E_INVALIDARG, "check"));
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
