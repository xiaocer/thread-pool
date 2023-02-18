#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage:%s <port>\n", argv[0]);
        return -1;
    }
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket() error\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind() error\n");
        close(lfd);
        return -1;
    }

    if (listen(lfd, 128) == -1) {
        perror("listen() error\n");
        close(lfd);
        return -1;
    }

    struct pollfd fdset[1024];
    for (int i = 0; i < 1024; ++i) {
        fdset[i].events = POLLIN;
        fdset[i].fd = -1;
    }
    fdset[0].fd = lfd;
    int maxFd = 0;

    while (1) {
        // 委托内核检测
        int ret = poll(fdset, maxFd + 1, -1);
        if (ret == -1) {
            perror("poll()error\n");
            break;
        }

        // 检测的读缓冲区有变化，则表示存在新连接
        if (fdset[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
            
            printf("sizeof(fdset) / sizeof(fdset[0]):%ld", sizeof(fdset) / sizeof(fdset[0]));
            

            char ip[64] = {0};
            printf("客户端成功连接：ip是%s，端口是%d\n",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));

            // 委托内核检测client_fd的读缓冲区
            int i;
            for (i = 1; i < sizeof(fdset) / sizeof(fdset[0]); ++i) {
                // 未经使用的文件描述符
                if (fdset[i].fd == -1) {
                    fdset[i].fd = client_fd;
                    fdset[i].events = POLLIN;
                    break;
                }
            }
            // 更新maxFd的值
            maxFd = i > maxFd ? i : maxFd;
        }

        // 通信
        for (int i = 1; i <= maxFd; ++i) {
            if (fdset[i].revents & POLLIN) {
                char buff[128] = {0};
                int read_len = read(fdset[i].fd, buff, sizeof(buff));
                if (read_len == -1) {
                    perror("read()error\n");
                    close(fdset[i].fd);
                    return -1;
                } else if (read_len == 0) {
                    printf("客户端已经断开连接\n");
                    fdset[i].fd = -1;
                    close(fdset[i].fd);
                    
                } else {
                    printf("客户端发送的消息:%s\n", buff);
                    write(fdset[i].fd, buff, strlen(buff) + 1);
                }
            }
        }
    }

    close(lfd);
    return 0;
}