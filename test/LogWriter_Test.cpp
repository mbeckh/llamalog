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

#include "llamalog/LogWriter.h"

#include "llamalog/LogLine.h"
#include "llamalog/Logger.h"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <detours_gmock.h>
#include <windows.h>

#include <cstdio>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace llamalog::test {

namespace t = testing;

namespace {

#pragma warning(suppress : 4100)
MATCHER_P(MatchesRegex, pattern, "") {
	return std::regex_match(arg, std::basic_regex(pattern));
}

#define WIN32_FUNCTIONS(fn_)                                                                                                                                                                       \
	fn_(2, BOOL, WINAPI, FileTimeToSystemTime,                                                                                                                                                     \
		(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime),                                                                                                                                   \
		(lpFileTime, lpSystemTime),                                                                                                                                                                \
		nullptr);                                                                                                                                                                                  \
	fn_(7, HANDLE, WINAPI, CreateFileW,                                                                                                                                                            \
		(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile), \
		(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile),                                                              \
		nullptr);                                                                                                                                                                                  \
	fn_(5, BOOL, WINAPI, WriteFile,                                                                                                                                                                \
		(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped),                                                                  \
		(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped),                                                                                                            \
		nullptr);                                                                                                                                                                                  \
	fn_(1, BOOL, WINAPI, CloseHandle,                                                                                                                                                              \
		(HANDLE hObject),                                                                                                                                                                          \
		(hObject),                                                                                                                                                                                 \
		nullptr);                                                                                                                                                                                  \
	fn_(1, BOOL, WINAPI, DeleteFileW,                                                                                                                                                              \
		(LPCWSTR lpFileName),                                                                                                                                                                      \
		(lpFileName),                                                                                                                                                                              \
		t::Return(TRUE));                                                                                                                                                                          \
	fn_(6, HANDLE, WINAPI, FindFirstFileExW,                                                                                                                                                       \
		(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags),                                 \
		(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags),                                                                                                  \
		detours_gmock::SetLastErrorAndReturn(ERROR_FILE_NOT_FOUND, INVALID_HANDLE_VALUE));                                                                                                         \
	fn_(2, BOOL, WINAPI, FindNextFileW,                                                                                                                                                            \
		(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData),                                                                                                                                     \
		(hFindFile, lpFindFileData),                                                                                                                                                               \
		detours_gmock::SetLastErrorAndReturn(ERROR_NO_MORE_FILES, FALSE));                                                                                                                         \
	fn_(1, BOOL, WINAPI, FindClose,                                                                                                                                                                \
		(HANDLE hFindFile),                                                                                                                                                                        \
		(hFindFile),                                                                                                                                                                               \
		t::Return(TRUE));                                                                                                                                                                          \
	fn_(2, int, __cdecl, fputs,                                                                                                                                                                    \
		(const char* str, FILE* stream),                                                                                                                                                           \
		(str, stream),                                                                                                                                                                             \
		t::Return(1));                                                                                                                                                                             \
	fn_(1, void, WINAPI, OutputDebugStringA,                                                                                                                                                       \
		(LPCSTR lpOutputString),                                                                                                                                                                   \
		(lpOutputString),                                                                                                                                                                          \
		nullptr)

DTGM_DECLARE_API_MOCK(Win32, WIN32_FUNCTIONS);

class LogWriter_Test : public t::Test {
public:
	void SetUp() override {
		ON_CALL(m_mock, CreateFileW(t::HasSubstr(L"ll_test."), DTGM_ARG6))
			.WillByDefault(detours_gmock::SetLastErrorAndReturn(ERROR_FILE_NOT_FOUND, INVALID_HANDLE_VALUE));
		EXPECT_CALL(m_mock, CreateFileW(t::HasSubstr(L"ll_test."), DTGM_ARG6))
			.Times(0);
		ON_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
			.WillByDefault(t::Invoke([](t::Unused, t::Unused, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, t::Unused) {
				*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
				return TRUE;
			}));
		ON_CALL(m_mock, CloseHandle(m_hFile))
			.WillByDefault(t::Return(TRUE));
	}

	void TearDown() override {
		t::Mock::VerifyAndClearExpectations(&m_mock);
		DTGM_DETACH_API_MOCK(Win32);
	}

protected:
	std::ostringstream m_out;
	int m_lines = 0;

protected:
	DTGM_DEFINE_API_MOCK(Win32, m_mock);
	HANDLE m_hFile = &m_mock;
	HANDLE m_hFind = &m_hFile;
};


class StringWriter : public LogWriter {
public:
	StringWriter(const Priority logLevel, std::ostringstream& out, int& lines)
		: LogWriter(logLevel)
		, m_out(out)
		, m_lines(lines) {
		// empty
	}

protected:
	void Log(const LogLine& logLine) final {
		fmt::basic_memory_buffer<char, 256> buffer;
		fmt::format_to(buffer, "{} {} [{}] {}:{} {} {}\n",
					   FormatTimestamp(logLine.GetTimestamp()),
					   FormatPriority(logLine.GetPriority()),
					   logLine.GetThreadId(),
					   logLine.GetFile(),
					   logLine.GetLine(),
					   logLine.GetFunction(),
					   logLine.GetLogMessage());
		m_out << std::string_view(buffer.data(), buffer.size());
		++m_lines;
	}

private:
	std::ostringstream& m_out;
	int& m_lines;
};

}  // namespace

//
// File Creation
//

TEST_F(LogWriter_Test, Log_HourlyNoOldFiles_CreateFileDeleteNone) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]_[0-2][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????_??.log"), DTGM_ARG5));
	EXPECT_CALL(m_mock, FindNextFileW(DTGM_ARG2))
		.Times(0);
	EXPECT_CALL(m_mock, FindClose(DTGM_ARG1))
		.Times(0);

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kHourly, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} DEBUG [^\\n]+ Test\\n"));
}

TEST_F(LogWriter_Test, Log_EverySecondNoOldFiles_CreateFileDeleteNone) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]_[0-2][0-9][0-5][0-9][0-5][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????_??????.log"), DTGM_ARG5));
	EXPECT_CALL(m_mock, FindNextFileW(DTGM_ARG2))
		.Times(0);
	EXPECT_CALL(m_mock, FindClose(DTGM_ARG1))
		.Times(0);

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kEverySecond, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} DEBUG [^\\n]+ Test\\n"));
}

TEST_F(LogWriter_Test, Log_MonthlyNoOldFiles_CreateFileDeleteNone) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.??????.log"), DTGM_ARG5));
	EXPECT_CALL(m_mock, FindNextFileW(DTGM_ARG2))
		.Times(0);
	EXPECT_CALL(m_mock, FindClose(DTGM_ARG1))
		.Times(0);

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kMonthly, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} DEBUG [^\\n]+ Test\\n"));
}

TEST_F(LogWriter_Test, Log_EveryMinuteThreeOldFiles_CreateFileDeleteNone) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]_[0-2][0-9][0-5][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????_????.log"), DTGM_ARG5))
		.WillOnce(t::Invoke([this](t::Unused, t::Unused, LPVOID lpFindFileData, t::Unused, t::Unused, t::Unused) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-01_1903.log");
			return m_hFind;
		}));
	EXPECT_CALL(m_mock, FindNextFileW(m_hFind, DTGM_ARG1))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-01_1901.log");
			return TRUE;
		}))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-01_1902.log");
			return TRUE;
		}))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_mock, FindClose(m_hFind));

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kEveryMinute, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} DEBUG [^\\n]+ Test\\n"));
}

TEST_F(LogWriter_Test, Log_DailyFiveOldFiles_CreateFileDeleteTwo) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.WillOnce(t::Invoke([this](t::Unused, t::Unused, LPVOID lpFindFileData, t::Unused, t::Unused, t::Unused) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-05.log");
			return m_hFind;
		}));
	EXPECT_CALL(m_mock, FindNextFileW(m_hFind, DTGM_ARG1))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-03.log");
			return TRUE;
		}))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-01.log");
			return TRUE;
		}))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-02.log");
			return TRUE;
		}))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-04.log");
			return TRUE;
		}))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_mock, FindClose(m_hFind));

	EXPECT_CALL(m_mock, DeleteFileW(t::StrEq(L"X:\\testing\\logs\\ll_test.2019-01-01.log")));
	EXPECT_CALL(m_mock, DeleteFileW(t::StrEq(L"X:\\testing\\logs\\ll_test.2019-01-02.log")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[0-9:. -]{23} DEBUG [^\\n]+ Test\\n"));
}

TEST_F(LogWriter_Test, FileTimeToSystemTime_Error_LogErrorAndDoNotCreateFile) {
	EXPECT_CALL(m_mock, FileTimeToSystemTime(DTGM_ARG2))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, FALSE));

	EXPECT_CALL(m_mock, FindFirstFileExW(DTGM_ARG6))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n([0-9:. -]{23} ERROR [^\\n]*RollFile Error rolling log: [^\\n]+ \\(50\\)\\n){2}"));
}

TEST_F(LogWriter_Test, CreateFile_TemporaryErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_FILE_EXISTS, INVALID_HANDLE_VALUE))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(1);  // one log event is lost when CreateFileW fails
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.Times(2);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} ERROR [^\\n]* RollFile Error creating log: [^\\n]+ \\(80\\)\\n"));
}

TEST_F(LogWriter_Test, CreateFile_PermanentErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillRepeatedly(detours_gmock::SetLastErrorAndReturn(ERROR_FILE_EXISTS, INVALID_HANDLE_VALUE));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.Times(3);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n([0-9:. -]{23} ERROR [^\\n]* RollFile Error creating log: [^\\n]+ \\(80\\)\\n){2}"));
}

TEST_F(LogWriter_Test, WriteFile_WritePartially_WriteChunked) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, t::Unused) {
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite / 2;
			return TRUE;
		}))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, t::Unused) {
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite / 2;
			return TRUE;
		}))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex(std::string("[0-9:. -]{23} DEBUG \\[[0-9]+\\] ") + llamalog::GetFilename(__FILE__) + ":99 TestBody Test\\n"));
}

TEST_F(LogWriter_Test, WriteFile_TemporaryError_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.WillOnce(t::Invoke([](t::Unused, t::Unused, t::Unused, LPDWORD lpNumberOfBytesWritten, t::Unused) {
			*lpNumberOfBytesWritten = 0;
			SetLastError(ERROR_FILE_CORRUPT);
			return FALSE;
		}))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} ERROR [^\\n]* Log Error writing [0-9]+ bytes to log: [^\\n]+ \\(1392\\)\\n"));
}

TEST_F(LogWriter_Test, WriteFile_PermanentError_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(3)
		.WillRepeatedly(t::Invoke([](t::Unused, t::Unused, t::Unused, LPDWORD lpNumberOfBytesWritten, t::Unused) {
			*lpNumberOfBytesWritten = 0;
			SetLastError(ERROR_FILE_CORRUPT);
			return FALSE;
		}));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n([0-9:. -]{23} ERROR [^\\n]* Log Error writing [0-9]+ bytes to log: [^\\n]+ \\(1392\\)\\n){2}"));
}

TEST_F(LogWriter_Test, CloseHandle_ErrorDuringDestruct_KeepSilent) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4));
	EXPECT_CALL(m_mock, CloseHandle(m_hFile))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, FALSE));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	// silent, error happens after closing the writer
	EXPECT_EQ(1, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+ DEBUG [^\\n]+\\n"));
}

TEST_F(LogWriter_Test, CloseHandle_ErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]_[0-2][0-9][0-5][0-9][0-5][0-9]\\.log"), DTGM_ARG6))
		.Times(t::Between(2, 3))
		.WillRepeatedly(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(3);
	EXPECT_CALL(m_mock, CloseHandle(m_hFile))
		.Times(t::Between(2, 3))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_NOT_SUPPORTED, FALSE))
		.WillRepeatedly(t::DoDefault());

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????_??????.log"), DTGM_ARG5))
		.Times(t::Between(2, 3));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kEverySecond, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	Sleep(1500);
	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");

	llamalog::Shutdown();

	EXPECT_EQ(3, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[^\\n]+\\n[0-9:. -]{23} WARN [^\\n]* RollFile Error closing log: [^\\n]+ \\(50\\)\\n"));
}

TEST_F(LogWriter_Test, FindFirstFileEx_ErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(2);
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_PARAMETER, INVALID_HANDLE_VALUE));
	EXPECT_CALL(m_mock, FindNextFileW(DTGM_ARG2))
		.Times(0);
	EXPECT_CALL(m_mock, FindClose(DTGM_ARG1))
		.Times(0);

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} WARN [^\\n]* RollFile Error deleting log: [^\\n]+ \\(87\\)\\n"));
}

TEST_F(LogWriter_Test, FindNextFileW_ErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(2);
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.WillOnce(t::Invoke([this](t::Unused, t::Unused, LPVOID lpFindFileData, t::Unused, t::Unused, t::Unused) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-05.log");
			return m_hFind;
		}));
	EXPECT_CALL(m_mock, FindNextFileW(m_hFind, DTGM_ARG1))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_PARAMETER, FALSE));
	EXPECT_CALL(m_mock, FindClose(m_hFind));

	EXPECT_CALL(m_mock, DeleteFileW(DTGM_ARG1))
		.Times(0);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 3u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} WARN [^\\n]* RollFile Error deleting log: [^\\n]+ \\(87\\)\\n"));
}

TEST_F(LogWriter_Test, FindClose_ErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(2);
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.WillOnce(t::Invoke([this](t::Unused, t::Unused, LPVOID lpFindFileData, t::Unused, t::Unused, t::Unused) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-05.log");
			return m_hFind;
		}));
	EXPECT_CALL(m_mock, FindNextFileW(m_hFind, DTGM_ARG1));
	EXPECT_CALL(m_mock, FindClose(m_hFind))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_INVALID_HANDLE, FALSE));

	EXPECT_CALL(m_mock, DeleteFileW(t::StrEq(L"X:\\testing\\logs\\ll_test.2019-01-05.log")))
		.Times(1);

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 0u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} WARN [^\\n]* RollFile Error deleting log: [^\\n]+ \\(6\\)\\n"));
}

TEST_F(LogWriter_Test, DeleteFileW_ErrorDuringRollFile_LogError) {
	EXPECT_CALL(m_mock, CreateFileW(MatchesRegex(L"X:\\\\testing\\\\logs\\\\ll_test\\.2[0-9]{3}[01][0-9][0-3][0-9]\\.log"), DTGM_ARG6))
		.WillOnce(t::Return(m_hFile));
	EXPECT_CALL(m_mock, WriteFile(m_hFile, DTGM_ARG4))
		.Times(2);
	EXPECT_CALL(m_mock, CloseHandle(m_hFile));

	EXPECT_CALL(m_mock, FindFirstFileExW(t::StrEq(L"X:\\testing\\logs\\ll_test.????????.log"), DTGM_ARG5))
		.WillOnce(t::Invoke([this](t::Unused, t::Unused, LPVOID lpFindFileData, t::Unused, t::Unused, t::Unused) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-05.log");
			return m_hFind;
		}));
	EXPECT_CALL(m_mock, FindNextFileW(m_hFind, DTGM_ARG1))
		.WillOnce(t::Invoke([](t::Unused, LPVOID lpFindFileData) {
			wcscpy_s(((WIN32_FIND_DATAW*) lpFindFileData)->cFileName, L"ll_test.2019-01-03.log");
			return TRUE;
		}))
		.WillOnce(t::DoDefault());
	EXPECT_CALL(m_mock, FindClose(m_hFind));

	EXPECT_CALL(m_mock, DeleteFileW(t::StrEq(L"X:\\testing\\logs\\ll_test.2019-01-05.log")))
		.WillOnce(detours_gmock::SetLastErrorAndReturn(ERROR_FILE_NOT_FOUND, FALSE));
	EXPECT_CALL(m_mock, DeleteFileW(t::StrEq(L"X:\\testing\\logs\\ll_test.2019-01-03.log")));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<llamalog::RollingFileWriter> fileWriter = std::make_unique<llamalog::RollingFileWriter>(Priority::kDebug, "X:\\testing\\logs\\", "ll_test.log", llamalog::RollingFileWriter::Frequency::kDaily, 0u);
	llamalog::Initialize(std::move(writer), std::move(fileWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", "Test");
	llamalog::Shutdown();

	EXPECT_EQ(2, m_lines);
	EXPECT_THAT(m_out.str(), MatchesRegex("[^\\n]+\\n[0-9:. -]{23} WARN [^\\n]* RollFile Error deleting log 'll_test.2019-01-05.log': [^\\n]+ \\(2\\)\\n"));
}

TEST_F(LogWriter_Test, StdErrWriter_Log_WriteOutput) {
	std::string value;
	EXPECT_CALL(m_mock, fputs(t::_, stderr))
		.WillOnce(t::Invoke([&value](const char* str, t::Unused) {
			value = str;
			return 1;
		}));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<StdErrWriter> stdErrWriter = std::make_unique<StdErrWriter>(Priority::kDebug);
	llamalog::Initialize(std::move(writer), std::move(stdErrWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Shutdown();  // shutdown instead of flush before detach

	EXPECT_EQ(1, m_lines);
	EXPECT_EQ(m_out.str(), value);
}

TEST_F(LogWriter_Test, DebugWriter_Log_WriteOutput) {
	std::string value;
	EXPECT_CALL(m_mock, OutputDebugStringA)
		.WillOnce(t::Invoke([&value](LPCSTR lpOutputString) {
			value = lpOutputString;
		}));

	std::unique_ptr<StringWriter> writer = std::make_unique<StringWriter>(Priority::kDebug, m_out, m_lines);
	std::unique_ptr<DebugWriter> debugWriter = std::make_unique<DebugWriter>(Priority::kDebug);
	llamalog::Initialize(std::move(writer), std::move(debugWriter));

	llamalog::Log(Priority::kDebug, GetFilename(__FILE__), 99, __func__, "{}", L"Test");
	llamalog::Shutdown();  // shutdown instead of flush before detach

	EXPECT_EQ(1, m_lines);
	EXPECT_EQ(m_out.str(), value);
}

}  // namespace llamalog::test
