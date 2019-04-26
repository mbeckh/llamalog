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

#include "llamalog/winapi_format.h"  // IWYU pragma: keep

#include "llamalog/winapi_log.h"

#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <windows.h>

#include <string>


namespace llamalog::test {

//
// error_code
//

TEST(winapi_formatTest, ErrorCode_IsSystemFormatDefault_PrintMessage) {
	std::string str;
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		str = fmt::format("{}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(5\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsSystemFormatAsDecimal_PrintDecimal) {
	std::string str;
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		str = fmt::format("{:02d}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(05\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsSystemFormatAsHex_PrintHex) {
	std::string str;
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		str = fmt::format("{:x}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(5\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsSystemOmitCode_PrintMessageOnly) {
	std::string str;
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		str = fmt::format("{:%}", arg);
	}

	EXPECT_THAT(str, testing::Not(testing::MatchesRegex(".+\\(.*\\)")));
}

TEST(winapi_formatTest, ErrorCode_IsHRESULTFormatDefault_PrintMessage) {
	std::string str;
	{
		const error_code arg{E_INVALIDARG};
		str = fmt::format("{}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(0x80070057\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsHRESULTFormatAsDecimal_PrintDecimal) {
	std::string str;
	{
		const error_code arg{E_INVALIDARG};
		str = fmt::format("{:02d}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(2147942487\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsHRESULTFormatAsHex_PrintHex) {
	std::string str;
	{
		const error_code arg{E_INVALIDARG};
		str = fmt::format("{:x}", arg);
	}

	EXPECT_THAT(str, testing::MatchesRegex(".+ \\(80070057\\)"));
}

TEST(winapi_formatTest, ErrorCode_IsHRESULTOmitCode_PrintMessageOnly) {
	std::string str;
	{
		const error_code arg{ERROR_ACCESS_DENIED};
		str = fmt::format("{:%}", arg);
	}

	EXPECT_THAT(str, testing::Not(testing::MatchesRegex(".+\\(.*\\)")));
}


//
// POINT
//

TEST(winapi_formatTest, POINT_FormatValue_PrintValue) {
	std::string str;
	{
		const POINT arg = {-10, 20};
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("(-10, 20)", str);
}

TEST(winapi_formatTest, POINT_FormatInline_PrintValue) {
	std::string str;
	{
		str = fmt::format("{}", POINT{-10, 20});
	}

	EXPECT_EQ("(-10, 20)", str);
}


//
// RECT
//

TEST(winapi_formatTest, POINT_Format_PrintValue) {
	std::string str;
	{
		const RECT arg = {-10, 20, 30, 40};
		str = fmt::format("{}", arg);
	}

	EXPECT_EQ("((-10, 20), (30, 40))", str);
}

}  // namespace llamalog::test
