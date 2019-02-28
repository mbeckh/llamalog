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
/// @copyright Code marked with "from NanoLog" is based on NanoLog (https://github.com/Iyengar111/NanoLog, commit
/// 40a53c36e0336af45f7664abeb939f220f78273e), copyright 2016 Karthik Iyengar and distributed unter the MIT License.

/*

Distributed under the MIT License (MIT)

	Copyright (c) 2016 Karthik Iyengar

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "llamalog/llamalog.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace llamalog {

namespace {

/// @brief A lock free buffer supporting parallel readers and writers.
/// @copyright Derived from `class Buffer` from NanoLog.
class Buffer final {
private:
	/// @brief Wrapper for the actual data to support move operation.
	/// @copyright Same as `struct Buffer::Item` from NanoLog.
	class Item {
	public:
		/// @brief Move a `LogLine` into the structure.
		/// @param logLine A `LogLine`.
		explicit Item(LogLine&& logLine) noexcept
			: logLine(std::move(logLine)) {
			// empty
		}
		Item(const Item&) = delete;  ///< @nocopyconstructor
		Item(Item&&) = delete;       ///< @nomoveconstructor
		~Item() = default;

	public:
		Item& operator=(const Item&) = delete;  ///< @noassignmentoperator
		Item& operator=(Item&&) = delete;       ///< @nomoveoperator

	public:
		/// @copyright Same as `Buffer::Item::logline` from NanoLog.
		LogLine logLine;  ///< @brief The log entry.
	};

	static_assert(sizeof(Item) == 256, "size of Item != 256");

public:
	/// @brief Initialize the internal structures.
	/// @copyright A modified version of `Buffer::Buffer` from NanoLog.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): m_buffer needs not initialization, m_writeState is initialized in a loop.
	Buffer() noexcept
		: m_remaining(kBufferSize) {
		for (std::atomic_bool& writeState : m_writeState) {
			writeState.store(false, std::memory_order_relaxed);
		}
	}
	Buffer(const Buffer&) = delete;  ///< @nocopyconstructor
	Buffer(Buffer&&) = delete;       ///< @nomoveconstructor

	/// @brief Call the destructors of all entries which had been moved into this buffer.
	/// @copyright Derived from `Buffer::~Buffer` from NanoLog.
	~Buffer() noexcept {
		const std::uint_fast32_t writeCount = kBufferSize - m_remaining.load();
		for (std::uint_fast32_t i = 0; i < writeCount; ++i) {
			reinterpret_cast<Item*>(m_buffer)[i].~Item();
		}
	}

public:
	Buffer& operator=(const Buffer&) = delete;  ///< @noassignmentoperator
	Buffer& operator=(Buffer&&) = delete;       ///< @nomoveoperator

	/// @brief Add a new item to this buffer.
	/// @param writeIndex The current index.
	/// @param logLine The `LogLine` to add.
	/// @return Returns `true` if we need to switch to next buffer after this operation.
	/// @copyright Same as `Buffer::push` from NanoLog.
	bool Push(const std::uint_fast32_t writeIndex, LogLine&& logLine) noexcept {
		Item* item = new (&reinterpret_cast<Item*>(m_buffer)[writeIndex]) Item(std::move(logLine));
		item->logLine.GenerateTimestamp();
		m_writeState[writeIndex].store(true, std::memory_order_release);
		return m_remaining.fetch_sub(1, std::memory_order_acquire) == 1;
	}

	/// @brief Gets an item from this buffer.
	/// @warning This function MUST NOT be called more than once for each index because the data is moved internally.
	/// @param readIndex The index to read.
	/// @param logLine The `LogLine` receiving the next event.
	/// @return Returns `true` if a value is available and has been copied to @p logLine.
	/// @copyright Same as `Buffer::try_pop` from NanoLog.
	bool TryPop(const std::uint_fast32_t readIndex, LogLine& logLine) noexcept {
		if (m_writeState[readIndex].load(std::memory_order_acquire)) {
			Item& item = reinterpret_cast<Item*>(m_buffer)[readIndex];
			logLine = std::move(item.logLine);
			return true;
		}
		return false;
	}

public:
	/// @details The size of `m_buffer` is just under 8 MB (to account for the two atomic fields.
	/// @copyright Same as `Buffer::size` from NanoLog.
	static constexpr std::uint32_t kBufferSize = 32768 - 1;  ///< @brief The number of elements in the buffer.

private:
	/// @copyright Same as `Buffer::m_buffer` from NanoLog, but on stack instead of heap.
	std::byte m_buffer[kBufferSize * sizeof(Item)];  ///< @brief The buffer holding the log events.
	std::atomic_uint32_t m_remaining;                ///< The number of free entries in this buffer.

	/// @copyright Same as `Buffer::m_write_state` from NanoLog, but on stack instead of heap.
	std::atomic_bool m_writeState[kBufferSize];  ///< @brief `true` if this entry has been written.
};


/// @brief A simple spin lock class supporting stack unwinding.
/// @copyright Same as `struct SpinLock` from NanoLog.
class SpinLock final {
public:
	/// @brief Get the spin lock.
	/// @param flag The flag to lock. @details Only a reference is kept internally.
	/// @copyright Same as `SpinLock::SpinLock` from NanoLog.
	explicit SpinLock(std::atomic_flag& flag) noexcept
		: m_flag(flag) {
		while (m_flag.test_and_set(std::memory_order_acquire)) {
			// empty
		}
	}
	SpinLock(const SpinLock&) = delete;  ///< @nocopyconstructor
	SpinLock(SpinLock&&) = delete;       ///< @nomoveconstructor

	/// @brief Release the spin lock.
	/// @copyright Same as `SpinLock::~SpinLock` from NanoLog.
	~SpinLock() noexcept {
		m_flag.clear(std::memory_order_release);
	}

public:
	SpinLock& operator=(const SpinLock&) = delete;   ///< @noassignmentoperator
	SpinLock& operator=(const SpinLock&&) = delete;  ///< @nomoveoperator

private:
	/// @copyright Same as `SpinLock::m_flag` from NanoLog.
	std::atomic_flag& m_flag;  ///< @brief Reference to the flag.
};


/// @brief A queue built using multiple `Buffer` objects.
/// @copyright Same as `class QueueBuffer` from NanoLog.
class QueueBuffer final {
public:
	/// @brief Create a new queue.
	/// @copyright Same as `QueueBuffer::QueueBuffer` from NanoLog.
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): m_currentWriteBuffer is initialized by function call.
	QueueBuffer() {
		SetupNextWriteBuffer();
	}
	QueueBuffer(const QueueBuffer&) = delete;  ///< @nocopyconstructor
	QueueBuffer(QueueBuffer&&) = delete;       ///< @nomoveconstructor

	~QueueBuffer() noexcept = default;

public:
	QueueBuffer& operator=(const QueueBuffer&) = delete;  ///< @noassignmentoperator
	QueueBuffer& operator=(QueueBuffer&&) = delete;       ///< @nomoveoperator

public:
	/// @brief Add a new entry to the queue.
	/// @param logLine The new entry.
	/// @copyright Same as `QueueBuffer::push` from NanoLog.
	void Push(LogLine&& logLine) {
		const std::uint_fast32_t writeIndex = m_writeIndex.fetch_add(1, std::memory_order_relaxed);
		if (writeIndex < Buffer::kBufferSize) {
			if (m_currentWriteBuffer.load(std::memory_order_acquire)->Push(writeIndex, std::move(logLine))) {
				SetupNextWriteBuffer();
			}
		} else {
			// someone else is preparing a new buffer
			while (m_writeIndex.load(std::memory_order_acquire) >= Buffer::kBufferSize) {
				// empty
			}
			Push(std::move(logLine));
		}
	}

	/// @brief Read the next available `LogLine` from this queue.
	/// @param logLine Receives the next entry.
	/// @return `true` if data is available.
	/// @copyright Same as `QueueBuffer::try_pop` from NanoLog.
	bool TryPop(LogLine& logLine) noexcept {
		if (m_currentReadBuffer == nullptr) {
			m_currentReadBuffer = GetNextReadBuffer();
		}

		Buffer* readBuffer = m_currentReadBuffer;

		if (readBuffer == nullptr) {
			return false;
		}

		if (readBuffer->TryPop(m_readIndex, logLine)) {
			++m_readIndex;
			if (m_readIndex == Buffer::kBufferSize) {
				m_readIndex = 0;
				m_currentReadBuffer = nullptr;

				SpinLock spinLock(m_flag);
				m_buffers.pop();
			}
			return true;
		}

		return false;
	}

private:
	/// @brief Create a new `Buffer` for writing.
	/// @copyright Same as `QueueBuffer::setup_next_write_buffer` from NanoLog.
	void SetupNextWriteBuffer() {
		std::unique_ptr<Buffer> nextWriteBuffer = std::make_unique<Buffer>();
		m_currentWriteBuffer.store(nextWriteBuffer.get(), std::memory_order_release);

		SpinLock spinLock(m_flag);
		m_buffers.push(std::move(nextWriteBuffer));
		m_writeIndex.store(0, std::memory_order_relaxed);
	}

	/// @brief Get next `Buffer` for reading.
	/// @return The next `Buffer` or `nullptr` if none is currently available.
	/// @copyright Same as `QueueBuffer::get_next_read_buffer` from NanoLog.
	Buffer* GetNextReadBuffer() noexcept {
		SpinLock spinLock(m_flag);
		return m_buffers.empty() ? nullptr : m_buffers.front().get();
	}

private:
	/// @copyright Same as `QueueBuffer::m_read_index` from NanoLog.
	std::uint_fast32_t m_readIndex = 0;  ///< @brief The next index for reading. @hideinitializer

	/// @copyright Same as `QueueBuffer::m_current_read_buffer` from NanoLog.
	Buffer* m_currentReadBuffer = nullptr;  ///< @brief The buffer used for reading entries. @hideinitializer

	/// @copyright Same as `QueueBuffer::m_write_index` from NanoLog.
	std::atomic_uint_fast32_t m_writeIndex = 0;  ///< @brief The next free index in the buffer. @hideinitializer

	/// @copyright Same as `QueueBuffer::m_current_write_buffer` from NanoLog.
	std::atomic<Buffer*> m_currentWriteBuffer;  ///< @brief The buffer used for writing new entries.

	/// @copyright Same as `QueueBuffer::m_flag` from NanoLog.
	std::atomic_flag m_flag = ATOMIC_FLAG_INIT;  ///< @brief Flag protecting the `m_buffers` structure. @hideinitializer

	/// @copyright Same as `QueueBuffer::m_buffers` from NanoLog.
	std::queue<std::unique_ptr<Buffer>> m_buffers;  ///< @brief The queue of buffers.
};

}  // anonymous namespace


/// @brief The main logger class.
/// @copyright Derived from `NanoLogger` from NanoLog.
class Logger final {
public:
	/// @brief Create a new logger connected to a writer.
	/// @param logWriter The log writer.
	/// @copyright Derived from `NanoLogger::NanoLogger` from NanoLog.
	explicit Logger(std::unique_ptr<LogWriter>&& logWriter)
		: m_thread(&Logger::Pop, this) {
		m_logWriters.push_back(std::move(logWriter));
		m_state.store(State::kReady, std::memory_order_release);
	}

	Logger(const Logger&) = delete;  ///< @nocopyconstructor
	Logger(Logger&&) = delete;       ///< @nomoveconstructor

	/// @brief Waits for all entries to be written.
	/// @copyright Derived from `NanoLogger::~NanoLogger` from NanoLog.
	~Logger() {
		m_state.store(State::kShutdown);
		m_thread.join();
	}

public:
	Logger& operator=(const Logger&) = delete;  ///< @noassignmentoperator
	Logger& operator=(Logger&&) = delete;       ///< @nomoveoperator

public:
	/// @brief Adds a new `LogWriter`.
	/// @param logWriter The `LogWriter`.
	void AddWriter(std::unique_ptr<LogWriter>&& logWriter) {
		m_logWriters.push_back(std::move(logWriter));
	}

	/// @brief Adds a new `LogLine`.
	/// @param logLine The `LogLine`.
	/// @copyright Same as `NanoLogger::add` from NanoLog.
	void AddLine(LogLine&& logLine) {
		m_buffer.Push(std::move(logLine));
	}

private:
	/// @brief Main method of the writing thread.
	/// @copyright Same as `NanoLogger::pop` from NanoLog.
	void Pop() noexcept {
		// Wait for constructor to complete and pull all stores done there to this thread / core.
		while (m_state.load(std::memory_order_acquire) == State::kInit) {
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}

		LogLine logLine(LogLevel::kInfo, nullptr, nullptr, 0, "");

		while (m_state.load() == State::kReady) {
			if (m_buffer.TryPop(logLine)) {
				const LogLevel level = logLine.GetLevel();
				for (std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
					if (logWriter->IsLogged(level)) {
						logWriter->Log(logLine);
					}
				}
			} else {
				std::this_thread::sleep_for(std::chrono::microseconds(50));
			}
		}

		// Pop and log all remaining entries
		while (m_buffer.TryPop(logLine)) {
			const LogLevel level = logLine.GetLevel();
			for (std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
				if (logWriter->IsLogged(level)) {
					logWriter->Log(logLine);
				}
			}
		}
	}

private:
	/// @brief Internal logger state.
	/// @copyright Same as `NanoLogger::State` from NanoLog.
	enum class State : uint_fast8_t {
		kInit,
		kReady,
		kShutdown
	};

	/**
	
	*/
	/// @copyright Same as `NanoLogger::m_state` from NanoLog.
	std::atomic<State> m_state = State::kInit;  ///< @brief The current state of the logger.

	/// @copyright Same as `NanoLogger::m_buffer_base` from NanoLog but on stack insteaf of heap.
	QueueBuffer m_buffer;  ///< @brief The buffer.

	/// @copyright Same as `NanoLogger::m_thread` from NanoLog.
	std::thread m_thread;  ///< @brief The thread writing the events.

	/// @copyright Similar to `NanoLogger::m_file_writer` from NanoLog.
	std::vector<std::unique_ptr<LogWriter>> m_logWriters;  ///< @brief A list of all log writers.
};

static std::unique_ptr<Logger> g_logger;
static std::atomic<Logger*> g_atomicLogger;


Log& Log::operator+=(std::unique_ptr<LogWriter>&& logWriter) {
	g_atomicLogger.load(std::memory_order_acquire)->AddWriter(std::move(logWriter));
	return *this;
}

// Derived from `Log::operator==` from NanoLog.
void Log::operator+=(LogLine& logLine) {
	g_atomicLogger.load(std::memory_order_acquire)->AddLine(std::move(logLine));
}

// Derived from `Log::operator==` from NanoLog.
void Log::operator+=(LogLine&& logLine) {
	g_atomicLogger.load(std::memory_order_acquire)->AddLine(std::move(logLine));
}

// Derived from `initialize` from NanoLog.
void Initialize(std::unique_ptr<LogWriter>&& logWriter) {
	g_logger = std::make_unique<Logger>(std::move(logWriter));
	g_atomicLogger.store(g_logger.get(), std::memory_order_seq_cst);
}

// Derived from `initialize` from NanoLog.
void Initialize() {
	//Initialize(std::unique_ptr<LogWriter>(new DailyRollingFileWriter(LogLevel::Trace, "t:\\tmp\\", "llamalog.log")));
	Initialize(std::unique_ptr<LogWriter>(new DebugWriter(LogLevel::kTrace)));
}
}  // namespace llamalog
