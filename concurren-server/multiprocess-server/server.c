#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

int work(int client_fd, const char* ip, uint16_t port);
void recycle(int sig) {
    while(1)
    {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if(pid <= 0)
        {
            printf("子进程正在运行, 或者子进程被回收完毕了\n");
            break;
        }
        printf("child die, pid = %d\n", pid);
    }
}
int main() {
	int lfd = socket(PF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket() error\n");
		exit(-1);
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6666);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 设置端口可复用
	int opt = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

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

	struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = recycle;
    sigemptyset(&act.sa_mask);
    // 注册SIGCHLD信号产生的回调函数
	sigaction(SIGCHLD, &act, NULL);

	
	
	while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
		int client_fd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client_fd == -1) {
            if (errno == EINTR) {
                continue; // 重试
            }
            break;
        }
        // 客户端的地址信息
        char ip[64] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof(ip));
        uint16_t port = ntohs(client_addr.sin_port);
        printf("客户端的IP地址: %s, 端口: %d\n", ip, port);

        pid_t pid = fork();
        // 子进程
        if (pid == 0) {
            close(lfd); // 子进程不需要进行监听客户端的连接
            while (1) {
                // 通信
                int ret = work(client_fd, ip, port);
                if (ret <= 0) break;
            }
            // 子进程退出
            close(client_fd);
            exit(0);
            
        } else if (pid > 0) {
            // 父进程
            close(client_fd); // 父进程不需要和客户端通信
        }

	}

	return 0;
}

int work(int client_fd, const char* ip, uint16_t port) {
    char buff[1024];
    int read_len;
    memset(buff, 0, sizeof(buff));

    read_len = read(client_fd, buff, sizeof(buff));
    if (read_len == -1) {
        perror("read() error\n");
    } else if (read_len == 0) {
        printf("客户端断开了连接,ip:%s,port:%d", ip, port);
    } else {
        printf("从客户端接收到的消息:%s\n", buff);
        write(client_fd, buff, read_len);
    }
    return read_len;
}