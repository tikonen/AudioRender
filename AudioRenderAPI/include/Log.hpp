#pragma once

#include <string>

void __LOG(const char* mod, _Printf_format_string_ const char* szFormat, ...);

#define LOGE(format, ...) __LOG("[ERR] ", format, ##__VA_ARGS__)
#define LOGW(format, ...) __LOG("[WARN]", format, ##__VA_ARGS__)
#define LOG(format, ...) __LOG(NULL, format, ##__VA_ARGS__)

const char* HResultToString(long result);
const char* GetLastErrorString();

