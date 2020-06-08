# llamalog Changes

## v2.0.0 - 2020-06-08
- [Breaking] Renamed LogLevel to Priority.
- [Breaking] Renamed CustomTypes.h to custom_types.h.
- [Breaking] Only escape non-printable characters but leave high ASCII unchanged.
- [Breaking] Changed format string for RECT.
- [Feature] Throw exceptions with formatted messages.
- [Feature] Allow formatting of exceptions, error codes and std::align_val_t.
- [Feature] Null-safe logging of values of pointer variables.
- [Feature] Flush function for logger.
- [Feature] Exception-safe logging by calling SLOG_....
- [Feature] Macro LOG_TRACE_RESULT for logging function return values.
- [Feature] Added LogWriter for sending output to stderr.
- [Feature] Target size of type MAY be provided using macro LLAMALOG_LOGLINE_SIZE for larger of smaller buffer requirements.
- [Feature] Allow setting log level at compile time using LLAMALOG_LEVEL_xxx.
- [Optimized] Reduced foot print of custom types by using a jump table.
- [Fix] Fixed wrong calculation of alignment for log arguments.
- [Fix] Fixed handling of errors during logging.
- [Fix] Compilation error when first argument is a custom argument.
- [Fix] Fixed various other bug found in tests and using clang-tidy.
- [Documentation] Made clear that only parameters are escaped.

## v1.0.0 - 2019-03-15
Initial Release.