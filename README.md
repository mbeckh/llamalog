# llamalog
A lightweight lean and mean asynchronous logger for C++.

Added
- Formatting is done in the background thread using fmtlib.
- Support for wide character strings (written as UTF-8)
- Support for custom types.
- Detailed types for reduced storage
- Comply with strict aliasing rules.
- Removed virtual functions in buffer access (no non-guaranteed logger)
- Use stack-based storage in buffer instead of heap
