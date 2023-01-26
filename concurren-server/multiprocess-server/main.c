#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main() {
	int lfd = socket(PF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket() error\n");
		exit(-1);
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(9999);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 设置端口可复用
	int opt = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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
	sigaction(SIGCHLD, &act, NULL);
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	while (1) {
		int client_fd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
		
	}
	return 0;
}