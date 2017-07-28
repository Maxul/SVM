/*
	Usage: vm_center

	Author: Ecular Xu
	<ecular_xu@trendmicro.com.cn>

	Author: Maxul Lee
	<maxul@bupt.edu.cn>

	Last Update: 2017/1/11
	Last Update: 2017/2/16 Maxul Lee

*/

#include <stdio.h>		// for printf
#include <stdlib.h>		// for exit
#include <string.h>		// for bzero

#include <fcntl.h>
#include <unistd.h>

#include <netinet/in.h>		// for sockaddr_in
#include <sys/types.h>		// for socket
#include <sys/socket.h>		// for socket
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <pthread.h>

#define SERVER_PORT (18888)
#define LISTEN_QUEUE_LENGTH (20)
#define BUFFER_SIZE (1<<22)
#define MAX_THREAD_NUM (100)

#define DBGprintf(fmt, ...) { \
    do { printf(fmt, ##__VA_ARGS__); fflush(stdout); } while (0); \
}

typedef struct {
    int socket;     // VM socket
    char *name;     // VM name
} List;

static int thread_count = 0;
static int socket_count = 0;

pthread_mutex_t mutex_list;

pthread_t threads[MAX_THREAD_NUM];
pthread_t readline_worker;

List name_list[MAX_THREAD_NUM];

/**
 * 接收数据的执行函数
 * @func worker
 * @param conn_socket
 */
void *worker(void *conn_socket);
void *scanner(void *unused);


int main(int argc, char** argv)
{
    printf("%s is booting!\n", argv[0]);

    pthread_mutex_init(&mutex_list,NULL);

    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Socket create failed.\n");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof (server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr*) &server_addr, sizeof (server_addr)) < 0)
    {
        printf("Bind port %d failed.\n", SERVER_PORT);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, LISTEN_QUEUE_LENGTH) < 0)
    {
        printf("Server Listen Failed!");
        return EXIT_FAILURE;
    }

    pthread_create(&readline_worker, NULL, scanner, (void *)0);

    bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);

        int client_conn = accept(server_socket, (struct sockaddr *) &client_addr, &length);
        if (client_conn < 0)
        {
            printf("Server Accept Failed!\n");
            return EXIT_FAILURE;
        }
        printf("New Connection: 0x%x\n", client_conn);

        int pthread_err = pthread_create(threads + (thread_count++),
                                         NULL, worker, (void *)&client_conn);
        if (pthread_err != 0)
        {
            printf("Create thread Failed!\n");
            return EXIT_FAILURE;
        }

    }
    close(server_socket);
    return (EXIT_SUCCESS);
}

int add_list(char *vmname, int socket)
{
    char *p_name = (char *)malloc(strlen(vmname) + 1);

    if (p_name == NULL)
        return 1;

    strcpy(p_name, vmname);

    name_list[socket_count].socket = socket;
    name_list[socket_count].name = p_name;
    socket_count += 1;
    return 0;
}

int del_list(char *vmname)
{
    int i = 0, j = 0;

    for (i = 0; i < socket_count; ++i)
    {
        if (strcmp(name_list[i].name, vmname) == 0)
        {
            free(name_list[i].name);
            for (j = i; j < socket_count; j++)
                name_list[j] = name_list[j+1];
            socket_count -= 1;
            return 0;
        }
    }
    return 1;
}

static int broadcast(char *data)
{
    int i, j = 0;
    char send_data[200] = {0};

    strcat(send_data, data);
    for (i = 0; i < socket_count; i++)
    {
        if (write((int)name_list[i].socket, send_data, strlen(send_data)) > 0)
        {
            DBGprintf("broadcast: \"%s\"! to %s success!\n", send_data, name_list[i].name);
        }
        else
        {
            j++;
            DBGprintf("broadcast: \"%s\"! to %s failed!\n", send_data, name_list[i].name);
        }
    }
    return !!j;
}

int broadcast_add(char *vmname)
{
    char send_data[100] = {'+'};

    strcat(send_data, vmname);
    return broadcast(send_data);
}

int broadcast_del(char *vmname)
{
    char send_data[100] = {'-'};

    strcat(send_data, vmname);
    return broadcast(send_data);
}

int find_socket(char *name)
{
    int i;

    for (i = 0; i < socket_count; i++)
        if (0 == strcmp(name_list[i].name, name))
            return name_list[i].socket;
    return -1;
}

static int send_to_vm(char *data, int fd)
{
    if (write(fd, data, strlen(data)) < 0)
    {
        DBGprintf("send_to_vm \"%s\" to %x failed!\n", data, fd);
        return -1;
    }
    return 0;
}

void *scanner(void *unused)
{

    char buffer[1024];
    int socket;

    while (1)
    {
        bzero(buffer, sizeof(buffer));
        if (2 == scanf("%s %x", buffer, &socket)) {
            switch (buffer[0])
            {
            case 'q': // quit
                puts("Bye");
                exit(0);
            case 'c': // cmd
                send_to_vm(buffer, socket);
                break;
            case 'u':
                send_to_vm(buffer, socket);
                break;
            }

        }
    }
}

#define MAGIC "TRUST"

void *worker(void *connect_socket)
{
    char buffer[BUFFER_SIZE];
    char this_vmname[64]= {0};

    int conn_socket = *((int *)connect_socket);
    int length = 0;
    int ret;

    bzero(buffer, BUFFER_SIZE);
    while ((length = recv(conn_socket, buffer, BUFFER_SIZE, 0)) >= 0)
    {
        if (length == 0)
        {
            pthread_mutex_lock(&mutex_list);
            //ret = broadcast_del(this_vmname);
            pthread_mutex_unlock(&mutex_list);

            //if (ret)
            //    DBGprintf("Delete Failed but Thread Exits! NO Broadcasting!!!\n");
            goto exit;
        }

        if (0 == memcmp(MAGIC, buffer, strlen(MAGIC))) /* if it's not fake */
        {
            strcpy(this_vmname, buffer + strlen(MAGIC));

            pthread_mutex_lock(&mutex_list);
            ret = add_list(this_vmname, conn_socket);
            pthread_mutex_unlock(&mutex_list);

            //if (!ret)
            //    broadcast_add(this_vmname);
        }

        printf("0x%x says: %s\n", conn_socket, buffer);
        bzero(buffer, BUFFER_SIZE);
    }
exit:
    close(conn_socket);
    return NULL;
}

