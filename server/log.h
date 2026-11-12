#pragma once
//Minimal logger with timestamps. Errors go to stderr, everything else to stdout.
//Log player ids, counts, and timings only. Never log raw client messages or
//anything personal, since logs are often shared when debugging.

#include <cstdarg>
#include <cstdio>
#include <ctime>

enum class LogLevel : unsigned char {Info, Warn, Error};

inline void logMsg(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

inline void logMsg(LogLevel level, const char *fmt, ...) {
    static const char *const kNames[] = {"INFO", "WARN", "ERROR"};
    FILE *out = (level == LogLevel::Error) ? stderr : stdout;

    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    char ts[16];
    std::strftime(ts, sizeof ts, "%H:%M:%S", &tm);

    std::fprintf(out, "%s [%s] ", ts, kNames[static_cast<int>(level)]);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(out, fmt, args);
    va_end(args);
    std::fputc('\n', out);
    std::fflush(out);
}