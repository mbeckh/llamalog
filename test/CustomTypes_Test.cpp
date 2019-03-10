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

#include "llamalog/CustomTypes.h"

#include "llamalog/LogLine.h"

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <string>
#include <type_traits>

namespace llamalog::test {

namespace {

thread_local int g_instancesCreated;
thread_local int g_destructorCalled;
thread_local int g_copyConstructorCalled;
thread_local int g_moveConstructorCalled;

class CustomTypesTest : public testing::Test {
public:
	CustomTypesTest() {
		g_instancesCreated = 0;
		g_destructorCalled = 0;
		g_copyConstructorCalled = 0;
		g_moveConstructorCalled = 0;
	}
};

class TriviallyCopyable final {
public:
	explicit TriviallyCopyable(int value)
		: m_value(value) {
		// empty
	}
	TriviallyCopyable(const TriviallyCopyable&) = default;
	TriviallyCopyable(TriviallyCopyable&&) = default;
	~TriviallyCopyable() = default;

public:
	TriviallyCopyable& operator=(const TriviallyCopyable&) = default;
	TriviallyCopyable& operator=(TriviallyCopyable&&) = default;

public:
	int GetInstanceNo() const {
		return m_instanceNo;
	}
	int GetValue() const {
		return m_value;
	}

private:
	int m_instanceNo = ++g_instancesCreated;
	int m_value;
};
static_assert(std::is_trivially_copyable_v<TriviallyCopyable>);


class MoveConstructible final {
public:
	explicit MoveConstructible(int value)
		: m_value(value) {
		// empty
	}
	MoveConstructible(const MoveConstructible& other)
		: m_value(other.m_value) {
		++g_copyConstructorCalled;
	}
	MoveConstructible(MoveConstructible&& other) noexcept
		: m_value(other.m_value) {
		++g_moveConstructorCalled;
	}
	~MoveConstructible() {
		++g_destructorCalled;
	}

public:
	MoveConstructible& operator=(const MoveConstructible&) = delete;
	MoveConstructible& operator=(MoveConstructible&&) = delete;

public:
	int GetInstanceNo() const {
		return m_instanceNo;
	}
	int GetValue() const {
		return m_value;
	}

private:
	const int m_instanceNo = ++g_instancesCreated;
	int m_value;
};
static_assert(!std::is_trivially_copyable_v<MoveConstructible>);
static_assert(std::is_nothrow_move_constructible_v<MoveConstructible>);


class CopyConstructible final {
public:
	explicit CopyConstructible(int value)
		: m_value(value) {
		// empty
	}
	CopyConstructible(const CopyConstructible& other) noexcept
		: m_value(other.m_value) {
		++g_copyConstructorCalled;
	}
	CopyConstructible(CopyConstructible&& other) = delete;
	~CopyConstructible() {
		++g_destructorCalled;
	}

public:
	CopyConstructible& operator=(const CopyConstructible&) = delete;
	CopyConstructible& operator=(CopyConstructible&&) = delete;

public:
	int GetInstanceNo() const {
		return m_instanceNo;
	}
	int GetValue() const {
		return m_value;
	}

private:
	const int m_instanceNo = ++g_instancesCreated;
	int m_value;
};
static_assert(!std::is_trivially_copyable_v<CopyConstructible>);
static_assert(!std::is_nothrow_move_constructible_v<CopyConstructible>);
static_assert(std::is_nothrow_copy_constructible_v<CopyConstructible>);

}  // namespace

}  // namespace llamalog::test

template <>
struct fmt::formatter<llamalog::test::TriviallyCopyable> {
public:
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(const llamalog::test::TriviallyCopyable& arg, FormatContext& ctx) {
		return format_to(ctx.out(), "T_{}_{}", arg.GetInstanceNo(), arg.GetValue());
	}
};

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::TriviallyCopyable& arg) {
	return logLine.AddCustomArgument(arg);
}


template <>
struct fmt::formatter<llamalog::test::MoveConstructible> {
public:
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(const llamalog::test::MoveConstructible& arg, FormatContext& ctx) {
		return format_to(ctx.out(), "M_{}_{}", arg.GetInstanceNo(), arg.GetValue());
	}
};

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::MoveConstructible& arg) {
	return logLine.AddCustomArgument(arg);
}


template <>
struct fmt::formatter<llamalog::test::CopyConstructible> {
public:
	template <typename ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(const llamalog::test::CopyConstructible& arg, FormatContext& ctx) {
		return format_to(ctx.out(), "C_{}_{}", arg.GetInstanceNo(), arg.GetValue());
	}
};

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::test::CopyConstructible& arg) {
	return logLine.AddCustomArgument(arg);
}

namespace llamalog::test {

namespace {

LogLine GetLogLine(const char* const szPattern = "{}") {
	return LogLine(LogLevel::kDebug, "file.cpp", 99, "myfunction()", szPattern);
}

}  // namespace

TEST_F(CustomTypesTest, TriviallyCopyable_IsValue_PrintValue) {
	{
		LogLine logLine = GetLogLine();
		{
			const TriviallyCopyable arg(7);
			ASSERT_EQ(1, arg.GetInstanceNo());
			ASSERT_EQ(1, g_instancesCreated);

			logLine << arg;
			EXPECT_EQ(1, g_instancesCreated);
		}
		EXPECT_EQ(1, g_instancesCreated);
		const std::string str = logLine.GetLogMessage();

		// everything is just copied as raw binary data
		EXPECT_EQ("T_1_7", str);
		EXPECT_EQ(1, g_instancesCreated);
	}
	EXPECT_EQ(1, g_instancesCreated);
}

TEST_F(CustomTypesTest, MoveConstructible_IsValue_PrintValue) {
	{
		LogLine logLine = GetLogLine();
		{
			const MoveConstructible arg(7);
			ASSERT_EQ(1, arg.GetInstanceNo());
			ASSERT_EQ(1, g_instancesCreated);
			ASSERT_EQ(0, g_copyConstructorCalled);
			ASSERT_EQ(0, g_moveConstructorCalled);
			ASSERT_EQ(0, g_destructorCalled);

			logLine << arg;
			EXPECT_EQ(2, g_instancesCreated);
			EXPECT_EQ(1, g_copyConstructorCalled);
			EXPECT_EQ(0, g_moveConstructorCalled);
			EXPECT_EQ(0, g_destructorCalled);

			// force an internal buffer re-allocation
			logLine << std::string(256, 'x');
			EXPECT_EQ(3, g_instancesCreated);
			EXPECT_EQ(1, g_copyConstructorCalled);
			EXPECT_EQ(1, g_moveConstructorCalled);
			EXPECT_EQ(1, g_destructorCalled);
		}
		EXPECT_EQ(3, g_instancesCreated);
		EXPECT_EQ(1, g_copyConstructorCalled);
		EXPECT_EQ(1, g_moveConstructorCalled);
		EXPECT_EQ(2, g_destructorCalled);
		const std::string str = logLine.GetLogMessage();

		EXPECT_EQ("M_3_7", str);  // no need to print string of x'es
		EXPECT_EQ(3, g_instancesCreated);
		EXPECT_EQ(1, g_copyConstructorCalled);
		EXPECT_EQ(1, g_moveConstructorCalled);
		EXPECT_EQ(2, g_destructorCalled);
	}
	EXPECT_EQ(3, g_instancesCreated);
	EXPECT_EQ(1, g_copyConstructorCalled);
	EXPECT_EQ(1, g_moveConstructorCalled);
	EXPECT_EQ(3, g_destructorCalled);
}

TEST_F(CustomTypesTest, CopyConstructible_IsValue_PrintValue) {
	{
		LogLine logLine = GetLogLine();
		{
			const CopyConstructible arg(7);
			ASSERT_EQ(1, arg.GetInstanceNo());
			ASSERT_EQ(1, g_instancesCreated);
			ASSERT_EQ(0, g_copyConstructorCalled);
			ASSERT_EQ(0, g_moveConstructorCalled);
			ASSERT_EQ(0, g_destructorCalled);

			logLine << arg;
			EXPECT_EQ(2, g_instancesCreated);
			EXPECT_EQ(1, g_copyConstructorCalled);
			EXPECT_EQ(0, g_moveConstructorCalled);
			EXPECT_EQ(0, g_destructorCalled);

			// force an internal buffer re-allocation
			logLine << std::string(256, 'x');
			EXPECT_EQ(3, g_instancesCreated);
			EXPECT_EQ(2, g_copyConstructorCalled);
			EXPECT_EQ(0, g_moveConstructorCalled);
			EXPECT_EQ(1, g_destructorCalled);
		}
		EXPECT_EQ(3, g_instancesCreated);
		EXPECT_EQ(2, g_copyConstructorCalled);
		EXPECT_EQ(0, g_moveConstructorCalled);
		EXPECT_EQ(2, g_destructorCalled);
		const std::string str = logLine.GetLogMessage();

		EXPECT_EQ("C_3_7", str);  // no need to print string of x'es
		EXPECT_EQ(3, g_instancesCreated);
		EXPECT_EQ(2, g_copyConstructorCalled);
		EXPECT_EQ(0, g_moveConstructorCalled);
		EXPECT_EQ(2, g_destructorCalled);
	}
	EXPECT_EQ(3, g_instancesCreated);
	EXPECT_EQ(2, g_copyConstructorCalled);
	EXPECT_EQ(0, g_moveConstructorCalled);
	EXPECT_EQ(3, g_destructorCalled);
}

}  // namespace llamalog::test
