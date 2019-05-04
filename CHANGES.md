# llamalog Changes

## (Upcoming)
- [Breaking] Renamed LogLevel to Priority.
- [Breaking] Changed function names to lower case
- [Breaking] Renamed CustomTypes.h to custom_types.h
- [Feature] Throw exceptions with formatted messages.
- [Feature] Allow formatting of exceptions.
- [Feature] Target size of type MAY be provided using macro LLAMALOG_LOGLINE_SIZE for larger of smaller buffer requirements.
- [Feature] Flush function.
- [Feature] Allow setting log level at compuile time using LLAMALOG_LEVEL_xxx.
- [Feature] Null-safe logging of values of pointer variables.
- [Fix] Fixed wrong calculation of alignment for log arguments.
- [Fix] Fixed handling of errors during logging.

## v1.0.0
Initial Release.