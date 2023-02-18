// 边缘触发模式的epoll模型，非阻塞的，单线程的
#include <sys/epoll.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h> // for fcntl()
#include <errno.h>
#include <pthread.h>

typedef struct SocketInfo {
    int fd;
    int epfd;
}SocketInfo;

// 用于接受客户端连接的子线程执行的函数
void* acceptConn(void* arg) {
    printf("acceptConn tid:%ld\n", pthread_self());
    SocketInfo* pinfo = (SocketInfo*) arg;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(pinfo->fd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    // 设置非阻塞属性
    int flag = fcntl(client_fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(client_fd, F_SETFL, flag);

    char ip[64] = {0};
    printf("客户端成功连接，IP是%s，端口是%d\n",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port));

    // 检测读缓冲区是否有数据，将文件描述符设置为边缘触发模式
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = client_fd;
    // 将用于通信的文件描述符添加到epoll模型中
    int ret = epoll_ctl(pinfo->epfd, EPOLL_CTL_ADD, client_fd, &event);
    if (ret == -1) {
        perror("epoll_ctl()error\n");
        close(pinfo->epfd);
        close(pinfo->fd);
        close(client_fd);
        if (pinfo) free(pinfo);
        return NULL;
    }
    if (pinfo) free(pinfo);
}

// 用于和客户端通信的子线程执行的函数
void* communicate(void* arg) {
    printf("communicate tid:%ld\n", pthread_self());
    SocketInfo* pinfo = (SocketInfo*) arg;
    char buff[4] = {0};
    int read_len;
    while (1) {
        // 当读缓冲区没有数据时，read不会阻塞
        read_len = read(pinfo->fd, buff, sizeof(buff));
        if (read_len == 0) {
            printf("客户端已经断开连接\n");
            epoll_ctl(pinfo->epfd, EPOLL_CTL_DEL, pinfo->fd, NULL);
            break;
        } else if (read_len == -1) {
            if (errno == EAGAIN) {
                printf("数据读取完毕\n");
                close(pinfo->fd);
                break;
            } else {
                perror("read()error\n");
                close(pinfo->epfd);
                close(pinfo->fd);
                if (pinfo) free(pinfo);
                return NULL;
            }
        } else {
            printf("客户端发送的数据:%s\n", buff);
            write(pinfo->fd, buff, read_len);
            memset(buff, 0, sizeof(buff));
        }
    }
    if (pinfo) free(pinfo);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage:%s <port>\n", argv[0]);
        return -1;
    }

    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket()error\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("bind()error\n");
        close(lfd);
        return -1;
    }

    if (listen(lfd, 128) == -1) {
        perror("listen()error\n");
        close(lfd);
        return -1;
    }

    // 创建epoll模型
    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create()error\n");
        close(lfd);
        return -1;
    }
    // 向epoll实例中添加需要检测的节点，现在是监听的套接字
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = lfd;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &event);
    if (ret == -1) {
        perror("epoll_ctl()error\n");
        close(epfd);
        close(lfd);
        return -1;
    }

    struct epoll_event evs[1024];
    int size = sizeof(evs) / sizeof(evs[0]);
    // 持续检测
    while (1) {
        int number = epoll_wait(epfd, evs, size, -1);
        for (int i = 0; i < number; ++i) {
            SocketInfo* pinfo = (SocketInfo*)malloc(sizeof(SocketInfo));
            // 用于监听的文件描述符的读缓冲区有数据表示有新的连接
            if (evs[i].data.fd == lfd) {
                pthread_t tid;
                pinfo->epfd = epfd;
                pinfo->fd = lfd;
                pthread_create(&tid, NULL, acceptConn, pinfo);
                pthread_detach(tid);
            } else {
                // 通信
                pthread_t tid;
                pinfo->epfd = epfd;
                pinfo->fd = evs[i].data.fd;
                pthread_create(&tid, NULL, communicate, pinfo);
                pthread_detach(tid);
            }
        }
    }

    close(epfd);
    close(lfd);
    return 0;
}