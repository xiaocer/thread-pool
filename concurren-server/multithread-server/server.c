#include <stdio.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct SockInfo {
    int fd;         // 通信描述符
    pthread_t tid;  // 子线程id
    struct sockaddr_in client_addr;     // 客户端地址信息
};
struct SockInfo infos[128];

void* work(void* arg) {
    struct SockInfo* info = (struct SockInfo*)arg;
    while (1) {
        char ip[64] = {0};
        char buff[1024] = {0};
        int read_len = read(info->fd, buff, sizeof(buff));
        if (read_len == 0) {
            printf("客户端已经关闭连接, 其使用的文件描述符是:%d，ip是:%s，端口是:%d\n",
                info->fd,
                inet_ntoa(info->client_addr.sin_addr),
                ntohs(info->client_addr.sin_port));
            info->fd = -1;
            close(info->fd);
            break;
        } else if (read_len == -1) {
            printf("接收客户端的数据失败\n");
            info->fd = -1;
            close(info->fd);
            break;
        } else {
            write(info->fd, buff, strlen(buff) + 1);
        }
        
    }
    return NULL;
}
int main() {
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket()error!\n");
        exit(-1);
    }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8999);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind() error\n");
        close(lfd);
        exit(-1);
    }

    if (listen(lfd, 128) == -1) {
        perror("listen() error\n");
        close(lfd);
        exit(-1);
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int max = sizeof(infos) / sizeof(infos[1]);
    for (int i = 0; i < max; i++) {
        infos[i].fd = -1;
        infos[i].tid = -1;
    }
    while (1) {

        int client_fd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept() error\n");
            close(lfd);
            exit(-1);
        }
        printf("主线程，client_fd：%d\n", client_fd);
        // 创建子线程用于和客户端IO
        struct SockInfo* pinfo;
        for (int i = 0; i < max; i++) {
            // 尚未使用的fd
            if (infos[i].fd == -1) {
                pinfo = &infos[i];
                break;
            }
            if (i == max - 1) {
                // 重试
                i--;
            }
        }
        pinfo->fd = client_fd;
        pinfo->client_addr = client_addr;
        pthread_create(&pinfo->tid, NULL, work, pinfo);
        // 线程分离
        pthread_detach(pinfo->tid);
    }

    close(lfd);
    return 0;
}