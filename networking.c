// @filename networking.c
// Created by zhaozhang@yxlink.com
// on 2017/7/6
// Complie: gcc networking.c -o networking.cgi

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

int time_out = 6000; // 6秒

void echo_error(){
    printf("Content-type: text/plain; charset=iso-8859-1\r\n\r\n");
    printf("CGI_RESULT_FAILED\n");
}

int SetupTcpClientSocket(const char *host , const char *service){

	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_UNSPEC;
	addrCriteria.ai_socktype = SOCK_STREAM;
	addrCriteria.ai_protocol = IPPROTO_TCP;

	struct addrinfo *servAddr;
	int rtnVal = getaddrinfo(host, service, &addrCriteria, &servAddr);
	if(rtnVal != 0){
		perror("getaddrinfo() failed");
	}

	int sock = -1;
    struct addrinfo *addr;
	for(addr = servAddr; addr != NULL; addr = addr->ai_next){
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(sock < 0){
			continue;
		}
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&time_out,sizeof(time_out));
		if(connect(sock, addr->ai_addr, addr->ai_addrlen) == 0){
			break;
		}

		close(sock);
		sock = -1;
	}

	freeaddrinfo(servAddr);
	return sock;
}

int main(int argc, char *argv[]){
	char *server = "127.0.0.1";

	int sock = SetupTcpClientSocket(server, "6739");
	if(sock < 0){
        goto cgi_error;
	}
    char cmdString[] = "config network";
	size_t cmdStringLen = strlen(cmdString);
	ssize_t numBytes = send(sock, cmdString, cmdStringLen, 0);
	if(numBytes < 0){
        goto cgi_error;
	} else if(numBytes != cmdStringLen){
        goto cgi_error;
	}

    char buffer[256] = {0};
    numBytes  = recv(sock, buffer, sizeof(buffer), 0);
    if(numBytes != 1){
        goto cgi_error;
    }
    //printf("return result is :%d\n", buffer[0]);
    if(buffer[0] == 1){
        printf("Content-type: text/plain; charset=iso-8859-1\r\n\r\n");
        printf("CGI_RESULT_SUCCESS\n");
        close(sock);
        exit(0);
    }

cgi_error:
    echo_error();
	close(sock);
	exit(0);
}
