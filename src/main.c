#include <stdio.h>
#include <unistd.h>
#include "mlog.h"

int main() {
    printf("Hello, World!\n");
    logger *logger = logger_create(LOG_LEVEL_ERROR);
    logger_add_console(logger);
    logger_add_file(logger);

    LOG_TRACE(logger, "This is a trace message");
    LOG_DEBUG(logger, "This is a debug message");
    LOG_INFO(logger, "This is a info message ");
    LOG_WARN(logger, "This is a warn message");
    LOG_ERROR(logger, "This is a error message");
    LOG_FATAL(logger, "This is a fatal message");

    logger_destroy(logger);

    return 0;
}
