#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>

#define LEN ((1<<3))

int main()
{
    int fd;
    char *start;
    char *buf;

    fd = open("/dev/dev_mem", O_RDWR);
    if (fd < 0) {
        perror("open /dev/dev_mem");
        exit(1);
    }

    buf = (char *)malloc(LEN);
    bzero(buf, LEN);

    start = mmap(NULL, LEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (start == MAP_FAILED) {
        perror("mmap error");
        exit(1);
    }

    strncpy(buf, start, LEN);
    printf("ORIGIN: buf = %s\n", buf);

    memset(start, 'x', LEN);

    bzero(buf, LEN);
    strncpy(buf, start, LEN);
    printf("EDITED: buf = %s\n", buf);

    munmap(start, LEN);
    free(buf);
    close(fd);

    return 0;
}

