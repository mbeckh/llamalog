/*
Copyright 2019 Michael Beckh

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/// @file

#include "llamalog/winapi_log.h"

#include "llamalog/LogLine.h"
#include "llamalog/custom_types.h"
#include "llamalog/modifier_format.h"  // IWYU pragma: keep
#include "llamalog/modifier_types.h"   // IWYU pragma: keep
#include "llamalog/winapi_format.h"    // IWYU pragma: keep

#include <windows.h>

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const llamalog::error_code arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const POINT* const arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT& arg) {
	return logLine.AddCustomArgument(arg);
}

llamalog::LogLine& operator<<(llamalog::LogLine& logLine, const RECT* const arg) {
	return logLine.AddCustomArgument(arg);
}
