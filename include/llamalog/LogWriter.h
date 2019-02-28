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
#include <fstream>
#include <memory>
#include <string>

namespace llamalog {

enum class LogLevel : std::uint8_t;
class LogLine;

/// @brief The base class for all log writers.
class LogWriter {
protected:
	/// @brief Creates a new log writer with a particular `#LogLevel`.
	/// @param level Only events at this `#LogLevel` or above will be logged by this writer.
	explicit LogWriter(LogLevel level) noexcept;

public:
	LogWriter(const LogWriter&) = delete;  ///< @nocopyconstructor
	LogWriter(LogWriter&&) = delete;       ///< @nomoveconstructor
	virtual ~LogWriter() noexcept = default;

public:
	LogWriter& operator=(const LogWriter&) = delete;  ///< @noassignmentoperator
	LogWriter& operator=(LogWriter&&) = delete;       ///< @nomoveoperator

public:
	/// @brief Check if this writer logs at the specific level.
	/// @param level The `#LogLevel` of the `LogLine`.
	/// @return `true` if this writer logs events with the respective level.
	/// @copyright Derived from `is_logged(LogLevel)` from NanoLog.
	bool IsLogged(LogLevel level) const noexcept;

	/// @brief Dynamically change the `#LogLevel`
	/// @param level The new `#LogLevel` for this writer.
	/// @copyright Derived from `set_log_level(LogLevel)` from NanoLog.
	void SetLogLevel(LogLevel level) noexcept;

	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	virtual void Log(const LogLine& logLine) = 0;

private:
	std::atomic<LogLevel> m_logLevel;  ///< @brief Atomic store for the `#LogLevel`.
};


/// @brief A `#LogWriter` sending all output to `OutputDebugString`.
class DebugWriter : public LogWriter {
public:
	/// @brief Create the writer.
	/// @param level Only events at this `#LogLevel` or above will be logged by this writer.
	explicit DebugWriter(LogLevel level) noexcept;

protected:
	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	void Log(const LogLine& logLine) final;
};


/// @brief A `LogWriter` that writes all output to a file.
/// @details A new file is started each day at 00:00:00 UTC.
/// @copyright Loosely based on `class FileWriter` from NanoLog.
class DailyRollingFileWriter : public LogWriter {
public:
	/// @brief Create the writer.
	/// @param level Only events at this `#LogLevel` or above will be logged by this writer.
	/// @param directory Directory where to store the log files.
	/// @param fileName File name for the log files.
	DailyRollingFileWriter(LogLevel level, std::string directory, std::string fileName);

protected:
	/// @brief Produce output for a `LogLine`.
	/// @param logLine The data.
	/// @copyright Derived from `NanoLogLine::stringify(std::ostream&)` from NanoLog.
	void Log(const LogLine& logLine) final;

private:
	/// @brief Start the next file.
	void RollFile();

private:
	const std::string m_directory;  ///< @brief The directory.
	const std::string m_fileName;   ///< @brief The file name.

	/// @copyright Same as `FileWriter::m_os` from NanoLog.
	std::unique_ptr<std::ofstream> m_os;  ///< @brief The output stream.
	FILETIME m_nextRollAt = {0};          ///< @brief Next time to roll over. @hideinitializer
};

}  // namespace llamalog
