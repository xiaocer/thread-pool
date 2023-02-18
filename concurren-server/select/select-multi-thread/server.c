#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

// 互斥锁
pthread_mutex_t mutex;

// 封装：传递给子线程的数据
struct FdInfo {
    int fd;     // 用于通信或者监听的文件描述符
    int* maxFd; // 子线程需要更改最大的文件描述符
    fd_set* rdset;  // 读集合
}fdInfo;

// 和客户端建立连接的子线程执行函数
void* acceptConn(void* arg) {
    printf("子线程线程id:%ld\n", pthread_self());
    struct FdInfo* pinfo = (struct FdInfo*)arg; 
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(pinfo->fd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    pthread_mutex_lock(&mutex);
    FD_SET(client_fd, pinfo->rdset);
    *pinfo->maxFd = client_fd > *pinfo->maxFd ? client_fd : *pinfo->maxFd;
    pthread_mutex_unlock(&mutex);

    char ip[64] = {0};
    printf("客户端IP：%s，端口号：%d，成功连接\n",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port));

    if (pinfo) free(pinfo);
    return NULL; 
} 

// 和客户端进行通信的子线程执行函数
void* communicate(void* arg) {
    printf("子线程线程id:%ld\n", pthread_self());
    struct FdInfo* pinfo = (struct FdInfo*)arg;
    char buff[1024] = {0};
    int read_len = read(pinfo->fd, buff, sizeof(buff));
    if (read_len == -1) {
        close(pinfo->fd);
        if (pinfo) free(pinfo);
        return NULL;
    } else if (read_len == 0) {
        printf("客户端已经断开连接\n");

        pthread_mutex_lock(&mutex);
        FD_CLR(pinfo->fd, pinfo->rdset);
        pthread_mutex_unlock(&mutex);

        close(pinfo->fd);
        if (pinfo) free(pinfo);
        return NULL;
    } else {
        printf("服务端接收到的数据:%s\n", buff);
        write(pinfo->fd, buff, strlen(buff) + 1);
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
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));
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

    fd_set rdset, temp;
    FD_ZERO(&rdset);
    FD_SET(lfd, &rdset);
    int maxFd = lfd;

    pthread_mutex_init(&mutex, NULL);

    while (1) {
        pthread_mutex_lock(&mutex);
        temp = rdset;
        pthread_mutex_unlock(&mutex);
        int number = select(maxFd + 1, &temp, NULL, NULL, NULL);
        // 判断有没有新连接
        if (FD_ISSET(lfd, &temp)) {
            pthread_t tid;
            struct FdInfo* pinfo = (struct FdInfo*)malloc(sizeof(struct FdInfo));
            pinfo->fd = lfd;
            pinfo->maxFd = &maxFd;
            pinfo->rdset = &rdset;
            pthread_create(&tid, NULL, acceptConn, pinfo);
            pthread_detach(tid);
        }
        // 判断是否可以进行通信
        for (int i = 0; i <= maxFd; ++i) {
            if (i != lfd && FD_ISSET(i, &temp)) {
                struct FdInfo* pinfo = (struct FdInfo*)malloc(sizeof(struct FdInfo));
                pinfo->fd = i;
                pinfo->rdset = &rdset;
                pthread_t tid;
                pthread_create(&tid, NULL, communicate, pinfo);
                pthread_detach(tid);
            }
        }
    }

    pthread_mutex_destroy(&mutex);

    close(lfd);
    return 0;
}

// BUG: