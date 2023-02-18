#ifndef _SOCKET_H
#define _SOCKET_H
#include <arpa/inet.h>

//////////////////////服务端/////////////////////////////
// 绑定和监听
int setListen(int lfd, unsigned short port);
int acceptConn(int lfd, struct sockaddr_in* client_addr);


//////////////////////客户端/////////////////////////////
int connectToHost(int fd, const char* ip, unsigned short port);


//////////////////////公用/////////////////////////////
int createScoket();
int sendMsg(int fd, const char* msg, int len);
int recvMsg(int fd, char**msg);
int closeSocket(int fd);
#endif