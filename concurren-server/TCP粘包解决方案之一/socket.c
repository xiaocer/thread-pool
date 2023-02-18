#include "socket.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 创建监听的套接字
int createScoket() {
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket()error\n");
        return -1;
    }
    printf("创建套接字成功,fd=%d\n", lfd);
    return lfd;
}

// 绑定本地的IP地址和端口号、设置监听
int setListen(int lfd, unsigned short port) {
    // 设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    int ret = bind(lfd, (const struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        perror("bind()error\n");
        close(lfd);
        return -1;
    }
    printf("套接字绑定成功\n");
    ret = listen(lfd, 128);
    if (ret == -1) {
        perror("listen()error\n");
        close(lfd);
        return -1;
    }
    printf("设置监听成功\n");
    return ret;
}

// 阻塞并等待客户端的连接
int acceptConn(int lfd, struct sockaddr_in* client_addr) {
    int client_fd;
    if (client_addr == NULL) {
        client_fd = accept(lfd, NULL, NULL);
    } else {
        socklen_t len = sizeof(struct sockaddr);
        client_fd = accept(lfd, (struct sockaddr*)client_addr, &len);
    }
    
    if (client_fd == -1) {
        perror("accept()error\n");
        close(lfd);
        return -1;
    }
    char ip[64] = {0};
    printf("成功和客户端建立连接, 客户端IP为：%s,客户端端口号为：%d\n",
        inet_ntoa(client_addr->sin_addr),
        ntohs(client_addr->sin_port));
    return client_fd;
}

int connectToHost(int fd, const char* ip, unsigned short port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    inet_pton(AF_INET, ip, (void*)&addr.sin_addr.s_addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int ret = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect()error\n");
        close(fd);
        return -1;
    }
    printf("和服务器成功建立连接\n");
    return ret;
}

// 发送指定长度的字符串,需要发送size个字节
int writeN(int fd, const char* msg, int size) {
    int count = size;
    const char* buff = msg;
    while (count > 0) {
        int write_len = send(fd, buff, count, 0);
        if (write_len == -1) {
            return -1;
        } else if (write_len == 0) {
            continue;
        } else {
            buff += write_len;
            count -= write_len;
        }
    }
    return size;
}

int sendMsg(int fd, const char* msg, int len) {
    // 数据头(4个字节)+ 数据块
    const char* data = (const char*)malloc(len + 4);
    printf("要发送的数据块的长度：%d\n", len);
    int bigLen = htons(len);
    memcpy((void*)data, &bigLen, 4);
    memcpy((void*)data + 4, msg, len);
    int send_len = writeN(fd, data, len + 4);
    if (send_len == -1) {
        close(fd);
    }
    return send_len;
}

// 接收指定字节个数的字符串
int readN(int fd, char* buff, int size) {
    char* ptr = buff;
    int count = size;
    printf("size:%d\n", size);
    while (count > 0) {
        int read_len = recv(fd, ptr, count, 0);
        // printf("read_len:%d\n", read_len);
        if (read_len == -1) {
            return -1;
        } else if (read_len == 0) {
            return size - count; // 已读的字节
        } else {
            count -= read_len;
            ptr += read_len;
        }
    }
    return size;
}

// msg：传出参数
int recvMsg(int fd, char**msg) {
    // 读取4个字节的数据头
    unsigned int len = 0;
    readN(fd, (char*)&len, 4);
    len = ntohl(len);
    // BUG
    printf("要接收的数据块的长度为:%d\n", len);
    // + 1个字节存储字符结束标志
    char* data = (char*)malloc(len + 1);
    int ret = readN(fd, data, len);
    if (ret <= 0) {
        close(fd);
        free(data);
        return -1;
    } 
    data[len] = '\0';
    *msg = data;
    return ret;

}


int closeSocket(int fd) {
    int ret = close(fd);
    if (ret == -1) {
        perror("connect()error\n");
    }
    return ret;
}