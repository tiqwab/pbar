#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pbar.h"

static struct fdprogress progress;

/*
 * Parse /proc/<pid>/fdinfo/<fd> into struct fdinfo.
 * Return 0 if success, otherwise -1.
 */
int read_fdinfo(const char *path, struct fdinfo *info) {
    char infobuf[INFO_BUF_SIZE];
    char *cur;
    int infofd;
    int n;

    if ((infofd = open(path, O_RDONLY)) < 0) {
        perror("open");
        return -1;
    }
    if ((n = read(infofd, infobuf, INFO_BUF_SIZE - 1)) < 0) {
        perror("read");
        return -1;
    }
    infobuf[n] = '\0';

    cur = infobuf;
    if ((cur = strstr(cur, "pos:")) == NULL) {
        fprintf(stderr, "invalid fdinfo\n");
        return -1;
    }
    cur += 4;
    while (isspace(*cur)) cur++;
    info->pos = strtol(cur, &cur, 10);

    if ((cur = strstr(cur, "flags:")) == NULL) {
        fprintf(stderr, "invalid fdinfo\n");
        return -1;
    }
    cur += 6;
    while (isspace(*cur)) cur++;
    info->flags = strtol(cur, &cur, 8);

    close(infofd);

    return 0;
}

/*
 * Print progress.
 * Assume that processing a file is finished if is_finished = true.
 */
void print_progress(struct fdprogress *progress, bool is_finished) {
    long pos, size;

    pos = progress->pos;
    size = progress->size;
    if (is_finished) {
        pos = size;
    }
    printf("\r%s: %ld / %ld (%3.1f %%)",
            progress->path, pos, size, 100.0 * pos / size);
    if (is_finished) {
        printf("\n");
    }
    fflush(stdout);
}

/*
 * Find a target fd and display the progress.
 * Return 0 if success, otherwise -1.
 */
int display_progress(pid_t pid) {
    char path_buf[PATH_MAX_LEN];
    DIR *fddir; // DIR* for /proc/<pid>/fd
    struct dirent *fddent; // each entry in /proc/<pid>/fd
    struct fdprogress prev_progress;

    // to memorize whether processing the previous file is finished
    prev_progress.fd = -1;

    snprintf(path_buf, PATH_MAX_LEN, "/proc/%d/fd", (int) pid);
    if ((fddir = opendir(path_buf)) == NULL) {
        perror("opendir");
        return -1;
    }

    errno = 0;
    while ((fddent = readdir(fddir)) != NULL) {
        int fd;
        int n;
        char src_path_buf[PATH_MAX_LEN]; // buffer for path each fd points
        struct stat stat_buf;
        struct fdinfo info;

        // open fd
        if (strcmp(fddent->d_name, ".") == 0 || strcmp(fddent->d_name, "..") == 0) {
            continue;
        }

        fd = atoi(fddent->d_name);

        // retrieve path
        snprintf(path_buf, PATH_MAX_LEN, "/proc/%d/fd/%d", (int) pid, fd);
        if ((n = readlink(path_buf, src_path_buf, PATH_MAX_LEN - 1)) < 0) {
            perror("readlink");
            continue;
        }
        src_path_buf[n] = '\0';

        // read fdinfo
        snprintf(path_buf, PATH_MAX_LEN, "/proc/%d/fdinfo/%d", (int) pid, fd);
        if (read_fdinfo(path_buf, &info) < 0) {
            continue;
        }

        // find a fd pointing to the source file
        if (IS_FLAGS_RDONLY(info.flags) == 0) {
            continue;
        }
        if (stat(src_path_buf, &stat_buf) < 0) {
            perror("stat");
            continue;
        }
        if (!S_ISREG(stat_buf.st_mode)) {
            continue;
        }

        if (strcmp(progress.path, src_path_buf) != 0) {
            // assume that processing the previous file was finished
            memcpy(&prev_progress, &progress, sizeof(struct fdprogress));
            prev_progress.pos = prev_progress.size;
        }
        progress.fd = fd;
        progress.pos = info.pos;
        progress.flags = info.flags;
        progress.size = stat_buf.st_size;
        strncpy(progress.path, src_path_buf, PATH_MAX_LEN);

        break;
    }
    if (errno) {
        perror("readdir");
        return -1;
    }
    closedir(fddir);

    if (progress.fd < 0) {
        fprintf(stderr, "No file to display\n");
        return -1;
    }

    if (prev_progress.fd > 0) {
        print_progress(&prev_progress, true);
    }

    print_progress(&progress, false);

    return 0;
}

int main(int argc, char *argv[]) {
    pid_t pid;
    char **cmd;
    int res;
    int status;

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s CMD [ARG]...\n"
                "Display progress of each file CMD processes.\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    cmd = &argv[1];

    // to check whether a source file is found
    progress.fd = -1;

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // child
        if (execvp(cmd[0], cmd) < 0) {
            perror("execl");
            exit(EXIT_FAILURE);
        }
    }

    // parent
    usleep(100 * 1000); // FIXME

    while ((res = waitpid(pid, &status, WNOHANG)) == 0) {
        display_progress(pid);
        if (usleep(DISPLAY_INTERVAL_MILLIS * 1000) < 0) {
            perror("usleep");
            exit(EXIT_FAILURE);
        }
    }

    if (res < 0) {
        // error
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    // child finished
    if (WIFEXITED(status)) {
        print_progress(&progress, true);
        exit(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "command finished unexpectedly with sinal: %d\n", WTERMSIG(status));
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "command finished unexpectedly\n");
    exit(EXIT_FAILURE);
}
