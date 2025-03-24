#include "mlog.h"

#ifdef _WIN32
static HANDLE hConsole;
static WORD defaultAttributes;
static void set_console_color(log_level level) {
    static int initialized = 0;
    if (!initialized) {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(hConsole, &info);
        defaultAttributes = info.wAttributes;
        initialized = 1;
    }

    WORD color;
    switch (level) {
        case LOG_LEVEL_DEBUG:   color = FOREGROUND_GREEN; break;
        case LOG_LEVEL_INFO:    color = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case LOG_LEVEL_WARN:    color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case LOG_LEVEL_ERROR:   color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case LOG_LEVEL_FATAL:   color = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
        default:      color = defaultAttributes;
    }
    SetConsoleTextAttribute(hConsole, color);
}

static void reset_console_color() {
    SetConsoleTextAttribute(hConsole, defaultAttributes);
}
#else
static void set_console_color(LogLevel level) {
    const char* color_code;
    switch (level) {
        case TRACE:   color_code = "\033[32m"; break;  // Green
        case DEBUG:   color_code = "\033[32m"; break;  // Green
        case INFO:    color_code = "\033[36m"; break;  // Cyan
        case WARNING: color_code = "\033[33m"; break;  // Yellow
        case ERROR:   color_code = "\033[31m"; break;  // Red
        case FATAL:   color_code = "\033[41;37m"; break; // White on red
        default:      color_code = "\033[0m";
    }
    fprintf(stdout, "%s", color_code);
}

static void reset_console_color() {
    fprintf(stdout, "\033[0m");
}
#endif

struct logger {
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    output_target* outputs;
    size_t output_count;
    log_level level;
    // 文件状态
    FILE* current_file;     // 当前日志文件指针
    size_t current_size;    // 当前文件大小
    time_t file_create_time; // 当前文件创建时间
    log_split_config split_cfg; // 日志分割
};

static const char* level_strings[] = {
       "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef _WIN32
static void mutex_init(CRITICAL_SECTION* mutex) { InitializeCriticalSection(mutex); }
static void mutex_lock(CRITICAL_SECTION* mutex) { EnterCriticalSection(mutex); }
static void mutex_unlock(CRITICAL_SECTION* mutex) { LeaveCriticalSection(mutex); }
static void mutex_destroy(CRITICAL_SECTION* mutex) { DeleteCriticalSection(mutex); }
#else
static int mutex_init(pthread_mutex_t* mutex) { return pthread_mutex_init(mutex, NULL) == 0; }
static void mutex_lock(pthread_mutex_t* mutex) { pthread_mutex_lock(mutex); }
static void mutex_unlock(pthread_mutex_t* mutex) { pthread_mutex_unlock(mutex); }
static void mutex_destroy(pthread_mutex_t* mutex) { pthread_mutex_destroy(mutex); }
#endif
static void get_timestamp(char* buffer, size_t len) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, len, "%04d-%02d-%02d %02d:%02d:%02d.%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    strftime(buffer, len, "%Y-%m-%d %H:%M:%S", &tm);
    size_t offset = strlen(buffer);
    snprintf(buffer + offset, len - offset, ".%03ld", tv.tv_usec / 1000);
#endif
}

/**
 *  检查是否需要分割文件的条件
 * @param ctx
 * @return
 */
static int need_rotate(logger* ctx) {
    // 按时间分割检查
    if (ctx->split_cfg.split_interval > 0) {
        time_t now = time(NULL);
        time_t t =now - ctx->file_create_time;
        if (t >= ctx->split_cfg.split_interval) {
            return 1;
        }
    }

    // 按大小分割检查
    if (ctx->split_cfg.max_size > 0 &&
        ctx->current_size >= ctx->split_cfg.max_size) {
        return 1;
    }

    return 0;
}

/**
 * 生成带时间戳的文件名
 * @param base_path
 * @param buffer
 * @param len
 */
static void generate_filename(logger* ctx, char* buffer,size_t len) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    snprintf(buffer, len, "%s/%s_%04d%02d%02d_%02d%02d%02d.log",
             ctx->split_cfg.base_path, ctx->split_cfg.base_name,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

logger* logger_create(log_level level) {
    logger* ctx = calloc(1, sizeof(logger));
    if (!ctx) return NULL;
#ifdef _WIN32
    mutex_init(&ctx->mutex);
#else
    if (!mutex_init(&ctx->mutex)) {
        free(ctx);
        return NULL;
    }
#endif
    ctx->level = level;
    ctx->outputs = NULL;
    ctx->output_count = 0;
    // 分割配置
    ctx->split_cfg.base_path="Log";
    ctx->split_cfg.base_name="app";
    ctx->split_cfg.max_size=1024 * 1024 * 10, // 100MB
    ctx->split_cfg.max_files=10;
    ctx->split_cfg.split_interval=86400; // 每天分割
    return ctx;
}

void logger_destroy(logger* ctx) {
    if (!ctx) return;
    // 初始化同步原语
    mutex_lock(&ctx->mutex);
    for (size_t i = 0; i < ctx->output_count; ++i) {
        if (ctx->outputs[i].type == OUTPUT_FILE && ctx->outputs[i].stream) {
            fclose(ctx->outputs[i].stream);
        }
    }
    free(ctx->outputs);
    mutex_unlock(&ctx->mutex);
    mutex_destroy(&ctx->mutex);
    free(ctx);
}

void logger_add_console(logger* ctx) {
    mutex_lock(&ctx->mutex);
    ctx->outputs = realloc(ctx->outputs, (ctx->output_count + 1) * sizeof(output_target));
    ctx->outputs[ctx->output_count++] = (output_target){OUTPUT_CONSOLE, stdout};
    mutex_unlock(&ctx->mutex);
}

/**
 * @brief 跨平台创建目录
 * @param path 目录路径，支持正斜杠和反斜杠
 * @return 0成功，-1失败（可通过errno获取错误码）
 */
int mkdir_p(const char *path) {
#ifdef _WIN32
    // Windows创建目录（自动处理斜杠）
    int ret = mkdir(path);
#else
    // Linux/macOS创建目录（设置权限为rwxr-xr-x）
    int ret = mkdir(path, 0755);
#endif
    if (ret == 0) return 0;  // 创建成功
    // 处理已存在的情况
    if (errno == EEXIST) {
        // 检查是否确实是目录
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(path);
        if (attrs != INVALID_FILE_ATTRIBUTES &&
            (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return 0;
        }
#else
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            return 0;
        }
#endif
        errno = EEXIST; // 路径存在但不是目录
    }
    return -1;
}

/**
 * 执行文件分割操作
 * @param ctx
 */
static void rotate_file(logger* ctx) {
    if (ctx->current_file) {
        fclose(ctx->current_file);
        ctx->current_file = NULL;
    }

    // 生成新文件名
    char new_path[512];
    generate_filename(ctx,new_path,sizeof(new_path));

    // 打开新文件
    FILE* new_file = fopen(new_path, "a");
    if (new_file) {
        ctx->current_file = new_file;
        ctx->current_size = 0;
        ctx->file_create_time = time(NULL);
    }

    // 清理旧文件（保留最近max_files）
    if (ctx->split_cfg.max_files > 0) {
        // 这里可以添加文件清理逻辑，使用scandir等函数
    }
}

void logger_add_file(logger* ctx) {
    mkdir_p(ctx->split_cfg.base_path);
    char fullPath[256];
    generate_filename(ctx,fullPath,sizeof(fullPath));
    FILE* fp = fopen(fullPath, "a+");
    if (!fp) return;
    if (fp) {
        ctx->current_file = fp;
        ctx->current_size = 0;
        ctx->file_create_time = time(NULL);
    }
    mutex_lock(&ctx->mutex);

    ctx->outputs = realloc(ctx->outputs, (ctx->output_count + 1) * sizeof(output_target));
    ctx->outputs[ctx->output_count++] = (output_target){OUTPUT_FILE, fp};

    mutex_unlock(&ctx->mutex);
}

void log_message(logger* ctx, log_level level, const char* file, int line, const char* fmt, ...) {
    if (!ctx || level < ctx->level) return;
    // 格式化日志消息
    char timestamp[30] ={0};
    get_timestamp(timestamp, sizeof(timestamp));

    char message[4096];
    va_list args;
    va_start(args, fmt);
    // vsnprintf将可变参数格式化为字符串,并将结果存储在指定的缓冲区中。
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // 构建完整日志
    char full_message[8192];
    int msg_len = snprintf(full_message, sizeof(full_message), "[%s] [%s] [%s:%d] %s\n",
             timestamp, level_strings[level], file, line, message);

    mutex_lock(&ctx->mutex);

    // 检查是否需要分割文件
    if (ctx->current_file && need_rotate(ctx)) {
        rotate_file(ctx);
    }

    // 文件输出
    for (size_t i = 0; i < ctx->output_count; ++i) {
        output_target* target = &ctx->outputs[i];

        if (target->type == OUTPUT_FILE && ctx->current_file) {
            // 写入文件并更新大小
            size_t written = fwrite(full_message, 1, msg_len, ctx->current_file);
            ctx->current_size += written;
            fflush(ctx->current_file);
        } else {
            // 控制台输出
            set_console_color(level);
            fprintf(target->stream, "%s", full_message);
            reset_console_color();
        }

//        fprintf 函数可以将数据按指定格式写入到文本文件中
//        fprintf(target->stream, "%s", full_message);
//        fflush(target->stream);
    }

    mutex_unlock( &ctx->mutex);
}
