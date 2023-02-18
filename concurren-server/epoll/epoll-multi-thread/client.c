#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("usage:%s <IP> <port>\n", argv[0]);
        return -1;
    }
    // 创建客户端套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket()error!");
        return -1;
    }

    // 发起连接请求
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret == -1)
    {
        printf("connect() error!\n");
        close(sockfd);
        return -1;
    }

    // 通信
    char buff[1024] = {0};
    int read_len, write_len;

    while (1) {
        fputs("input message(Q/q to quit)", stdout);
        fgets(buff, sizeof(buff), stdin);
        if (!strcmp(buff, "q\n") || !strcmp(buff, "Q\n")) {
            break;
        }
        write_len = write(sockfd, buff, strlen(buff));
        int count = 0;
        char* temp = buff;
        while (count < write_len) {
            read_len = read(sockfd, temp, sizeof(buff) - 1);
            printf("read_len:%d\n", read_len);
            if (read_len == -1) {
                close(sockfd);
                exit(0);
            } else if (read_len == 0) {
                close(sockfd);
                exit(0);
            }
            temp += read_len;
            count += read_len;
            
        }
        buff[count] = 0;
        printf("Message from server:%s\n", buff);
        
    }

    // 释放连接
    close(sockfd);
    return 0;
}