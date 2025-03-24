
(本人菜鸡一枚，欢迎大佬么指正！)

### mLog说明

mLog 是一个轻量级的日志管理库，用于记录应用程序的关键信息。个人趋向于简单化所以只有一个 .c 和 .h 文件，设理论是上支持跨平台操作的，因为做了适配但目前还未验证。目前Windows平台运行正常，可以输出到控制台和文件。输出文件支持按时间分割或文件大小分割，文件命名追加了时间进行区分。


### mLog使用

只需将 mlog.h 和 mlog.c 引入到项目中即可直接使用，支持 TRACE、DEBUG、INFO、WARN、ERROR、FATAL多种级别的日志输出。日志级别由高到底分别是：fatal、error、warn、info、debug、trace，低级别的会输出高级别的信息但高级别的不会输出低级别。

```c
#define LOG_TRACE(ctx, ...) 
#define LOG_DEBUG(ctx, ...) 
#define LOG_INFO(ctx, ...)  
#define LOG_WARN(ctx, ...)  
#define LOG_ERROR(ctx, ...)
#define LOG_FATAL(ctx, ...) 
```



### mLog示例

下面是一个简单的使用示例，通过logger_create来初始化一个日志实例。logger_add_console用来添加控制台的输出，logger_add_file用来添加文件的输出。

```
#include <stdio.h>
#include <unistd.h>
#include "mlog.h"

int main() {
    logger *logger = logger_create(LOG_LEVEL_DEBUG);
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
```



### mLog版本说明



V1.0.0-Alpha

2025年3月24日跟新，初版发布 。

