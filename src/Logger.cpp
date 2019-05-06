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

#include "llamalog/Logger.h"

#include "llamalog/LogLine.h"
#include "llamalog/LogWriter.h"
#include "llamalog/finally.h"
#include "llamalog/winapi_log.h"

#include <windows.h>

#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <exception>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace llamalog {

namespace {

/// @brief Variable to prevent endless loops caused by internal errors while logging internal errors.
std::atomic_uint8_t g_currentPriority;

/// @brief A lock free buffer supporting parallel readers and writers.
/// @copyright Derived from `class Buffer` from NanoLog.
class Buffer final {
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
			reinterpret_cast<LogLine*>(m_buffer)[i].~LogLine();
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
		LogLine* pLogLine = new (&reinterpret_cast<LogLine*>(m_buffer)[writeIndex]) LogLine(std::move(logLine));
		pLogLine->GenerateTimestamp();
		m_writeState[writeIndex].store(true, std::memory_order_release);
		return m_remaining.fetch_sub(1, std::memory_order_acquire) == 1;
	}

	/// @brief Gets an item from this buffer.
	/// @remarks The function uses a placement `new` for creating the `LogLine`.
	/// @warning This function MUST NOT be called more than once for each index because the data is moved internally.
	/// @param readIndex The index to read.
	/// @param pLogLine A pointer to the adress where the `LogLine` receiving the next event shall be created.
	/// @return Returns `true` if a value is available and has been created in @p pLogLine.
	/// @copyright Same as `Buffer::try_pop` from NanoLog.
	bool TryPop(const std::uint_fast32_t readIndex, LogLine* const pLogLine) noexcept {
		if (m_writeState[readIndex].load(std::memory_order_acquire)) {
			LogLine& logLine = reinterpret_cast<LogLine*>(m_buffer)[readIndex];
			new (pLogLine) LogLine(std::move(logLine));
			return true;
		}
		return false;
	}

public:
	///< @brief The number of elements in the buffer.
	/// @details The size of `m_buffer` is just under 8 MB (to account for the two atomic fields.
	/// @copyright Same as `Buffer::size` from NanoLog.
	static constexpr std::uint_fast32_t kBufferSize =
		(8388608 - sizeof(std::atomic_uint_fast32_t) - sizeof(std::atomic_bool)) / sizeof(LogLine);

private:
	static_assert((sizeof(LogLine) & (sizeof(LogLine) - 1)) == 0, "size of LogLine is not a power of 2");
	/// @copyright Same as `Buffer::m_buffer` from NanoLog, but on stack instead of heap.
	std::byte m_buffer[kBufferSize * sizeof(LogLine)];  ///< @brief The buffer holding the log events.
	std::atomic_uint_fast32_t m_remaining;              ///< The number of free entries in this buffer.

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
	_Acquires_exclusive_lock_(m_flag) explicit SpinLock(std::atomic_flag& flag) noexcept
		: m_flag(flag) {
		while (m_flag.test_and_set(std::memory_order_acquire)) {
			// empty
		}
	}
	SpinLock(const SpinLock&) = delete;  ///< @nocopyconstructor
	SpinLock(SpinLock&&) = delete;       ///< @nomoveconstructor

	/// @brief Release the spin lock.
	/// @copyright Same as `SpinLock::~SpinLock` from NanoLog.
	_Releases_exclusive_lock_(m_flag) ~SpinLock() noexcept {
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
	/// @param pLogLine A pointer to the adress where the `LogLine` receiving the next event shall be created.
	/// @return `true` if data is available and a new `LogLine` has been created.
	/// @copyright Same as `QueueBuffer::try_pop` from NanoLog.
	bool TryPop(LogLine* const pLogLine) noexcept {
		if (!m_currentReadBuffer) {
			m_currentReadBuffer = GetNextReadBuffer();
		}

		Buffer* const readBuffer = m_currentReadBuffer;

		if (!readBuffer) {
			return false;
		}

		if (readBuffer->TryPop(m_readIndex, pLogLine)) {
			++m_readIndex;
			if (m_readIndex == Buffer::kBufferSize) {
				m_readIndex = 0;
				m_currentReadBuffer = nullptr;

				SpinLock spinLock(m_flag);
				m_buffers.pop_front();  // may throw, will crash - nothing we could do anyway
			}
			return true;
		}

		return false;
	}

	/// @brief Waits until all currently available entries have been written.
	/// @param wait A lambda expression called when waiting is required.
	template <typename W>
	void Flush(W&& wait) {
		const Buffer* writeBuffer;
		std::uint_fast32_t writeIndex;
		while (true) {
			writeBuffer = m_currentWriteBuffer.load(std::memory_order_acquire);
			writeIndex = m_writeIndex.load(std::memory_order_acquire);
			// check if still the same
			if (m_currentWriteBuffer.load(std::memory_order_acquire) == writeBuffer && writeIndex < Buffer::kBufferSize) {
				break;
			}
		}
		while (true) {
			if (!m_currentReadBuffer) {
				return;
			}
			if (m_currentReadBuffer == writeBuffer) {
				if (writeIndex <= m_readIndex) {
					return;
				}
				goto wait;  // NOLINT(cppcoreguidelines-avoid-goto): I DO want a goto here!
			}
			{
				SpinLock spinLock(m_flag);
				for (const std::unique_ptr<Buffer>& ptr : m_buffers) {
					if (ptr.get() == writeBuffer) {
						goto wait;  // NOLINT(cppcoreguidelines-avoid-goto): I DO want a goto here!
					}
				}
				// write buffer is no longer queued
				return;
			}
		wait:
			wait();
		}
	}

private:
	/// @brief Create a new `Buffer` for writing.
	/// @copyright Same as `QueueBuffer::setup_next_write_buffer` from NanoLog.
	void SetupNextWriteBuffer() {
		std::unique_ptr<Buffer> nextWriteBuffer = std::make_unique<Buffer>();
		m_currentWriteBuffer.store(nextWriteBuffer.get(), std::memory_order_release);

		SpinLock spinLock(m_flag);
		m_buffers.push_back(std::move(nextWriteBuffer));
		m_writeIndex.store(0, std::memory_order_relaxed);
	}

	/// @brief Get next `Buffer` for reading.
	/// @return The next `Buffer` or `nullptr` if none is currently available.
	/// @copyright Same as `QueueBuffer::get_next_read_buffer` from NanoLog.
	Buffer* GetNextReadBuffer() noexcept {
		SpinLock spinLock(m_flag);
		return m_buffers.empty() ? nullptr : m_buffers.front().get();  // might throw, will crash, nothing we could do anyway
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
	_Guarded_by_(m_flag) std::deque<std::unique_ptr<Buffer>> m_buffers;  ///< @brief The queue of buffers.
};


/// @brief The main logger class.
/// @copyright Derived from `NanoLogger` from NanoLog.
class Logger final {
public:
	/// @brief Create a new logger connected to a writer.
	/// @copyright Derived from `NanoLogger::NanoLogger` from NanoLog.
	explicit Logger()
		: m_thread(&Logger::Pop, this) {
		// empty
	}

	Logger(const Logger&) = delete;  ///< @nocopyconstructor
	Logger(Logger&&) = delete;       ///< @nomoveconstructor

	/// @brief Waits for all entries to be written.
	/// @copyright Derived from `NanoLogger::~NanoLogger` from NanoLog.
	~Logger() noexcept {
		m_state.store(State::kShutdown);
		WakeConditionVariable(&m_wakeConsumer);
		try {
			m_thread.join();
		} catch (const std::exception& e) {
			LLAMALOG_PANIC(e.what());
		} catch (...) {
			LLAMALOG_PANIC("Error during shutdown");
		}
	}

public:
	Logger& operator=(const Logger&) = delete;  ///< @noassignmentoperator
	Logger& operator=(Logger&&) = delete;       ///< @nomoveoperator

public:
	/// @brief Start logging.
	/// @details Moved from the constructor to a separate function to allow writers to be set before logging starts.
	void Start() {
		if (!SetThreadPriority(m_thread.native_handle(), THREAD_PRIORITY_BELOW_NORMAL)) {
			LLAMALOG_INTERNAL_WARN("Error configuring thread: {}", LastError());
		}
		MEMORY_PRIORITY_INFORMATION mpi = {0};
		mpi.MemoryPriority = MEMORY_PRIORITY_LOW;
		if (!SetThreadInformation(m_thread.native_handle(), ThreadMemoryPriority, &mpi, sizeof(mpi))) {
			LLAMALOG_INTERNAL_WARN("Error configuring thread: {}", LastError());
		}

		m_state.store(State::kReady, std::memory_order_release);
		WakeAllConditionVariable(&m_wakeConsumer);
	}

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
		WakeConditionVariable(&m_wakeConsumer);
	}

	/// @brief Waits until all currently available entries have been written.
	void Flush() {
		AcquireSRWLockShared(&m_lock);
		while (m_state.load(std::memory_order_acquire) == State::kInit) {
			SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, kConditionInterval, CONDITION_VARIABLE_LOCKMODE_SHARED);
		}
		m_buffer.Flush([this]() noexcept {
			ReleaseSRWLockShared(&m_lock);
			// do not use SleepConditionVariableSRW so that regular `AddLine` does not have to do a WakeAllConditionVariable
			Sleep(kFlushInterval);
			AcquireSRWLockShared(&m_lock);
		});
		ReleaseSRWLockShared(&m_lock);
	}

private:
	/// @brief Main method of the writing thread.
	/// @copyright Same as `NanoLogger::pop` from NanoLog.
	void Pop() noexcept {
		AcquireSRWLockExclusive(&m_lock);
		while (m_state.load(std::memory_order_acquire) == State::kInit) {
			SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, kConditionInterval, 0);
		}

		// the place where the logline is copied to
		std::byte buffer[sizeof(LogLine)];
		LogLine* const pLogLine = reinterpret_cast<LogLine*>(buffer);

		while (m_state.load() == State::kReady) {
			if (m_buffer.TryPop(pLogLine)) {
				// release any resources of the log line as quickly as possible
				auto finally = llamalog::finally([pLogLine]() noexcept {
					pLogLine->~LogLine();
				});

				const Priority priority = pLogLine->GetPriority();
				g_currentPriority.store(static_cast<std::uint8_t>(priority), std::memory_order_release);
				for (const std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
					if (logWriter->IsLogged(priority)) {
						try {
							logWriter->Log(*pLogLine);
						} catch (const std::exception& e) {
							try {
								LLAMALOG_INTERNAL_ERROR("Error writing log: {}", e);
							} catch (...) {
								LLAMALOG_PANIC(e.what());
							}
						} catch (...) {
							try {
								LLAMALOG_INTERNAL_ERROR("Error writing log");
							} catch (...) {
								LLAMALOG_PANIC("Error writing log");
							}
						}
					}
				}
			} else {
				SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, kConditionInterval, 0);
			}
		}

		// pop and log all remaining entries
		while (m_buffer.TryPop(pLogLine)) {
			// release any resources of the log line as quickly as possible
			auto finally = llamalog::finally([pLogLine]() noexcept {
				pLogLine->~LogLine();
			});

			const Priority priority = pLogLine->GetPriority();
			g_currentPriority.store(static_cast<std::uint8_t>(priority), std::memory_order_release);
			for (const std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
				if (logWriter->IsLogged(priority)) {
					try {
						logWriter->Log(*pLogLine);
					} catch (const std::exception& e) {
						try {
							LLAMALOG_INTERNAL_ERROR("Error writing log: {}", e);
						} catch (...) {
							LLAMALOG_PANIC(e.what());
						}
					} catch (...) {
						try {
							LLAMALOG_INTERNAL_ERROR("Error writing log");
						} catch (...) {
							LLAMALOG_PANIC("Error writing log");
						}
					}
				}
			}
		}

		ReleaseSRWLockExclusive(&m_lock);
	}

private:
	/// @brief Internal logger state.
	/// @copyright Same as `NanoLogger::State` from NanoLog.
	enum class State : uint_fast8_t {
		kInit,
		kReady,
		kShutdown
	};

	static constexpr DWORD kConditionInterval = 5000u;  ///< @brief Milliseconds to wait on condition before wake-up.
	static constexpr DWORD kFlushInterval = 200u;       ///< @brief Milliseconds to wait when lock is held in flush.

	/**

	*/
	/// @copyright Same as `NanoLogger::m_state` from NanoLog.
	std::atomic<State> m_state = State::kInit;  ///< @brief The current state of the logger.

	/// @copyright Same as `NanoLogger::m_buffer_base` from NanoLog but on stack insteaf of heap.
	QueueBuffer m_buffer;  ///< @brief The buffer.

	/// @brief A condition to trigger the worker thread. @hideinitializer
	/// @note This MUST be declared before `m_thread` because the latter waits on this condition.
	CONDITION_VARIABLE m_wakeConsumer = CONDITION_VARIABLE_INIT;

	SRWLOCK m_lock = SRWLOCK_INIT;  ///< @brief A lock held while events are being processed.

	/// @copyright Same as `NanoLogger::m_thread` from NanoLog.
	std::thread m_thread;  ///< @brief The thread writing the events.

	/// @copyright Similar to `NanoLogger::m_file_writer` from NanoLog.
	std::vector<std::unique_ptr<LogWriter>> m_logWriters;  ///< @brief A list of all log writers.
};

/// @brief The default logger.
std::unique_ptr<Logger> g_pLogger;
/// @brief An atomic reference to the default logger.
std::atomic<Logger*> g_pAtomicLogger;

}  // namespace

namespace internal {

// Derived from `Initialize` from NanoLog.
void Initialize() {
	g_pLogger = std::make_unique<Logger>();
	g_pAtomicLogger.store(g_pLogger.get(), std::memory_order_release);
}

void Start() {
	std::atomic_thread_fence(std::memory_order_release);
	g_pAtomicLogger.load(std::memory_order_acquire)->Start();
}

Priority GetInternalPriority(const Priority priority) noexcept {
	return static_cast<Priority>(static_cast<std::uint8_t>(priority) | ((g_currentPriority.load(std::memory_order_acquire) & 3u) + 1));
}

void CallNoExcept(const char* __restrict const file, const std::uint32_t line, const char* __restrict const function, void (*const thunk)(_In_z_ const char* __restrict, std::uint32_t, _In_z_ const char* __restrict, _In_ void*), _In_ void* const log) noexcept {
	try {
		try {
			thunk(file, line, function, log);
		} catch (std::exception& e) {
			llamalog::Log(Priority::kError, file, line, function, "Error logging: {}", e);
		} catch (...) {
			llamalog::Log(Priority::kError, file, line, function, "Error logging");
		}
	} catch (...) {
		internal::Panic(file, line, function, "Error logging");
	}
}

void Panic(const char* const file, const std::uint32_t line, const char* const function, const char* const message) noexcept {
	// avoid anything that could cause an error
	char msg[1024];                                                                               // NOLINT(readability-magic-numbers)
	if (sprintf_s(msg, "PANIC: %s @ %s(%s:%" PRIu32 ")\n", message, function, file, line) < 0) {  // NOLINT(cppcoreguidelines-pro-type-vararg): sprintf_s as a last resort
		OutputDebugStringA("PANIC: Error writing log\n");
	} else {
		OutputDebugStringA(msg);
	}
}

}  // namespace internal

void AddWriter(std::unique_ptr<LogWriter>&& writer) {
	g_pAtomicLogger.load(std::memory_order_acquire)->AddWriter(std::move(writer));
}

// Derived from `Log::operator==` from NanoLog.
void Log(LogLine& logLine) {
	g_pAtomicLogger.load(std::memory_order_acquire)->AddLine(std::move(logLine));
}

// Derived from `Log::operator==` from NanoLog.
void Log(LogLine&& logLine) {
	g_pAtomicLogger.load(std::memory_order_acquire)->AddLine(std::move(logLine));
}

void Flush() {
	g_pAtomicLogger.load(std::memory_order_acquire)->Flush();
}

void Shutdown() noexcept {
	// first delete the logger, the the reference. This allows the logger to log messages during shutdown
	g_pLogger.reset();
	g_pAtomicLogger.store(nullptr);
}

}  // namespace llamalog
