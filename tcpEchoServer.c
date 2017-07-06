// @filename tcpEchoServer.c
// Created by zhaozhang@yxlink.com
// on 2017/7/6

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
//#include <type.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFSIZE 1024
int MAXPENDING  = 5;  // Maxinum outstanding connection requests

void DieWithUserMessage(const char *msg, const char *detail){
		fputs(msg, stderr);
		fputs(":", stderr);
		fputs(detail, stderr);
		fputc('\n', stderr);
		exit(1);
}

void DieWithSystemMessage(const char *msg){
		perror(msg);
		exit(1);
}

void PrintSocketAddress(const struct sockaddr *address, FILE *stream){
	//Test for address and stream
	if(address == NULL || stream == NULL){
		return;
	}

	void *numericAddress;           // Pointer to binary address
	//Buffer to contain result (IPV6 sufficient to hold IPV4)
	char addrBuffer[INET6_ADDRSTRLEN];
	int port;
	//Set pointer to address based on address family
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
			fputs("[unknown type]", stream);        // Unhandled type
			return;
	}

	//Convert binary to printable address
	if(inet_ntop(address->sa_family, numericAddress, addrBuffer,
		sizeof(addrBuffer)) == NULL){
		fputs("[invalid address]", stream);          //Unable to convert
	}
	else {
		fprintf(stream, "%s", addrBuffer);
		if(port != 0){
			fprintf(stream, "-%u", port);
		}
	}
}
void HandleTcpClient(int clntSocket){
	char buffer[BUFSIZE];            //Buffer for echo string

	//Receive message from client
	ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
	if(numBytesRcvd < 0){
		DieWithSystemMessage("recv() failed");
	}
    printf("receive str is :%s\n", buffer);
	//Send received string and receive again until end of stream
	while(numBytesRcvd > 0){// 0 indicates end of stream
		//Echo message back to client
		ssize_t numByteSend = send(clntSocket, buffer, numBytesRcvd, 0);
        printf("send data len is:%d\n", numByteSend);
		if(numByteSend < 0){
			DieWithSystemMessage("send() failed");
		}
		else if(numByteSend != numBytesRcvd){
		        DieWithUserMessage("send()","sent unexpected number of bytes");
		}

		//see if there if more data to receive
		numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
		if(numBytesRcvd < 0){
			DieWithSystemMessage("recv() failed");
		}

		close(clntSocket);   //Close client socket
	}
}

int SetupTcpServerSocket(const char *service){
	//construct the server address structure
	struct addrinfo addrCriteria;        //Criteria for address match
	memset(&addrCriteria, 0, sizeof(addrCriteria)); //Zero out structure
	addrCriteria.ai_family = AF_UNSPEC;               //Any address family
	addrCriteria.ai_flags = AI_PASSIVE;                 //Accept on any  address/port
    addrCriteria.ai_socktype = SOCK_STREAM; // Only stream sockets
    addrCriteria.ai_protocol = IPPROTO_TCP; //Only TCP protocol

	struct addrinfo *servAddr;                  //List of server addresses
	int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
	if(rtnVal != 0){
		printf("getaddrinfo() failed");
        exit(1);
	}

	int servSock = -1;
    struct addrinfo *addr;
	for(addr = servAddr; addr != NULL; addr = addr->ai_next){
		//Create a Tcp socket
		servSock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
		if(servSock < 0){
			continue;   //Socket creation failed ; try next address
		}

		//Bind to the local address and set socket to list
		if((bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) ==0) && (listen(servSock, MAXPENDING) == 0)){
            //Print local address of socket
            struct sockaddr_storage localAddr;
			socklen_t addrSize = sizeof(localAddr);
			if(getsockname(servSock,(struct sockaddr *)&localAddr, &addrSize) < 0){
				DieWithSystemMessage("getsockname() failed");
			}
			fputs("Binding to ", stdout);
			PrintSocketAddress((struct sockaddr *)&localAddr, stdout);
			fputc('\n',stdout);
			break;            //Bind and list successful
		}

		close(servSock);    //Close and try again
		servSock = -1;
	}

	//Free address list allocated by getaddrinfo()
	freeaddrinfo(servAddr);

	return servSock;
}     

int AcceptTcpConnection(int servSock){
	struct sockaddr_storage clntAddr;      //Client address
	//Set length of client address structure (in-out parameter)
	socklen_t clntAddrLen = sizeof(clntAddr);

	//Wait for a client to connect
	int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
	if(clntSock < 0){
		DieWithSystemMessage("accept() failed");
	}

	//clntSock is connect to client!
	fputs("Handling client ", stdout);
	PrintSocketAddress((struct sockaddr *)&clntAddr, stdout);
	fputc('\n', stdout);

	return clntSock;
}

int main(int argc, char *argv[]){
	if(argc !=2){ //Test for correct number of arguments
		DieWithUserMessage("Parameter(s)", "<Server Port/Service>");
	}

	char *service = argv[1];  //First arg: local port
	//Create socket for incoming connections
	int servSock = SetupTcpServerSocket(service);
	if(servSock < 0){
		DieWithUserMessage("SetupTCPServerSocket() failed", service);
	}

	for(; ;){
		//New connection creates a connected client socket
		int clntSock = AcceptTcpConnection(servSock);

		HandleTcpClient(clntSock);
		close(clntSock);
	}
	//NOT REACHED
}
