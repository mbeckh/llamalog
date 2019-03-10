# llamalog
![llamalog Logo](/img/logo_150x150.png) A **l**ightweight **l**ean **a**nd **m**ean **a**synchronous **log**ger for C++.

## Feature Highlights
llamalog is basically [NanoLog](https://github.com/Iyengar111/NanoLog) reloaded. It **keeps** the following highlights:
- All formatting (including any number or object to string conversions) is done in an asynchronous background thread.
- Log messages of 256 characters or less do not require any heap allocations.
- Minimalistic header includes to avoid common pattern of (template-heavy) header only library. This helps to keep compilation times of projects low. (BTW: This really is what drew me to NanoLog in the first place).

The **main differences** are:
- llamalog uses [{fmt}](https://github.com/fmtlib/fmt) for formatting. All formatting is still done asynchronously in a background thread.
- Support for formatting custom types (as long as they have a copy constructor). For better efficiency, the logger uses a move constructor if available.
- Support for wide character strings which are very common in the Windows API. The output is encoded as UTF-8.
- A time-based rolling file sink.
- My focus is on Windows desktop development where heap size is not an issue. So I removed the non-guaranteed logger.
- Shameless use of the Windows API where it provides better performance than the STL.
- I had to drop the zero-copying for string literals - however it does not seem to hurt performance too badly.

In the course of all the rewriting, I also made some performance improvements, e.g. removing virtual functions because there remains only one type of logger, replacing heap-allocated data with stack-based structures and doing some more "dirty" stuff.

## Performance
It's *fast*. Detailed benchmarks to come.

## Usage
### Basic Example
```cpp
#include <llamalog/llamalog.h>
#include <llamalog/LogWriter.h>
#include <memory>

int main(int argc, char* argv[]) {
    std::unique_ptr<llamalog::RollingFileWriter> writer = 
      std::make_unique<llamalog::RollingFileWriter>(
        // the writer only logs events of this level and above
        llamalog::LogLevel::kTrace,
        // directory for the log files
        "L:\\logs",
        // name of the log file; writer will add date, e.g. llamalog.20190331.log
        "llamalog.log",
        // how often to start a new file
        llamalog::RollingFileWriter::Frequency::kDaily,
        // how many old log files to keep around
        7);
    llamalog::Initialize(std::move(writer));
    LOG_INFO("Program {} called with {} arguments.", argv[0], argc);
}
```
This produces the following log line, given the code is stored as `main.cpp` and compiled to `L:\\llamalog.exe` (`9234` is a sample thread id):
```
2019-03-31 15:27:12.283 INFO [9234] main.cpp:19 main Program L:\\llamalog.exe called with 1 arguments.
```

## History
While writing a Windows desktop application I wanted to do some logging from a COM DLL. Experimenting with the usual logging macros which write to a file etc. did just not feel right. Also doing a lot of work in Java, I was used to highly configurable and fast loggers. There are many logging frameworks for C++, but either they required a config file (which I did not want to ship with just a small DLL), they were code-heavy or they required including tons of headers adding to the time needed for compilation.

After a little research I found the marvelous [NanoLog](https://github.com/Iyengar111/NanoLog) NanoLog written by Karthik Iyengar. The library is small, fast and did nearly everything I wanted. Benchmarks are well documented. So - perfect? Not quite, especially the formatting part did not make me really happy. Using the << operator for adding log arguments and having to interleave arguments and message text (e.g. `LOG_INFO("There are ") << count << " items."`) just felt awkward.

So I started to add some features, change a little bit here, add a little bit there, refactor just slightly... And after some time coding, here we are now.

## Where does the name come from?
llamalog stands for (l)ightweight (l)ean (a)nd (m)ean (a)synchronous (log)ger for C++. As a plus, there are not yet so many hits in Google. And of course because llamas are cute and everybody likes them. :wink:
