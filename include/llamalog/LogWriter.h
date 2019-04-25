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

/// @file
#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>

namespace llamalog {

enum class Priority : std::uint8_t;
class LogLine;

/// @brief The base class for all log writers.
/// @details Except for the constructor and destructor, all access to a `LogWriter` is from a single thread.
class __declspec(novtable) LogWriter {
public:
	/// @brief Creates a new log writer with a particular `#Priority`.
	/// @param priority Only events at this `#Priority` or above will be logged by this writer.
	explicit LogWriter(Priority priority) noexcept;
	LogWriter(const LogWriter&) = delete;  ///< @nocopyconstructor
	LogWriter(LogWriter&&) = delete;       ///< @nomoveconstructor
	virtual ~LogWriter() noexcept = default;

public:
	LogWriter& operator=(const LogWriter&) = delete;  ///< @noassignmentoperator
	LogWriter& operator=(LogWriter&&) = delete;       ///< @nomoveoperator

public:
	/// @brief Check if this writer logs at the specific priority.
	/// @param priority The `#Priority` of the `LogLine`.
	/// @return `true` if this writer logs events with the respective priority.
	/// @copyright Derived from `is_logged(LogLevel)` from NanoLog.
	[[nodiscard]] bool IsLogged(Priority priority) const noexcept;

	/// @brief Dynamically change the `#Priority`
	/// @param priority The new `#Priority` for this writer.
	/// @copyright Derived from `set_log_level(LogLevel)` from NanoLog.
	void SetPriority(Priority priority) noexcept;

	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	virtual void Log(const LogLine& logLine) = 0;

public:
	/// @brief Return a string for a `#Priority`.
	/// @param priority A `#Priority`.
	/// @return One of `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` - or `-` for unknown priorities.
	/// @copyright Derived from `to_string(LogLevel)` from NanoLog.
	[[nodiscard]] static __declspec(noalias) _Ret_z_ const char* FormatPriority(Priority priority) noexcept;

	/// @brief Format a timestamp as `YYYY-MM-DD HH:mm:ss.SSS`.
	/// @details In case of an error, `0000-00-00 00:00:00.000` is returned.
	/// @param timestamp The timestamp.
	/// @return The timestamp as a string.
	[[nodiscard]] static std::string FormatTimestamp(const FILETIME& timestamp);

	/// @brief Format a timestamp as `YYYY-MM-DD HH:mm:ss.SSS` to a target buffer.
	/// @details The buffer MUST be of type `fmt::basic_memory_buffer`.
	/// In case of an error, `0000-00-00 00:00:00.000` is written.
	/// @remarks Using a template instead of the concrete type removes the need to add {fmt} as a depency for this header.
	/// @tparam Out The target buffer which MUST be of type `fmt::basic_memory_buffer`.
	/// @param out The target buffer.
	/// @param timestamp The timestamp.
	template <typename Out>
	static void FormatTimestampTo(Out& out, const FILETIME& timestamp);

private:
	std::atomic<Priority> m_priority;  ///< @brief Atomic store for the `#Priority`.
};


/// @brief A `#LogWriter` sending all output to `OutputDebugString`.
class DebugWriter : public LogWriter {
	using LogWriter::LogWriter;

protected:
	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	void Log(const LogLine& logLine) final;
};


/// @brief A `LogWriter` that writes all output to a file.
/// @details A new file is started each day at 00:00:00 UTC.
/// @copyright Loosely based on `class FileWriter` from NanoLog.
class RollingFileWriter : public LogWriter {
public:
	/// @brief The rolling frequency.
	/// @details @internal The value specifies the number of 100 nanosecond intervals to match `FILETIME`.
	enum class Frequency : std::uint8_t {
		kMonthly,      ///< @brief Start a new file every month.
		kDaily,        ///< @brief Start a new file every day.
		kHourly,       ///< @brief Start a new file every hour.
		kEveryMinute,  ///< @brief Start a new file every minute.
		kEverySecond,  ///< @brief Start a new file every second. @note The main use is for testing.
		kCount         ///< @brief Maximum value for calculations.
	};

public:
	/// @brief Create the writer.
	/// @warning The logger deletes any log files older than the neweset @p maxFiles files. Please ensure that any
	/// files are copied to a different location if their contents are required for a longer period.
	/// @param priority Only events at this `#Priority` or above will be logged by this writer.
	/// @param directory Directory where to store the log files.
	/// @param fileName File name for the log files.
	/// @param frequency The interval at which a new the writer starts a new file.
	/// @param maxFiles The maximum number of old log files which should be kept in @p directory.
	RollingFileWriter(Priority priority, std::string directory, std::string fileName, Frequency frequency = Frequency::kDaily, std::uint32_t maxFiles = 60) noexcept;  // NOLINT(readability-magic-numbers)

	RollingFileWriter(const RollingFileWriter&) = delete;  ///< @nocopyconstructor
	RollingFileWriter(RollingFileWriter&&) = delete;       ///< @nomoveconstructor
	~RollingFileWriter() noexcept;

public:
	RollingFileWriter& operator=(const RollingFileWriter&) = delete;  ///< @noassignmentoperator
	RollingFileWriter& operator=(RollingFileWriter&&) = delete;       ///< @nomoveoperator

protected:
	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	void Log(const LogLine& logLine) final;

private:
	/// @brief Start the next file.
	/// @param logLine The `LogLine` which triggered the roll over.
	void RollFile(const LogLine& logLine);

private:
	const std::string m_directory;   ///< @brief The directory.
	const std::string m_fileName;    ///< @brief The file name.
	const Frequency m_frequency;     ///< @brief The rolling frequency.
	const std::uint32_t m_maxFiles;  ///< @brief The maximum number of files to keep.

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast): INVALID_HANDLE_VALUE is part of the Windows API.
	HANDLE m_hFile = INVALID_HANDLE_VALUE;  ///< @brief The handle of the log file. @hideinitializer
	FILETIME m_nextRollAt = {0};            ///< @brief Next time to roll over. @hideinitializer
};

}  // namespace llamalog
