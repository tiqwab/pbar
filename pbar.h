#ifndef PBAR_MAIN_H
#define PBAR_MAIN_H

#ifdef PATH_MAX
#define PATH_MAX_LEN PATH_MAX
#else
#define PATH_MAX_LEN 1024
#endif

#define INFO_BUF_SIZE 1024
#define DISPLAY_INTERVAL_MILLIS 200

// mask to retrieve file mode from fdinfo.flags
#define IS_FLAGS_RDONLY(flags) ((flags & O_ACCMODE) == O_RDONLY)
#define IS_FLAGS_WRONLY(flags) ((flags & O_ACCMODE) == O_WRONLY)
#define IS_FLAGS_RDWR(flags)   ((flags & O_ACCMODE) == O_RDWR)

struct fdinfo {
    long pos;
    int flags;
};

struct fdprogress {
    int fd;
    char path[PATH_MAX_LEN];
    long pos;
    int flags;
    long size;
};

#endif /* PBAR_MAIN_H */
