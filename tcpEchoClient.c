#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/type.h>
#include <sys/socket.h>
#include <netdb.h>

int SetupTcpClientSocket(const char *host , const char *service){
	//Tell the system what kind(s) of address info we want 
	struct addrinfo addrCriteria;              //Criteria for address match
	memset(&addrCriteria, 0, sizeof(addrCriteria));   //Zero out structrue
	addrCriteria.ai_family = AF_UNSPEC;                 // v4 or v6 is Ok
	addrCriteria.ai_socktype = SOCK_STREAM;       // Only streaming sockets
	addrCriteria.ai_protocol = IPPROTO_TCP;          // Only TCP protocol

	//Get address(es)
	struct addrinfo *servAddr;                   //Holder for returned list of  server addrs
	int rtnVal = getaddrinfo(host, service, &addrCriteria, &servAddr);
	if(rtnVal != 0){
		DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));
	}

	int sock = -1;
	for(struct addrinfo *addr = servAddr; addr != NULL; addr = addr->ai_next){
		//Create a reliable , stream socket using TCP
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(sock < 0){
			continue;       //Socket creation failed ; try next address
		}

		//Establish the connection to the echo server
		if(connect(sock, addr->ai_addr, addr->ai_addrlen) == 0){
			break;          // Socket connection succeded; break and return  socket
		}

		close(sock);         //Socket connection failed; try next address
		sock = -1;
	}

	freeaddrinfo(servAddr);  //Free addrinfo allocated in getaddrinfo()
	return sock;
}

int main(int argc, char *argv[]){
	if(argc < 3 || argv >4){
		DieWithUserMessage("Parameter(s)", "<Server Address/Name> <Echo Word> 
		[<Server Port/Service>]");
	}

	char *server = argc[1];                    //First arg: server address/name
	char *echoString = argc[2];             // Second arg: string to echo
	//Third arg(optional): server port/service
	char *service = (argc == 4) ? argv[3] : "echo";

	//Create a connect TCP socket
	int sock = SetupTcpClientSocket(server, service);
	if(sock < 0){
		DieWithUserMessage("SetupTcpClientSocket() failed", "unable to connect");
	}

	size_t echoStringLen = strlen(echoString);     //Dtermine input length

	//Send the string to server 
	ssize_t numBytes = send(sock, echoString, echoStringLen, 0);
	if(numBytes < 0){
		DieWithSystemMessage("send() failed");
	}
	else if(numBytes != echoStringLen){
		DieWithUserMessage("send()", "sent unexpected number of bytes");
	}

	//Receive the same string back from the server
	unsigned int totalBytesRcvd = 0;      //Count of total bytes received
	fputs("Received: ", stdout);               //Setup to print the echoed  string
	while(totalBytesRcvd < echoStringLen){
		char buffer[BUFSIZE];           // I/O buffer
		//Receive up to the buffer size (minus 1 to leave space for
		// a null terminator) bytes from the sender
                numBytes  = recv(sock, buffer, BUFSIZE -1, 0);
		if(numBytes < 0){
			DieWithSystemMessage("recv() failed");
		}
		else if(numBytes = 0){
			DieWithUserMessage("recv()", "connection closed prematurely");
		}
		totalBytesRcvd += numBytes;       //Keep tally of total bytes
		buffer[numBytes] = '\0';               // Terminate the string!
		fputs(buffer, stdout);
	}

	fputc('\n', stdout);                             //Print a final linefeed

	close(sock);
	exit(0);
}
