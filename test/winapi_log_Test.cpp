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

#include "llamalog/winapi_log.h"

#include "llamalog/LogLine.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>


namespace llamalog::test {

namespace t = testing;

namespace {

LogLine GetLogLine(const char* const pattern = "{}") {
	return LogLine(Priority::kDebug, "file.cpp", 99, "myfunction()", pattern);
}

}  // namespace


//
// error_code
//

TEST(winapi_logTest, errorcode_LogWin32Error_PrintMessage) {
	LogLine logLine = GetLogLine();
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex(".+\\S \\(5\\)"));
}

TEST(winapi_logTest, errorcode_LogHRESULT_PrintMessage) {
	LogLine logLine = GetLogLine();
	{
		const error_code arg{E_INVALIDARG};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex(".+\\S \\(0x80070057\\)"));
}

TEST(winapi_logTest, errorcode_LastError_PrintMessage) {
	LogLine logLine = GetLogLine();
	{
		SetLastError(ERROR_ACCESS_DENIED);
		logLine << LastError();
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_THAT(str, t::MatchesRegex(".+\\S \\(5\\)"));
}


//
// LARGE_INTEGER
//

TEST(winapi_logTest, LARGEINTEGER_Log_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const LARGE_INTEGER arg = {.LowPart = 0xFFu, .HighPart = -1L};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-4294967041", str);
}


//
// ULARGE_INTEGER
//

TEST(winapi_logTest, ULARGEINTEGER_Log_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const ULARGE_INTEGER arg = {.LowPart = 0xFFu, .HighPart = 0xFFu};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("1095216660735", str);
}


//
// HINSTANCE
//

TEST(winapi_logTest, HINSTANCE_LogValue_PrintValue) {
	LogLine logLine = GetLogLine();
	char sz[1024];
	{
		const HINSTANCE arg = GetModuleHandle(nullptr);  // NOLINT(misc-misplaced-const): We DO want a const variable.
		sprintf_s(sz, "0x%" PRIxPTR, reinterpret_cast<uintptr_t>(arg));
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ(sz, str);
}

TEST(winapi_logTest, HINSTANCE_LogNullptr_PrintZero) {
	LogLine logLine = GetLogLine();
	{
		const HINSTANCE arg = nullptr;  // NOLINT(misc-misplaced-const): We DO want a const variable.
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("0x0", str);
}


//
// POINT
//

TEST(winapi_logTest, POINT_LogValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const POINT arg = {-10, 20};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(winapi_logTest, POINT_LogInline_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << POINT{-10, 20};
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(winapi_logTest, POINT_LogValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const POINT arg = {-10, 20};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-010,  020)", str);
}

TEST(winapi_logTest, POINT_LogPointerValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const POINT value = {-10, 20};
		const POINT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(winapi_logTest, POINT_LogPointerNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const POINT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(winapi_logTest, POINT_LogPointerValueWithCustomformat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const POINT value = {-10, 20};
		const POINT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(winapi_logTest, POINT_LogPointerNullptrWithCustomformat_PrintNull) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const POINT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
}

TEST(winapi_logTest, POINT_LogPointerValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const POINT value = {-10, 20};
		const POINT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-010,  020)", str);
}

TEST(winapi_logTest, POINT_LogPointerNullptrPrintPadded_PrintNull) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const POINT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(winapi_logTest, POINT_LogPointerValuePrintPaddedWithCustomformat_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4?nullptr}");
	{
		const POINT value = {-10, 20};
		const POINT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-010,  020)", str);
}

TEST(winapi_logTest, POINT_LogPointerNullptrPrintPaddedWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:0= 4?nullptr}");
	{
		const POINT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
}


//
// RECT
//

TEST(winapi_logTest, RECT_LogValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const RECT arg = {-10, 20, 30, 40};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-10, 20) - (30, 40))", str);
}

TEST(winapi_logTest, RECT_LogValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const RECT arg = {-10, 20, 30, 40};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-010,  020) - ( 030,  040))", str);
}

TEST(winapi_logTest, RECT_LogPointerValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const RECT value = {-10, 20, 30, 40};
		const RECT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-10, 20) - (30, 40))", str);
}

TEST(winapi_logTest, RECT_LogPointerNullptr_PrintNull) {
	LogLine logLine = GetLogLine();
	{
		const RECT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(winapi_logTest, RECT_LogPointerValueWithCustomformat_PrintValue) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const RECT value = {-10, 20, 30, 40};
		const RECT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-10, 20) - (30, 40))", str);
}

TEST(winapi_logTest, RECT_LogPointerNullptrWithCustomformat_PrintNull) {
	LogLine logLine = GetLogLine("{:?nullptr}");
	{
		const RECT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
}

TEST(winapi_logTest, RECT_LogPointerValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const RECT value = {-10, 20, 30, 40};
		const RECT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-010,  020) - ( 030,  040))", str);
}

TEST(winapi_logTest, RECT_LogPointerNullptrPrintPadded_PrintNull) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const RECT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(null)", str);
}

TEST(winapi_logTest, RECT_LogPointerValuePrintPaddedWithCustomformat_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4?nullptr}");
	{
		const RECT value = {-10, 20, 30, 40};
		const RECT* const arg = &value;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-010,  020) - ( 030,  040))", str);
}

TEST(winapi_logTest, RECT_LogPointerNullptrPrintPaddedWithCustomFormat_PrintNull) {
	LogLine logLine = GetLogLine("{:0= 4?nullptr}");
	{
		const RECT* const arg = nullptr;
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("nullptr", str);
}

}  // namespace llamalog::test
