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

#include "llamalog/WindowsTypes.h"

#include "llamalog/LogLine.h"

#include <gmock/gmock.h>

#include <windows.h>

#include <cinttypes>
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
// LARGE_INTEGER
//

TEST(WindowsTypesTest, LARGEINTEGER_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const LARGE_INTEGER arg = {{0xFFu, -1L}};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("-4294967041", str);
}


//
// ULARGE_INTEGER
//

TEST(WindowsTypesTest, ULARGEINTEGER_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const ULARGE_INTEGER arg = {{0xFFu, 0xFFu}};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("1095216660735", str);
}


//
// HINSTANCE
//

TEST(WindowsTypesTest, HINSTANCE_IsValue_PrintValue) {
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

TEST(WindowsTypesTest, HINSTANCE_IsNullptr_PrintZero) {
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

TEST(WindowsTypesTest, POINT_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const POINT arg = {-10, 20};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(WindowsTypesTest, POINT_IsInline_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		logLine << POINT{-10, 20};
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-10, 20)", str);
}

TEST(WindowsTypesTest, POINT_IsValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const POINT arg = {-10, 20};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("(-010,  020)", str);
}


//
// RECT
//

TEST(WindowsTypesTest, RECT_IsValue_PrintValue) {
	LogLine logLine = GetLogLine();
	{
		const RECT arg = {-10, 20, 30, 40};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-10, 20), (30, 40))", str);
}

TEST(WindowsTypesTest, RECT_IsValuePrintPadded_PrintPadded) {
	LogLine logLine = GetLogLine("{:0= 4}");
	{
		const RECT arg = {-10, 20, 30, 40};
		logLine << arg;
	}
	const std::string str = logLine.GetLogMessage();

	EXPECT_EQ("((-010,  020), ( 030,  040))", str);
}

}  // namespace llamalog::test
