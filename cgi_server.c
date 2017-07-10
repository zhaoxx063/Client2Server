// @filename cgi_server.c
// Created by zhaozhang@yxlink.com
// on 2017/7/6
// Complie: gcc cgi_server.c -o cgi_server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>

int MAXPENDING  = 5;
char SERVER_PORT[] = "6739";
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

int daemon_init(void)
{
	pid_t pid;
	int i;

	if ((pid = fork()) < 0)  // fork error
		return(-1);
	else if(pid != 0)
		exit(0); /* parent exit */


	/* child continues */
	setsid(); /* become session leader */


	if ((pid=fork()) < 0) //fork error
		return(-1);
	else if (pid!=0)
		exit(0);  // the first child exit

	/* the child of the first child */

	for (i=0; i < NOFILE; i++) // because a lot of codes still use stdout, stderr,  keep them open....
		close(i);

	int fd;
	// redirect the stdin, stdout, stderr to /dev/null
	fd = open ("/dev/null", O_RDWR, 0);
	if (fd != -1)
	{
		dup2 (fd, STDIN_FILENO);
		dup2 (fd, STDOUT_FILENO);
		dup2 (fd, STDERR_FILENO);

		if (fd > 2)
			close (fd);
	}

	chdir("/"); /* change working directory */
	umask(0); /* clear file mode creation mask */

	return(0);
}

int is_already_running(const char* process_name)
{
	char lock_file[128];
	snprintf(lock_file, sizeof(lock_file), "/var/tmp/%s.lock",process_name);

	int lock_fd = open(lock_file, O_RDWR|O_CREAT, LOCKMODE);
	if(lock_fd < 0){
		perror("open lock_file error.");
		return -1;
	}

	if(flock(lock_fd, LOCK_EX|LOCK_NB) == 0){
		return 0;
	}

	close(lock_fd);
	return -1;

}

void PrintSocketAddress(const struct sockaddr *address, FILE *stream){
	if(address == NULL || stream == NULL){
		return;
	}

	void *numericAddress;
	char addrBuffer[INET6_ADDRSTRLEN];
	int port;
	switch(address->sa_family){
		case AF_INET:
			numericAddress = &((struct sockaddr_in *)address)->sin_addr;
			port = ntohs(((struct sockaddr_in *)address)->sin_port);
			break;
		case AF_INET6:
			numericAddress = &((struct sockaddr_in6 *)address)->sin6_addr;
			port = ntohs(((struct sockaddr_in6 *)address)->sin6_port);
			break;
		default:
			fprintf(stream, "[unknown type]");
			return;
	}

	if(inet_ntop(address->sa_family, numericAddress, addrBuffer, sizeof(addrBuffer)) < 0){
		fprintf(stream, "[invalid address]");
	} else {
		fprintf(stream, "%s", addrBuffer);
		if(port != 0){
			fprintf(stream, "-%u", port);
		}
	}
}

void HandleTcpClient(int clntSocket){
	char buffer[1024] = {0};
	char result = 0;

	ssize_t numBytesRcvd = recv(clntSocket, buffer, sizeof(buffer), 0);
	if(numBytesRcvd < 0){
		perror("recv() failed");
	}

	if(strcmp(buffer, "config network") == 0){
		char cmd[] = "/usr/bin/python /var/waf/network.py";
		FILE *fp = popen(cmd, "r");
		char buf[256] = {0};
		if(fp != NULL){
			if(fgets(buf, sizeof(buf), fp) != NULL){
//				printf("buf is:%s\n", buf);
				if(strlen(buf) > 0){
					result = atoi(buf);
					send(clntSocket, &result, sizeof(result), 0);
//					printf("send result is: %d\n", result);
				}
			}
		}
		pclose(fp);
	} //else if(strcmp(buffer, "xxx") == 0){
	//  TDD: more system command to be execute.
	//}
	close(clntSocket);
}

int SetupTcpServerSocket(const char *service){

	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_UNSPEC;
	addrCriteria.ai_flags = AI_PASSIVE;
    addrCriteria.ai_socktype = SOCK_STREAM;
    addrCriteria.ai_protocol = IPPROTO_TCP;

	struct addrinfo *servAddr;
	int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
	if(rtnVal != 0){
		perror("getaddrinfo() failed");
        exit(1);
	}

	int servSock = -1;
    struct addrinfo *addr;
	for(addr = servAddr; addr != NULL; addr = addr->ai_next){

		servSock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
		if(servSock < 0){
			continue;
		}
		int flag = 1;
		int len = sizeof(int);
		if(setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &flag, len) == -1)
		{
			perror("setsockopt");
			exit(1);
		}

		if((bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) ==0) && (listen(servSock, MAXPENDING) == 0)){
            struct sockaddr_storage localAddr;
			socklen_t addrSize = sizeof(localAddr);
			if(getsockname(servSock,(struct sockaddr *)&localAddr, &addrSize) < 0){
				perror("getsockname() failed");
			}
//			fprintf(stdout, "Binding to \n");
//			PrintSocketAddress((struct sockaddr *)&localAddr, stdout);
			break;
		}

		close(servSock);
		servSock = -1;
	}
	freeaddrinfo(servAddr);

	return servSock;
}     

int AcceptTcpConnection(int servSock){
	struct sockaddr_storage clntAddr;
	socklen_t clntAddrLen = sizeof(clntAddr);

	int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
	if(clntSock < 0){
		perror("accept() failed");
	}

//	fprintf(stdout, "Handling client \n");
//	PrintSocketAddress((struct sockaddr *)&clntAddr, stdout);

	return clntSock;
}

int main(int argc, char *argv[]){

	char *service = SERVER_PORT;
	if (daemon_init() == -1){
		printf("can't fork self\n");
		exit(0);
	}

    if(is_already_running(argv[0]) != 0){
        fprintf(stdout, "%s process is already exists.\n", argv[0]);
        exit(-1);
    }

	int servSock = SetupTcpServerSocket(service);
	if(servSock < 0){
		fprintf(stderr, "SetupTCPServerSocket() failed");
	}

	for(; ;){
		int clntSock = AcceptTcpConnection(servSock);

		HandleTcpClient(clntSock);
		close(clntSock);
	}
}
