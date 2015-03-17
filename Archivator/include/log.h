#ifndef _ARCHIVATOR_LOG_
#define _ARCHIVATOR_LOG_

#ifndef LOG_GROUP
#define LOG_GROUP "LOG_GROUP is not specified"
#endif //LOG_GROUP

#ifdef DEBUG
#define LOGMESG(a, ...) logMesg( __FILE__, __LINE__, LOG_GROUP, a, ##__VA_ARGS__ )
#else

#define LOGMESG(a) do{}while(0)
#endif //DEBUG
#include <pthread.h>
#include <sys/time.h>

#define MAX_MESG_SIZE 1024
#define WRITER_ATOM_SIZE MAX_MESG_SIZE

enum LOGLEVELS{
    LOG_ALL = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_LEVELS_COUNT
};

enum LOGFLAGS{
    LOG_PRINT_TIME = 1<<0,
    LOG_PRINT_GROUP = 1<<1,
    LOG_PRINT_FILE = 1<<2,
    LOG_PRINT_LINE = 1<<3
};

typedef struct
{
    int isStarted ;
    int logDes;
    unsigned flags;
    int logLevel;
    int writeBufDes;
    int readBufDes;
    pthread_t writerThreadId;
    struct timeval startTime;
} LOGMAININFO;


int logInit(unsigned logLevel, unsigned flags, const char * filename);

int logMesg( const char *fname, int lineno ,char* group, int priority ,const char* str,...);

#endif // _ARCHIVATOR_LOG_
