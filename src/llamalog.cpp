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

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace llamalog {

namespace {

/// @brief Checks if the `Priority` is a priority from an internal message.
/// @details Used internally to prevent endless loops from logging errors that happen during logging of errors.
/// @param priority The `Priority` to check.
/// @return `true` if this is an internal `Priority`.
constexpr bool IsInternal(const Priority priority) noexcept {
	return static_cast<uint8_t>(priority) & 1u;
}

/// @brief Checks if the `Priority` is a priority from an internal exception message.
/// @details Used internally to prevent endless loops from logging errors that happen during logging of errors.
/// @param priority The `Priority` to check.
/// @return `true` if this is an exception priority.
constexpr bool IsException(const Priority priority) noexcept {
	return static_cast<uint8_t>(priority) & 2u;
}

/// @brief Gets the priority for an internal error message.
/// @details If the original `LogLine` which caused an error is a regular message, an internal priority is used.
/// If the `Priority` is already internal, an elevated level is returned. If the log is already from an elevated level,
/// `Priority::kNone` is returned to prevent endless loops from logging errors that happen during logging of errors.
/// @param priority The `Priority` to check.
/// @return The priority for logging an error message for logging at this priority.
constexpr Priority GetErrorPriority(const Priority priority) noexcept {
	if (IsInternal(priority)) {
		return static_cast<llamalog::Priority>(static_cast<std::uint8_t>(Priority::kError) | 2u);
	}
	if (!IsException(priority)) {
		return static_cast<llamalog::Priority>(static_cast<std::uint8_t>(Priority::kError) | 1u);
	}
	return Priority::kNone;
}

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

	static_assert(sizeof(Item) == sizeof(LogLine), "size of Buffer::Item is not equal to size of LogLine");
	static_assert((sizeof(Item) & (sizeof(Item) - 1)) == 0, "size of Buffer::Item is not a power of 2");

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
	/// @remarks The function uses a placement `new` for creating the `LogLine`.
	/// @warning This function MUST NOT be called more than once for each index because the data is moved internally.
	/// @param readIndex The index to read.
	/// @param pLogLine A pointer to the adress where the `LogLine` receiving the next event shall be created.
	/// @return Returns `true` if a value is available and has been created in @p pLogLine.
	/// @copyright Same as `Buffer::try_pop` from NanoLog.
	bool TryPop(const std::uint_fast32_t readIndex, LogLine* const pLogLine) noexcept {
		if (m_writeState[readIndex].load(std::memory_order_acquire)) {
			Item& item = reinterpret_cast<Item*>(m_buffer)[readIndex];
			new (pLogLine) LogLine(std::move(item.logLine));
			return true;
		}
		return false;
	}

public:
	///< @brief The number of elements in the buffer.
	/// @details The size of `m_buffer` is just under 8 MB (to account for the two atomic fields.
	/// @copyright Same as `Buffer::size` from NanoLog.
	static constexpr std::uint_fast32_t kBufferSize =
		(8388608 - sizeof(std::atomic_uint_fast32_t) - sizeof(std::atomic_bool)) / sizeof(Item);

private:
	/// @copyright Same as `Buffer::m_buffer` from NanoLog, but on stack instead of heap.
	std::byte m_buffer[kBufferSize * sizeof(Item)];  ///< @brief The buffer holding the log events.
	std::atomic_uint_fast32_t m_remaining;           ///< The number of free entries in this buffer.

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
				m_buffers.pop_front();  // may throw, will crash - nothing we could do anyways
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
				goto wait;
			}
			{
				SpinLock spinLock(m_flag);
				for (const std::unique_ptr<Buffer>& ptr : m_buffers) {
					if (ptr.get() == writeBuffer) {
						goto wait;
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
		return m_buffers.empty() ? nullptr : m_buffers.front().get();  // might throw, will crash, nothing we could do anyways
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

}  // namespace


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
			LOG_WARN_INTERNAL("Error configuring thread: {}", GetLastError());
		}
		MEMORY_PRIORITY_INFORMATION mpi = {0};
		mpi.MemoryPriority = MEMORY_PRIORITY_LOW;
		if (!SetThreadInformation(m_thread.native_handle(), ThreadMemoryPriority, &mpi, sizeof(mpi))) {
			LOG_WARN_INTERNAL("Error configuring thread: {}", GetLastError());
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
			SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, 5000, CONDITION_VARIABLE_LOCKMODE_SHARED);
		}
		m_buffer.Flush([this]() noexcept {
			ReleaseSRWLockShared(&m_lock);
			// do not use SleepConditionVariableSRW so that regular AddLine does not have to do a WakeAllConditionVariable
			Sleep(200);
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
			SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, 5000, 0);
		}

		// the place where the logline is copied to
		std::byte buffer[sizeof(LogLine)];
		LogLine* const pLogLine = reinterpret_cast<LogLine*>(buffer);

		// Helper class to enable destructor call during stack unwinding for pLogLine.
		class CallDestruct {
		public:
			constexpr explicit CallDestruct(LogLine* const pLogLine) noexcept
				: m_pLogLine(pLogLine) {
				// empty
			}
			CallDestruct(const CallDestruct&) = delete;
			CallDestruct(CallDestruct&&) = delete;
			~CallDestruct() noexcept {
				m_pLogLine->~LogLine();
			}

		public:
			CallDestruct& operator=(const CallDestruct&) = delete;
			CallDestruct& operator=(CallDestruct&&) = delete;

		private:
			LogLine* const m_pLogLine;
		};

		while (m_state.load() == State::kReady) {
			if (m_buffer.TryPop(pLogLine)) {
				// release any resources of the log line as quickly as possible
				CallDestruct guard(pLogLine);

				const Priority priority = pLogLine->GetPriority();
				for (const std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
					if (logWriter->IsLogged(priority)) {
						try {
							logWriter->Log(*pLogLine);
						} catch (const std::exception& e) {
							const Priority errorPriority = GetErrorPriority(priority);
							if (errorPriority != Priority::kNone) {
								try {
									LLAMALOG_LOG(errorPriority, "Error writing log: {}", e);
								} catch (...) {
									try {
										LLAMALOG_LOG(errorPriority, "Error writing log");
									} catch (...) {
										LLAMALOG_PANIC("Error writing log");
									}
								}
							} else {
								LLAMALOG_PANIC(e.what());
							}
						} catch (...) {
							const Priority errorPriority = GetErrorPriority(priority);
							if (errorPriority != Priority::kNone) {
								try {
									LLAMALOG_LOG(errorPriority, "Error writing log");
								} catch (...) {
									LLAMALOG_PANIC("Error writing log");
								}
							} else {
								LLAMALOG_PANIC("Error writing log");
							}
						}
					}
				}
			} else {
				SleepConditionVariableSRW(&m_wakeConsumer, &m_lock, 5000, 0);
			}
		}

		// Pop and log all remaining entries
		while (m_buffer.TryPop(pLogLine)) {
			// release any resources of the log line as quickly as possible
			CallDestruct guard(pLogLine);

			const Priority priority = pLogLine->GetPriority();
			for (const std::unique_ptr<LogWriter>& logWriter : m_logWriters) {
				if (logWriter->IsLogged(priority)) {
					try {
						logWriter->Log(*pLogLine);
					} catch (const std::exception& e) {
						const Priority errorPriority = GetErrorPriority(priority);
						if (errorPriority != Priority::kNone) {
							try {
								LLAMALOG_LOG(errorPriority, "Error writing log: {}", e);
							} catch (...) {
								try {
									LLAMALOG_LOG(errorPriority, "Error writing log");
								} catch (...) {
									LLAMALOG_PANIC(e.what());
								}
							}
						} else {
							LLAMALOG_PANIC(e.what());
						}
					} catch (...) {
						const Priority errorPriority = GetErrorPriority(priority);
						if (errorPriority != Priority::kNone) {
							try {
								LLAMALOG_LOG(errorPriority, "Error writing log");
							} catch (...) {
								LLAMALOG_PANIC("Error writing log");
							}
						} else {
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
static std::unique_ptr<Logger> g_pLogger;
/// @brief An atomic reference to the default logger.
static std::atomic<Logger*> g_pAtomicLogger;

namespace internal {

// Derived from `initialize` from NanoLog.
void Initialize() {
	g_pLogger = std::make_unique<Logger>();
	g_pAtomicLogger.store(g_pLogger.get(), std::memory_order_release);
}

void Start() {
	std::atomic_thread_fence(std::memory_order_release);
	g_pAtomicLogger.load(std::memory_order_acquire)->Start();
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

_Ret_maybenull_ const BaseException* GetCurrentExceptionAsBaseException() noexcept {
	try {
		throw;
	} catch (const BaseException& e) {
		return &e;
	} catch (...) {
		return nullptr;
	}
}

}  // namespace llamalog
