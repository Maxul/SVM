#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>

char *ram;

void *open_ram(void)
{
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0)
    {
        perror("/dev/mem");
        exit(1);
    }

    void *mem = mmap(0, 0xffff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
    {
        perror("mmap low memory error");
        exit(1);
    }

    return mem;
}

void *scanner(void *unused)
{
    char buffer[1024];

    for (;;)
    {
        bzero(buffer, sizeof(buffer));
        if (1 == scanf("%s", buffer))
        {
            strcpy(&ram[0xffac], buffer);
            ram[0xffaa] = '1';
        }
    }
}

#define MAGIC "TRUST"

int main(int argc, char** argv)
{
    pthread_t id;
    
    puts("Neo man is ready!");

    ram = open_ram();
    if (memcmp(ram, MAGIC, sizeof(MAGIC)) != 0)
    {
        exit(1);
    }

    pthread_create(&id, NULL, &scanner, NULL);

    char buffer[1024];

    for (;;)
    {
        bzero(buffer, sizeof(buffer));
        if (ram[0xffab] == '1')
        {
            strcpy(buffer, &ram[0xffac]);
            printf("%s\n", buffer);
            ram[0xffab] = '0';
        }
    }

    return 0;
}

