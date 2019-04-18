# llamalog
![llamalog Logo](/img/logo_150x150.png) A **l**ightweight **l**ean **a**nd **m**ean **a**synchronous **log**ger for C++.

## Feature Highlights
llamalog is basically [NanoLog](https://github.com/Iyengar111/NanoLog) reloaded. It **keeps** the following highlights:
- All formatting (including any number or object to string conversions) is done in an asynchronous background thread.
- Log messages of 256 characters or less do not require any heap allocations.
- Minimalistic header includes to avoid common pattern of (template-heavy) header only library. This helps to keep compilation times of projects low. (BTW: This really is what drew me to NanoLog in the first place).

The **main differences** are:
- llamalog uses [{fmt}](https://github.com/fmtlib/fmt) for formatting. All formatting is still done asynchronously in a background thread.
- The background thread does not perform near-busy waits in 50 microsecond intervals but instead is notified using conditions.
- Support for formatting custom types (as long as they have a copy constructor). For better efficiency, the logger uses a move constructor if available.
- Support for wide character strings which are very common in the Windows API. The output is encoded as UTF-8.
- A time-based rolling file sink.
- My focus is on Windows desktop development where heap size is not an issue. So I removed the non-guaranteed logger.
- Shameless use of the Windows API where it provides better performance than the STL.
- I had to drop the zero-copying for string literals - however it does not seem to hurt performance too badly.

In the course of all the rewriting, I also made some performance improvements, e.g. removing virtual functions because there remains only one type of logger, replacing heap-allocated data with stack-based structures and doing some more "dirty" stuff.

## Performance
The following benchmarks were compiled using Microsoft Visual Studio 15.9.8 (Microsoft C/C++ Compiler version 19.16.27027.1) as x64 binaries and executed using Windows 10 on Intel(R) Core(TM) i7-3770 CPU @ 3.40GHz/3.90 GHz. Data was written to a HDD.

The benchmark compares llamalog to its ancester [NanoLog](https://github.com/Iyengar111/NanoLog), to [spdlog](https://github.com/gabime/spdlog) which claims to be very fast and also uses {fmt} and to [g3log](https://github.com/KjellKod/g3log) which has also been very thoroughly tested for performance.

The following versions were used for the benchmark.

|Project|Commit|
|---|---|
|fmt|287eaab3b2777daa5d0d0cf72d977196ba54efb7|
|g3log|cb4a94da7ddaec239d6cd95d0dafa0545b3c7c47|
|llamalog|102713b0d2c9548d20d8c07ee69aa87cc4bbb80d|
|NanoLog|40a53c36e0336af45f7664abeb939f220f78273e|
|spdlog|bdfc7d2a5a4ad9cc1cebe1feb7e6fcc703840d71|
|msvc-common|12bcbf336e284294498298b0e7214d9ae28b58aa|

The following tables show the average of three runs for each logger at each setting. Lower numbers are always better. Precentiles, worst and average duration for a logger call are given in microseconds. The total duration, which also includes writing all data to the log file, is stated as seconds.

### 1 Thread - 1,000,000 Messages
llamalog is slightly faster than NanoLog when logging and more than twice as fast when writing which can be accounted to replacing the C++ stream API with native Windows `WriteFile` calls.

spdlog is slower on average but gives a very consistent behavior with a very low worst case latency. spdlog is also blazing fast when writing mainly because it uses `fwrite` internally which does very heavy buffering.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.1|0.1|0.2|1.4|3.0|194.2|0.20132|7.64794|
|NanoLog|0.2|0.2|0.2|1.4|3.2|224.9|0.24535|16.20528|
|spdlog|0.6|0.7|0.8|3.2|10.5|77.8|0.73845|0.87369|
|g3log|2.8|2.9|3.7|7.9|13.9|415.7|3.08563|12.41233|

### 2 Threads - 500,000 Messages Each (= 1,000,000 Messages in Total)
The performance hit for both llamalog and NanoLog is higher (nearly twice the latency of the single threaded test) than for spdlog and g3log. However, llamalog and NanoLog deliver a better overall performance.

spdlog is still writing very fast, but the worst case latency does not keep up with the single threaded test. g3log - as advertised - keeps a very consistent timing and hardly suffers any latency hit at all.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.2|0.2|1.4|2.1|4.3|178.9|0.38859|7.64205|
|NanoLog|0.2|0.3|1.4|2.1|3.8|248.9|0.39784|16.22859|
|spdlog|0.7|0.8|1.4|5.2|16.1|455.5|0.96818|0.91599|
|g3log|2.9|3.1|4.4|7.5|11.8|558.1|3.17071|12.60405|

### 4 Threads - 250,000 Messages Each (= 1,000,000 Messages in Total)
The trend continues with llamalog and NanoLog again nearly doubling the latency. The same holds true for spdlog and even g3log now takes a little performance hit but still a lot lower than the other three on average. However, worst case latency severly suffers for g3log.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.3|0.5|2.0|3.3|4.6|448.9|0.67265|7.66290|
|NanoLog|0.3|0.6|2.0|3.2|4.6|559.7|0.70082|16.17560|
|spdlog|1.1|1.6|3.0|12.0|68.8|862.8|1.79919|1.00225|
|g3log|4.7|5.0|6.0|10.5|87.1|2,368.7|4.75029|12.55430|

### 10 Threads - 100,000 Messages Each (= 1,000,000 Messages in Total)
At ten threads logging in parallel, timings for all loggers skyrocket. g3log and spdlog still lead the pack when relative numbers are compared. Both have average latencies of twice the results for 4 threds whereas timings for llamalog and NanoLog rise by a factor of five. spdlog shines with a worst case latency in a league of its own. This is also the first test where NanoLog clearly outperforms llamalog.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.3|2.7|3.2|4.7|9.0|40,275.1|3.82976|7.99707|
|NanoLog|0.3|2.5|3.1|4.6|6.8|40,967.5|3.34428|16.36179|
|spdlog|1.2|1.9|3.5|23.6|221.1|27,640.3|3.55314|1.04362|
|g3log|4.9|5.5|7.3|12.1|162.3|48,077.1|8.66527|12.85125|

### 1 Thread - 1,000,000 Messages - Log Message Longer than 500 Bytes
The following two benchmarks used data for log message longer than 500 bytes to examine the effect when the internal on-stack buffers are no longer sufficient.

Compared with the single thread test all loggers take a performance hit, yet g3log again showing the smallest relative change. The total time for NanoLog nearly doubles which shows why the C++ stream API is not well suited for performance critical applications. spdlog handles this test remarkably well when the 99th and 99.9th percentiles are considered.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.5|0.6|0.7|2.8|100.8|253.9|0.97674|9.16119|
|NanoLog|0.6|0.7|0.8|3.2|103.9|541.3|1.18773|36.64014|
|spdlog|1.0|1.1|1.2|3.8|92.7|447.8|1.27233|2.66903|
|g3log|3.7|3.8|4.1|11.3|17.1|267.6|3.95533|13.51272|

### 4 Threads - 250,000 Messages Each - Log Message Longer than 500 Bytes
Longer messages with 4 threads show the expected results. Average latency for llamalog nearly doubles. This time, even g3log shows measurable differences. Again, the results for spdlog are very good and the 99th and 99.9th percentiles.

|Logger|50th|75th|90th|99th|99.9th|Worst|Average|Total|
|:---|---:|---:|---:|---:|---:|---:|---:|---:|
|llamalog|0.7|1.3|2.5|4.6|146.4|471.0|1.62763|9.12183|
|NanoLog|0.9|1.2|2.4|4.6|149.4|647.4|1.78600|37.44443|
|spdlog|1.5|2.0|3.2|7.7|117.6|555.4|2.19079|3.17157|
|g3log|6.1|6.4|7.4|12.8|110.0|2,123.2|6.09669|13.96604|

### Conclusion
The various loggers are obviously optimized for different use cases. Logging using spdlog and g3log takes longer but does not change that much with number of parallel threads or message size. On the other hand, both NanoLog and llamalog generally show the best results up to the 90th percentile.

My aim was not to build the fastest logger but something that is easily useable and fast enough. Based on the data above, I think llamalog is mature enough to get in the ring. And I can use it for my programming without worrying too much.


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
        "llamalog.log");
    llamalog::Initialize(std::move(writer));
    LOG_INFO("Program {} called with {} arguments.", argv[0], argc);
}
```
This produces the following log line, given the code is stored as `main.cpp` and compiled to `L:\\llamalog.exe` (`9234` is a sample thread id):
```
2019-03-31 15:27:12.283 INFO [9234] main.cpp:19 main Program L:\\llamalog.exe called with 1 arguments.
```


## Formatting
The patterns use the standard {fmt} syntax with the following enhancements:
- Output can be escaped using the syntax for strings in C.
- Exceptions are supported as formatter arguments.

### Escaping
Any character and string arguments are escaped. To prevent escaping, add the type specifier in the format string, i.e. `{0}` prints escaped, while `{0:s}` prints the raw string.

### Exception Formatting
Exceptions can be formatted as first-class arguments, though adding the exception as a logger argument MUST happen inside the catch clause. If an exception is thrown using llamalog::Throw (and the macro LLAMALOG_THROW respectively) the result of `what()` is ignored and replaced by the logging message.

The following code shows a valid usage pattern:
```cpp
try {
    THROW(std::invalid_argument("ignored"), "Value {} is invalid", val);
} catch (const std::exception& e) {
    LOG_INFO("An exception has occurred: {}", e);
}
```
It is also possible to log plain C++ exceptions like in the following example. Yet, no extended logging context is available.
```cpp
try {
    throw std::invalid_argument("somearg");
} catch (const std::exception& e) {
    LOG_INFO("An exception has occurred: {}", e);
}
```

Exceptions are formatted using a default pattern, however a custom pattern MAY be used as in the following example where only the exception message (i.e. `e.what()`) is logged.
```cpp
try {
    THROW(std::invalid_argument("somearg"));
} catch (const std::exception& e) {
    LOG_INFO("An exception has occurred: {:%w}", e);
}
```
More than one argument specifier MAY be used, e.g. `{:%F:%L}` will print the file name and line number where the exception happened. The following specifiers are supported. If an exception does not have the information an empty string is used in the output.

|Specifier|Output|
|:---|:---|
|`%l`|The log message. If the log message is `nullptr`, `%w` is logged.|
|`%C`|The name of the error category for `std::system_error`, `llamalog::SystemError` and derived classes.|
|`%c`|The error code for `std::system_error`, `llamalog::SystemError` and derived classes.|
|`%m`|The error message for `std::system_error`, `llamalog::SystemError` and derived classes.|
|`%w`|The exception message, i.e. the result of `std::exception` `what()`. If the exception was thrown using `llamalog::Throw` (and `LLAMALOG_THROW` respectively) the log message is returned and - in case of `std::system_error` and `llamalog::SystemError` - includes the error message `%m` (i.e. the output matches `std::system_error` except for the exception argument replaced by the log message). |
|`%T`|The timestamp as `yyyy-MM-dd HH:mm:ss.SSS`.|
|`%t`|The thread id of the thread which has thrown the exception.|
|`%F`|The file name where the exception was thrown.|
|`%L`|The line number where the exception was thrown.|
|`%f`|The function name where the exception was thrown.|

Exceptions are often caught using a base class. Therefore it is not known which data is actually available for logging. Using the pattern syntax above could either lead to ugly output (like in `An error with code {0:%c} has happened: {0:%m}` when the exception is not a `std::system_error` or overly complex catch clauses just to log the right data.

llamalog therefore supports conditional patterns which are only printed if any one of the contained specifiers produced a non-empty output. The pattern from the previous paragraph would produce <code>An error with code &nbsp;has happened: somemessage</code> for a `std::exception` (please also note the double space between "code" and "has"). The following pattern solves the problem: `An error {0:%[with code %c ]}has happened: {0:%w}` and will output `An error has happened: somemessage` if the exception is not a `std::system_error`. You may even nest `%[...]` blocks and you can also use formatter arguments inside these blocks, like in the following example:
```cpp
try {
    // code could throw exceptions with extended logging context and without
} catch (const std::exception& e) {
    LOG_INFO("An exception has occurred: {:%w%[ {0}%F:%L]}", e, "location=");
}
```
This sample logs the exception message and - if extended logging data is available - a space character, the text "location=" followed by file and line number.

Using this syntax, the default exception pattern is `%w%[ (%C %c)]%[ @\{%T \[%t\] %F:%L %f\}]` which produces output like `Log Message: Invalid argument (system 22) @{2019-03-27 22:18:23.231 [9384] myfile.cpp:87 myfunction}`.

By the way: Was I carried away by the formatting syntax? Maybe. Is it useful? Indeed. Does it hurt, if anyone does not want to use it? Not really. So I keep the feature although the default formatting will probably be sufficient for most purposes. :sunglasses:


## Documentation
Run doxygen on `doc/public.doxy` for a user documentation and on `doc/developer.doxy` for a documentation also including all internals.


## History
While writing a Windows desktop application I wanted to do some logging from a COM DLL. Experimenting with the usual logging macros which write to a file etc. did just not feel right. Also doing a lot of work in Java, I was used to highly configurable and fast loggers. There are many logging frameworks for C++, but either they required a config file (which I did not want to ship with just a small DLL), they were code-heavy or they required including tons of headers adding to the time needed for compilation. I also wanted to have a text-based log file which I could use for debugging right away without having to resort to some tool for decrypting or unpacking a log file.

After a little research I found the marvelous [NanoLog](https://github.com/Iyengar111/NanoLog) written by Karthik Iyengar. The library is small, fast and did nearly everything I wanted. Benchmarks are well documented. So - perfect? Not quite, especially the formatting part did not make me really happy. Using the << operator for adding log arguments and having to interleave arguments and message text (e.g. `LOG_INFO("There are ") << count << " items."`) just felt awkward.

So I started to add some features, change a little bit here, add a little bit there, refactor just slightly... And after some time coding, here we are now.


## Where Does the Name Come From?
llamalog stands for (l)ightweight (l)ean (a)nd (m)ean (a)synchronous (log)ger for C++. As a plus, there are not yet so many hits in Google. And of course because llamas are cute and everybody likes them. :wink:
