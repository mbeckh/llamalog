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

#include "llamalog/LogLine.h"

#include "llamalog/custom_types.h"
#include "llamalog/exception.h"

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

LogLine GetLogLine(const char* const pattern = "{} {}") {
	return LogLine(Priority::kDebug, "file.cpp", 99, "myfunction()", pattern);
}

class CustomTypeTrivial {
public:
	CustomTypeTrivial(int value) noexcept
		: m_value(value) {
		// empty
	}
	CustomTypeTrivial(const CustomTypeTrivial& oth) noexcept = default;
	CustomTypeTrivial(CustomTypeTrivial&& oth) noexcept = default;

	int GetValue() const noexcept {
		return m_value;
	}

private:
	int m_value;
};

class CustomTypeCopyOnly {
public:
	CustomTypeCopyOnly() noexcept
		: m_copies(0) {
		// empty
	}
	CustomTypeCopyOnly(const CustomTypeCopyOnly& oth) noexcept
		: m_copies(oth.m_copies + 1) {
		// empty
	}
	CustomTypeCopyOnly(CustomTypeCopyOnly&& oth) = delete;

	int GetCopies() const noexcept {
		return m_copies;
	}

private:
	const int m_copies;
};

class CustomTypeMove {
public:
	CustomTypeMove() noexcept
		: m_copies(0)
		, m_moves(0) {
		// empty
	}
	CustomTypeMove(const CustomTypeMove& oth) noexcept
		: m_copies(oth.m_copies + 1)
		, m_moves(oth.m_moves) {
		// empty
	}
	CustomTypeMove(CustomTypeMove&& oth) noexcept
		: m_copies(oth.m_copies)
		, m_moves(oth.m_moves + 1) {
		// empty
	}

	int GetCopies() const noexcept {
		return m_copies;
	}
	int GetMoves() const noexcept {
		return m_moves;
	}

private:
	const int m_copies;
	const int m_moves;
};

}  // namespace
}  // namespace llamalog::test

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeTrivial& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeCopyOnly& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CustomTypeMove& arg) {
	return logLine.AddCustomArgument(arg);
}

template <>
struct fmt::formatter<llamalog::test::CustomTypeTrivial> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.end();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeTrivial& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "({})", arg.GetValue());
	}
};

template <>
struct fmt::formatter<llamalog::test::CustomTypeCopyOnly> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.begin();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeCopyOnly& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "(copy #{})", arg.GetCopies());
	}
};

template <>
struct fmt::formatter<llamalog::test::CustomTypeMove> {
public:
	fmt::format_parse_context::iterator parse(const fmt::format_parse_context& ctx) {
		auto it = ctx.begin();
		const auto last = ctx.end();
		while (it != last && *it != '}') {
			++it;
		}
		return it;
	}

	fmt::format_context::iterator format(const llamalog::test::CustomTypeMove& arg, fmt::format_context& ctx) {
		return fmt::format_to(ctx.out(), "(copy #{} move #{})", arg.GetCopies(), arg.GetMoves());
	}
};

namespace llamalog::test {

//
// bool
//

TEST(LogLine_Test, bool_IsTrue_PrintTrue) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = true;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true true", str);
}

TEST(LogLine_Test, bool_IsFalse_PrintFalse) {
	LogLine logLine = GetLogLine();
	{
		const bool arg = false;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("false false", str);
}

TEST(LogLine_Test, bool_IsTrueAsNumber_PrintOne) {
	LogLine logLine = GetLogLine("{:d} {:d}");
	{
		const bool arg = true;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("1 1", str);
}

TEST(LogLine_Test, bool_PointerIsTrue_PrintTrue) {
	LogLine logLine = GetLogLine();
	{
		const bool value = true;
		const bool* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true true", str);
}

TEST(LogLine_Test, bool_PointerIsTrueWithCustomFormat_PrintTrue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const bool value = true;
		const bool* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("true true", str);
}

TEST(LogLine_Test, bool_PointerIsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const bool* arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null) (null)", str);
}

TEST(LogLine_Test, bool_PointerIsNullptrWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const bool* arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("null\tptr null\\tptr", str);
}


//
// char
//

TEST(LogLine_Test, char_IsCharacter_PrintCharacter) {
	LogLine logLine = GetLogLine();
	{
		const char arg = 'a';
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("a a", str);
}

TEST(LogLine_Test, char_IsCharacterAsNumber_PrintNumber) {
	LogLine logLine = GetLogLine("{:d} {:d}");
	{
		const char arg = 'a';
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("97 97", str);
}

TEST(LogLine_Test, char_IsCharacterAsHex_PrintHex) {
	LogLine logLine = GetLogLine("{:x} {:x}");
	{
		const char arg = 'M';
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("4d 4d", str);
}

TEST(LogLine_Test, char_IsTab_PrintTab) {
	LogLine logLine = GetLogLine();
	{
		const char arg = '\n';
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\n \\n", str);
}


//
// signed char
//

TEST(LogLine_Test, signedchar_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const signed char arg = 64u;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("64 64", str);
}

TEST(LogLine_Test, signedchar_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const signed char value = 64u;
		const signed char* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("64 64", str);
}


//
// unsigned char
//

TEST(LogLine_Test, unsignedchar_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned char arg = 179u;
		static_assert(arg > SCHAR_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("179 179", str);
}

TEST(LogLine_Test, unsignedchar_PointerIsValue_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const unsigned char value = 179u;
		static_assert(value > SCHAR_MAX);
		const unsigned char* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("179 179", str);
}


//
// short
//

TEST(LogLine_Test, short_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = 2790;
		static_assert(arg > INT8_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("2790 2790", str);
}

TEST(LogLine_Test, short_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const short arg = -2790;
		static_assert(arg < INT8_MIN);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-2790 -2790", str);
}

TEST(LogLine_Test, short_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const short value = 2790;
		const short* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("2790 2790", str);
}


//
// unsigned short
//

TEST(LogLine_Test, unsignedshort_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned short arg = 37900u;
		static_assert(arg > SHRT_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900 37900", str);
}

TEST(LogLine_Test, unsignedshort_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const unsigned short value = 37900u;
		const unsigned short* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900 37900", str);
}


//
// int
//

TEST(LogLine_Test, int_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = 27900;
		static_assert(arg > INT8_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("27900 27900", str);
}

TEST(LogLine_Test, int_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int arg = -27900;
		static_assert(arg < INT8_MIN);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900 -27900", str);
}

TEST(LogLine_Test, int_PointerIsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int value = -27900;
		const int* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900 -27900", str);
}

TEST(LogLine_Test, int_PointerIsNullptr_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const int* arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null) (null)", str);
}

TEST(LogLine_Test, int_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const int value = -27900;
		const int* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-27900 -27900", str);
}

TEST(LogLine_Test, int_PointerIsNullptrWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const int* arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("null\tptr null\\tptr", str);
}


//
// unsigned int
//

TEST(LogLine_Test, unsignedint_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned int arg = 37900u;
		static_assert(arg > INT16_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900 37900", str);
}

TEST(LogLine_Test, unsignedint_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const unsigned int value = 37900u;
		const unsigned int* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("37900 37900", str);
}


//
// long
//

TEST(LogLine_Test, long_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = 379000L;
		static_assert(arg > INT16_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("379000 379000", str);
}

TEST(LogLine_Test, long_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long arg = -379000L;
		static_assert(arg < INT16_MIN);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000 -379000", str);
}

TEST(LogLine_Test, long_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const long value = -379000L;
		const long* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000 -379000", str);
}


//
// unsigned long
//

TEST(LogLine_Test, unsignedlong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long arg = 3790000000ul;
		static_assert(arg > LONG_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("3790000000 3790000000", str);
}

TEST(LogLine_Test, unsignedlong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const unsigned long value = 3790000000;
		const unsigned long* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("3790000000 3790000000", str);
}


//
// long long
//

TEST(LogLine_Test, longlong_IsPositive_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = 379000000000LL;
		static_assert(arg > LONG_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("379000000000 379000000000", str);
}

TEST(LogLine_Test, longlong_IsNegative_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long long arg = -379000000000LL;
		static_assert(arg < LONG_MIN);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000000000 -379000000000", str);
}

TEST(LogLine_Test, longlong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const long long value = -379000000000LL;
		const long long* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-379000000000 -379000000000", str);
}


//
// unsigned long long
//

TEST(LogLine_Test, unsignedlonglong_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const unsigned long long arg = 10790000000000000000ull;
		static_assert(arg > LLONG_MAX);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("10790000000000000000 10790000000000000000", str);
}

TEST(LogLine_Test, unsignedlonglong_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const unsigned long long value = 10790000000000000000ull;
		const unsigned long long* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("10790000000000000000 10790000000000000000", str);
}


//
// float
//

TEST(LogLine_Test, float_IsValue_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const float arg = 8.8f;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}

TEST(LogLine_Test, float_IsFltMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const float arg = FLT_MIN;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", FLT_MIN, FLT_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, float_IsFltMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const float arg = -FLT_MAX;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", -FLT_MAX, -FLT_MAX);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, float_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?null\tptr} {:g?null\tptr}");
	{
		const float value = 8.8f;
		const float* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}


//
// double
//

TEST(LogLine_Test, double_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const double arg = 8.8;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}

TEST(LogLine_Test, double_IsDblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const double arg = DBL_MIN;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", DBL_MIN, DBL_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, double_IsDblMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const double arg = -DBL_MAX;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", -DBL_MAX, -DBL_MAX);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, double_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?null\tptr} {:g?null\tptr}");
	{
		const double value = 8.8f;
		const double* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}


//
// long double
//

TEST(LogLine_Test, longdouble_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const long double arg = 8.8L;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}

TEST(LogLine_Test, longdouble_IsLdblMin_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const long double arg = LDBL_MIN;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", LDBL_MIN, LDBL_MIN);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, longdouble_IsLdblMax_PrintValue) {
	LogLine logLine = GetLogLine("{:g} {:g}");
	{
		const long double arg = LDBL_MAX;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	char sz[1024];
	sprintf_s(sz, "%g %g", LDBL_MAX, LDBL_MAX);
	EXPECT_EQ(sz, str);
}

TEST(LogLine_Test, longdouble_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:g?null\tptr} {:g?null\tptr}");
	{
		const long double value = 8.8f;
		const long double* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("8.8 8.8", str);
}


//
// void*
//

TEST(LogLine_Test, voidptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		void* arg = reinterpret_cast<void*>(0x123);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x123 0x123", str);
}

TEST(LogLine_Test, voidptr_IsNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const void* const arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0 0x0", str);
}


//
// nullptr
//

TEST(LogLine_Test, nullptr_IsValue_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const nullptr_t arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0 0x0", str);
}


//
// char*
//

TEST(LogLine_Test, charptr_IsLiteral_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << "Test" << escape("Test");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, charptr_IsLiteralEmpty_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << "" << escape("");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(" ", str);
}

TEST(LogLine_Test, charptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "Test";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, charptr_IsValueEmpty_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(" ", str);
}

TEST(LogLine_Test, charptr_IsUtf8_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = reinterpret_cast<const char*>(u8"\u00FC");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	ASSERT_EQ(5, str.length());
	EXPECT_EQ(static_cast<char>(0xC3), str[0]);
	EXPECT_EQ(static_cast<char>(0xBC), str[1]);
	EXPECT_EQ(0x20, str[2]);
	EXPECT_EQ(static_cast<char>(0xC3), str[3]);
	EXPECT_EQ(static_cast<char>(0xBC), str[4]);
}

// Tests that string in buffer is still accessible after grow
TEST(LogLine_Test, charptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine("{} {} {:.3}");
	{
		const char* const arg0 = "Test\nNext\\Line";
		const std::string arg1(1024, 'x');
		logLine << arg0 << escape(arg0) << arg1.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext\\Line Test\\nNext\\\\Line xxx", str);
}

TEST(LogLine_Test, charptr_Escape_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = "\\\n\r\t\b\f\v\a\u0002\u0019";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\\\n\r\t\b\f\v\a\u0002\u0019 \\\\\\n\\r\\t\\b\\f\\v\\a\\x02\\x19", str);
}

TEST(LogLine_Test, charptr_IsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const char* const arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null) (null)", str);
}

TEST(LogLine_Test, charptr_IsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const char* const arg = "Test";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, charptr_IsNullptrWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const char* const arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("null\tptr null\\tptr", str);
}


//
// wchar_t*
//

TEST(LogLine_Test, wcharptr_IsLiteral_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << L"Test" << escape(L"Test");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, wcharptr_IsLiteralEmpty_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << L"" << escape(L"");
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(" ", str);
}

TEST(LogLine_Test, wcharptr_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"Test";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, wcharptr_IsValueEmpty_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(" ", str);
}

TEST(LogLine_Test, wcharptr_IsUtf8_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"\u00FC";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	ASSERT_EQ(5, str.length());
	EXPECT_EQ(static_cast<char>(0xC3), str[0]);
	EXPECT_EQ(static_cast<char>(0xBC), str[1]);
	EXPECT_EQ(0x20, str[2]);
	EXPECT_EQ(static_cast<char>(0xC3), str[3]);
	EXPECT_EQ(static_cast<char>(0xBC), str[4]);
}

TEST(LogLine_Test, wcharptr_IsLongValue_PrintValue) {
	LogLine logLine = GetLogLine("{} {} {:.3}");
	{
		const wchar_t* const arg0 = L"Test\nNext\\Line";
		const std::wstring arg1(1025, 'x');
		logLine << arg0 << escape(arg0) << arg1.c_str();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\nNext\\Line Test\\nNext\\\\Line xxx", str);
}

TEST(LogLine_Test, wcharptr_IsLongValueAfterConversion_PrintUtf8) {
	LogLine logLine = GetLogLine("{:.5}");
	{
		std::wstring arg(256, L'x');
		arg[0] = L'\xE4';
		logLine << arg.c_str() << escape(arg.c_str());
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\xC3\xA4xxx", str);
}

TEST(LogLine_Test, wcharptr_Escape_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = L"\\\n\r\t\b\f\v\a\u0002\u0019";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("\\\n\r\t\b\f\v\a\u0002\u0019 \\\\\\n\\r\\t\\b\\f\\v\\a\\x02\\x19", str);
}

TEST(LogLine_Test, wcharptr_IsNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const wchar_t* const arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null) (null)", str);
}

TEST(LogLine_Test, wcharptr_IsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const wchar_t* const arg = L"Test";
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, wcharptr_IsNullptrWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:?null\tptr} {:?null\tptr}");
	{
		const wchar_t* const arg = nullptr;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("null\tptr null\\tptr", str);
}

//
// string
//

TEST(LogLine_Test, string_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << std::string("Test") << escape(std::string("Test"));
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, string_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::string arg("Test");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, string_IsEscaped_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const std::string arg("Test\r\nNext\tLine");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\r\nNext\tLine Test\\r\\nNext\\tLine", str);
}

TEST(LogLine_Test, string_IsUtf8_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const std::string arg("\u00C3\u00BC");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	ASSERT_EQ(5, str.length());
	EXPECT_EQ(static_cast<char>(0xC3), str[0]);
	EXPECT_EQ(static_cast<char>(0xBC), str[1]);
	EXPECT_EQ(0x20, str[2]);
	EXPECT_EQ(static_cast<char>(0xC3), str[3]);
	EXPECT_EQ(static_cast<char>(0xBC), str[4]);
}

//
// wstring
//

TEST(LogLine_Test, wstring_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << std::wstring(L"Test") << escape(std::wstring(L"Test"));
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, wstring_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(L"Test");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

TEST(LogLine_Test, wstring_IsEscaped_PrintEscaped) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(L"Test\r\nNext\tLine");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test\r\nNext\tLine Test\\r\\nNext\\tLine", str);
}

TEST(LogLine_Test, wstring_IsUtf8_PrintUtf8) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring arg(L"\u00FC");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	ASSERT_EQ(5, str.length());
	EXPECT_EQ(static_cast<char>(0xC3), str[0]);
	EXPECT_EQ(static_cast<char>(0xBC), str[1]);
	EXPECT_EQ(0x20, str[2]);
	EXPECT_EQ(static_cast<char>(0xC3), str[3]);
	EXPECT_EQ(static_cast<char>(0xBC), str[4]);
}

//
// string_view
//

TEST(LogLine_Test, stringview_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::string_view arg("Test");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

//
// wstring_view
//

TEST(LogLine_Test, wstringview_IsReference_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::wstring_view arg(L"Test");
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("Test Test", str);
}

//
// std::align_val_t
//

TEST(LogLine_Test, stdalignvalt_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const std::align_val_t arg = static_cast<std::align_val_t>(4096);
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("4096 4096", str);
}

TEST(LogLine_Test, stdalignvalt_PointerIsValueWithCustomFormat_PrintValue) {
	LogLine logLine = GetLogLine("{:x?null\tptr} {:x?null\tptr}");
	{
		const std::align_val_t value = static_cast<std::align_val_t>(4096);
		const std::align_val_t* arg = &value;
		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("1000 1000", str);
}


//
// Multiple arguments
//

TEST(LogLine_Test, Multiple_ThreeArguments_PrintValues) {
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

TEST(LogLine_Test, Multiple_ThreeArgumentsWithLong_PrintValues) {
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


//
// Copy and Move
//

TEST(LogLine_Test, CopyMove_StackBuffer_IsSame) {
	LogLine logLine = GetLogLine();
	{
		const char* arg = "Test";

		logLine << arg << escape(arg);
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ("Test Test", str);

	// first create a copy
	LogLine copy(logLine);
	// then move that copy to a new instance
	LogLine move(std::move(copy));

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);

	// now check that the result is still the same
	EXPECT_EQ(str, moveAssign.GetLogMessage());
}

TEST(LogLine_Test, CopyMove_StackBufferWithNonTriviallyCopyable_IsSame) {
	LogLine logLine = GetLogLine("{} {} {} {}");
	{
		const char* arg0 = "Test";
		const CustomTypeTrivial customTrivial(7);
		const CustomTypeCopyOnly customCopy;
		const CustomTypeMove customMove;

		logLine << customTrivial << customCopy << customMove << arg0;
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ("(7) (copy #1) (copy #1 move #0) Test", str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +1

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +1

	// optimum is copy +2 and move +2

	// now check that the result is still the same
	EXPECT_EQ("(7) (copy #5) (copy #3 move #2) Test", moveAssign.GetLogMessage());
}

TEST(LogLine_Test, CopyMove_HeapBuffer_IsSame) {
	LogLine logLine = GetLogLine(
		"{} {} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {} {:.3} {:.3} "
		"{} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {}");
	{
		const bool arg00 = true;
		const bool* ptr00 = &arg00;

		const char arg01 = 'c';

		const signed char arg02 = -2;
		const signed char* ptr02 = &arg02;

		const unsigned char arg03 = 3;
		const unsigned char* ptr03 = nullptr;

		const signed short arg04 = -4;
		const signed short* ptr04 = &arg04;

		const unsigned short arg05 = 5;
		const unsigned short* ptr05 = &arg05;

		const signed int arg06 = -6;
		const signed int* ptr06 = &arg06;

		const unsigned int arg07 = 7;
		const unsigned int* ptr07 = &arg07;

		const signed long arg08 = -8;
		const signed long* ptr08 = &arg08;

		const unsigned long arg09 = 9;
		const unsigned long* ptr09 = &arg09;

		const signed long long arg10 = -10;
		const signed long long* ptr10 = &arg10;

		const unsigned long long arg11 = 11;
		const unsigned long long* ptr11 = &arg11;

		const float arg12 = 12.12f;
		const float* ptr12 = &arg12;

		const double arg13 = 13.13;
		const double* ptr13 = &arg13;

		const long double arg14 = 14.14L;
		const long double* ptr14 = &arg14;

		const void* ptr15 = reinterpret_cast<const void*>(0x150015);

		const nullptr_t ptr16 = nullptr;

		const char* arg17 = "Test17";
		const wchar_t* arg18 = L"Test18";

		const std::string arg19(512, 'x');

		const std::wstring arg20(256, 'y');

		logLine << arg00 << arg01 << arg02 << arg03 << arg04
				<< arg05 << arg06 << arg07 << arg08 << arg09
				<< arg10 << arg11 << arg12 << arg13 << arg14
				<< arg17 << arg18 << arg19 << arg20
				<< ptr00 << ptr02 << ptr03 << ptr04
				<< ptr05 << ptr06 << ptr07 << ptr08 << ptr09
				<< ptr10 << ptr11 << ptr12 << ptr13 << ptr14
				<< ptr15 << ptr16;
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ(
		"true c -2 3 -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 Test17 Test18 xxx yyy "
		"true -2 (null) -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 0x150015 0x0",
		str);

	// first create a copy
	LogLine copy(logLine);
	// then move that copy to a new instance
	LogLine move(std::move(copy));

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);

	// now check that the result is still the same
	EXPECT_EQ(str, moveAssign.GetLogMessage());
}

TEST(LogLine_Test, CopyMove_HeapBufferWithNonTriviallyCopyable_IsSame) {
	LogLine logLine = GetLogLine(
		"{} {} {} "
		"{} {} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {} {:.3} {:.3} "
		"{} {} {} {} "
		"{} {} {} {} {} "
		"{} {} {:g} {:g} {:g} "
		"{} {}");
	{
		const CustomTypeTrivial customTrivial(7);
		const CustomTypeCopyOnly customCopy;
		const CustomTypeMove customMove;

		const bool arg00 = true;
		const bool* ptr00 = &arg00;

		const char arg01 = 'c';

		const signed char arg02 = -2;
		const signed char* ptr02 = &arg02;

		const unsigned char arg03 = 3;
		const unsigned char* ptr03 = nullptr;

		const signed short arg04 = -4;
		const signed short* ptr04 = &arg04;

		const unsigned short arg05 = 5;
		const unsigned short* ptr05 = &arg05;

		const signed int arg06 = -6;
		const signed int* ptr06 = &arg06;

		const unsigned int arg07 = 7;
		const unsigned int* ptr07 = &arg07;

		const signed long arg08 = -8;
		const signed long* ptr08 = &arg08;

		const unsigned long arg09 = 9;
		const unsigned long* ptr09 = &arg09;

		const signed long long arg10 = -10;
		const signed long long* ptr10 = &arg10;

		const unsigned long long arg11 = 11;
		const unsigned long long* ptr11 = &arg11;

		const float arg12 = 12.12f;
		const float* ptr12 = &arg12;

		const double arg13 = 13.13;
		const double* ptr13 = &arg13;

		const long double arg14 = 14.14L;
		const long double* ptr14 = &arg14;

		const void* ptr15 = reinterpret_cast<const void*>(0x150015);

		const nullptr_t ptr16 = nullptr;

		const char* arg17 = "Test17";
		const wchar_t* arg18 = L"Test18";

		const std::string arg19(512, 'x');

		const std::wstring arg20(256, 'y');

		logLine << customTrivial << customCopy << customMove
				<< arg00 << arg01 << arg02 << arg03 << arg04
				<< arg05 << arg06 << arg07 << arg08 << arg09
				<< arg10 << arg11 << arg12 << arg13 << arg14
				<< arg17 << arg18 << arg19 << arg20
				<< ptr00 << ptr02 << ptr03 << ptr04
				<< ptr05 << ptr06 << ptr07 << ptr08 << ptr09
				<< ptr10 << ptr11 << ptr12 << ptr13 << ptr14
				<< ptr15 << ptr16;
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ(
		"(7) (copy #3) (copy #1 move #2) true c -2 3 -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 Test17 Test18 xxx yyy "
		"true -2 (null) -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 0x150015 0x0",
		str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +0 because of heap buffer

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +0 because of heap buffer

	// optimum is copy +2 and move +0

	// now check that the result is still the same
	EXPECT_EQ(
		"(7) (copy #5) (copy #3 move #2) true c -2 3 -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 Test17 Test18 xxx yyy "
		"true -2 (null) -4 5 -6 7 -8 9 -10 11 12.12 13.13 14.14 0x150015 0x0",
		moveAssign.GetLogMessage());
}


TEST(LogLine_Test, CopyMove_Exceptions_IsSame) {
	LogLine logLine = GetLogLine("{:%l} {:%l} {:%l} {:%l} {:%w} {:%c}");
	{
		try {
			llamalog::Throw(std::exception("e1"), "myfile.cpp", 15, "exfunc", "m1={:.3}", std::string(2, 'x'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::system_error(7, std::system_category(), "e2"), "myfile.cpp", 15, "exfunc", "m2={:.3}", std::string(2, 'y'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::exception("e3"), "myfile.cpp", 15, "exfunc", "m3={:.3}", std::string(512, 'x'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			llamalog::Throw(std::system_error(7, std::system_category(), "e4"), "myfile.cpp", 15, "exfunc", "m4={:.3}", std::string(512, 'y'));
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			throw std::exception("e5");
		} catch (std::exception& e) {
			logLine << e;
		}
		try {
			throw std::system_error(7, std::system_category(), "e6");
		} catch (std::exception& e) {
			logLine << e;
		}
	}
	const std::string str = logLine.GetLogMessage();
	EXPECT_EQ("m1=xx m2=yy m3=xxx m4=yyy e5 7", str);

	// first create a copy
	LogLine copy(logLine);  // copy +1
	// then move that copy to a new instance
	LogLine move(std::move(copy));  // move +0 because of heap buffer

	// assign the moved-to instance to the next
	LogLine assign(Priority::kError, "", 0, "", "");
	assign = move;  // copy +1

	// and finally move assign the value
	LogLine moveAssign(Priority::kError, "", 0, "", "");
	moveAssign = std::move(assign);  // move +0 because of heap buffer

	// optimum is copy +2 and move +0

	// now check that the result is still the same
	EXPECT_EQ(str, moveAssign.GetLogMessage());
}

}  // namespace llamalog::test
