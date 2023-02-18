#include "socket.h"
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

struct SockInfo {
    pthread_t tid; // 子线程id
    struct sockaddr_in client_addr;     // 客户端地址信息
    int client_fd; // 用于和客户端通信的文件描述符
};
struct SockInfo infos[128];


void* work(void* arg) {
    struct SockInfo* info = (struct SockInfo*)arg;
    char ip[64] = {0};
    printf("客户端的IP：%s，端口号：%d\n",
        inet_ntoa(info->client_addr.sin_addr),
        ntohs(info->client_addr.sin_port));

    while (1) {
        char* buff;
        int len = recvMsg(info->client_fd, &buff);
        printf("接收数据...\n");
        if (len > 0) {
            printf("%s\n\n\n\n", buff);
            free(buff);
        } else {
            break;
        }
        sleep(1);
    }
    info->client_fd = -1;
    return NULL;
}
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage:%s <port>\n", argv[0]);
        return -1;
    }

    int lfd = createScoket();
    if (lfd == -1) {
        return -1;
    }

    int ret = setListen(lfd, atoi(argv[1]));
    if (ret == -1) {
        return -1;
    }

    int max = sizeof(infos) / sizeof(infos[0]);
    for (int i = 0; i < max; ++i) {
        bzero(&infos[i], sizeof(infos[i]));
        infos[i].client_fd = -1;
    }

    while (1) {
        struct SockInfo* pinfo;
        for (int i = 0; i < max; ++i) {
            if (infos[i].client_fd == -1) {
                pinfo = &infos[i];
                break;
            }
            if (i == max - 1) {
                --i; // 重试
            }
        }
        int client_fd = acceptConn(lfd, &pinfo->client_addr);
        if (client_fd == -1) {
            return -1;
        }
        pinfo->client_fd = client_fd;
        pthread_create(&pinfo->tid, NULL, work, pinfo);
        pthread_detach(pinfo->tid);
    }
    return 0;
}