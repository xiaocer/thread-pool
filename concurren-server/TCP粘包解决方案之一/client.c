#include "socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage:%s <port>\n", argv[0]);
        return -1;
    }

    int fd = createScoket();
    if (fd == -1) {
        exit(-1);
    }

    int ret = connectToHost(fd, "127.0.0.1", atoi(argv[1]));
    if (ret == -1) {
        return -1;
    }

    int file_fd = open("data.txt", O_RDONLY);
    int length = 0;
    char buff[128] = {0};
    while ((length = read(file_fd, buff, rand() % 64)) > 0) {
        sendMsg(fd, buff, length);

        memset(buff, 0, sizeof(buff));
        // 每隔300微秒发送一次
        usleep(500);
    }

    sleep(20);
    closeSocket(fd);
    return 0;
}