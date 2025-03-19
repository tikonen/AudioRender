#include <Windows.h>

#include <stdio.h>
#include <ctime>

#include "Log.hpp"

const char* GetErrorMsg(const unsigned long error)
{
    static char msg[512];

    // Call FormatMessage to get error string from winapi
    LPSTR buffer = nullptr;
    DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);
    if (size > 0) {
        // trim trailing newlines, some error messages might contain them
        while (size-- > 0) {
            if ((buffer[size] != '\n') && (buffer[size] != '\r')) {
                break;
            }

            buffer[size] = '\0';
        }
    }
    sprintf_s(msg, "0x%X: %s", error, buffer);
    LocalFree(buffer);

    return msg;
}

// Returns message text for last error if set, empty string otherwise.
const char* GetLastErrorString()
{
    DWORD err = GetLastError();
    return err ? GetErrorMsg(err) : "";
}

const char* HResultToString(const long result)
{
    // Handle success case
    if (result == ERROR_SUCCESS) return "Success";

    return GetErrorMsg(result);  // Call other helper to convert win32 error to string
}


void __LOG(const char* mod, _Printf_format_string_ const char* format, ...)
{
    va_list args;
    va_start(args, format);
    if (mod) {
        fputs(mod, stdout);
        fputs(" ", stdout);
    }
    vprintf(format, args);
    fputs("\n", stdout);
    va_end(args);
}
