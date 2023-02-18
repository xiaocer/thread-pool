// 基于select实现并发服务器

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage:%s <port>\n", argv[0]);
        return -1;
    }

    // 创建用于监听的套接字
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

    
    // 初始化待检测的读集合
    fd_set rdset;
    // 由于select的第二参数是传入传出参数，会改变fd_set中的数据
    fd_set temp;
    // 清零
    FD_ZERO(&rdset);
    // 将用于监听的套接字的状态检测委托给内核检测，主要是读缓冲区
    FD_SET(lfd, &rdset);

    int maxFd = lfd;

    // 通过select委托内核检测读集合中的文件描述符状态，检测读缓冲区中有没有数据
    // 如果有数据则select解除阻塞返回。应该让内核持续监测。

    while (1) {
        temp = rdset;
        int number = select(maxFd + 1, &temp, NULL, NULL, NULL);
        // select解除阻塞返回后，temp中的数据将被内核改写
        // 只保留了发生变化的文件描述的标志位上的1, 没变化的改为0
        // 只要temp中的fd对应的标志位为1，则表示有数据了

        // 判断有没有新连接
        if (FD_ISSET(lfd, &temp)) {
            // 接受连接请求，这个调用一定不会阻塞
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(lfd, (struct sockaddr*)&client_addr, &addr_len);
            
            char ip[64] = {0};
            printf("客户端IP：%s，端口号：%d，成功连接\n",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));
            
            // 将通信的文件描述符加入到下一轮select检测
            FD_SET(client_fd, &rdset);
            // 重置最大的文件描述符
            maxFd = client_fd > maxFd ? client_fd : maxFd;
            
        }
        // 判断是否可以进行通信
        for (int i = 0; i <= maxFd; ++i) {
            if (i != lfd && FD_ISSET(i, &temp)) {
                // 接收数据
                char buff[1024] = {0};
                int read_len = read(i, buff, sizeof(buff));
                if (read_len == -1) {
                    close(i);
                    break;
                } else if (read_len == 0) {
                    printf("客户端已经断开连接\n");
                    // 将检测的文件描述符从读集合中删除
                    FD_CLR(i, &rdset);
                    close(i);
                } else {
                    write(i, buff, strlen(buff) + 1);
                }
            }
        }
    }

    close(lfd);
    return 0;
}