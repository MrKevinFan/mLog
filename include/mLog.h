#ifndef MLOG_H
#define MLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
// 平台抽象
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <synchapi.h>
#include <direct.h>  // for _mkdir
#define mkdir(path) _mkdir(path)
#else
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h> // for mkdir
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define LOG_TRACE(ctx, ...) log_message(ctx, LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(ctx, ...) log_message(ctx, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(ctx, ...)  log_message(ctx, LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(ctx, ...)  log_message(ctx, LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(ctx, ...) log_message(ctx, LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(ctx, ...) log_message(ctx, LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

typedef struct {
    enum { OUTPUT_CONSOLE, OUTPUT_FILE } type;
    FILE* stream;
} output_target;
/**
 * 日志级别定义
 */
typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level;

/**
 * 控制台颜色定义
 */
typedef enum {
    COLOR_RESET,
    COLOR_BLACK = 30,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE
} ConsoleColor;

/**
 * 日志文件分割配置
 */
typedef struct {
    char* base_path;        // 基础路径（不含扩展名）
    char* base_name;        // 日志名称（不含扩展名）
    size_t max_size;        // 最大文件大小（字节）
    int max_files;          // 最大保留文件数
    time_t split_interval;  // 分割间隔（秒，0表示不按时间分割）
} log_split_config;

typedef struct logger logger;

logger* logger_create(log_level level);
void logger_destroy(logger* ctx);
void logger_add_console(logger* ctx);
void logger_add_file(logger* ctx);
void log_message(logger* ctx, log_level level, const char* file, int line, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // MLOG_H
