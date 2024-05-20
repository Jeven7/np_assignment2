#include <calcLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "protocol.h"

#define PORT 1234
#define MAXDATASIZE 100
using namespace std;

int main(int argc, char *argv[])
{
  // 初始化参数
  int sockfd, num;
  char buf[MAXDATASIZE];
  int count = 0;
  struct sockaddr_in server, peer;
  calcMessage message;
  calcProtocol protocol;
  int sleepTime;

  char *Desthost;
  char *Destport;
  //Get argv
  if (argc < 2) {
		fprintf(stderr, "Please enter the correct form: <ip>:<port> \n");
		exit(1);
	}
	else {
		char* lastColon = strrchr(argv[1], ':');
		if (lastColon == NULL) {
			fprintf(stderr, "Invalid format. Please enter the correct form: <ip>:<port> \n");
			exit(1);
		}
		*lastColon = '\0';
		Desthost = argv[1];
		Destport = lastColon + 1;
    sleepTime = 0;
	}
  if (argc == 3)
  {
    sleepTime = atoi(argv[2]);
  }
	int port=atoi(Destport);
	printf("Host %s, and port %d.\n", Desthost, port);
	struct addrinfo hints, * servinfo, * p;
	struct sockaddr_storage connector_address; //Connectors address info
	socklen_t sin_size = sizeof(connector_address);
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //supports both IPv4 and IPv6
	hints.ai_socktype = SOCK_DGRAM;//Socket type is UDP
	hints.ai_flags = AI_PASSIVE;// Use passive mode for server

	if ((rv = getaddrinfo(NULL, Destport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo : %s \n", gai_strerror(rv));
		return 1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		//Get the address of the server, create a listening socket, and bind it to the specified address
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			//Create a socket based on the current address information struct, if the creation fails, print the error message and move on to the next struct
			perror("Server: socket");
			continue;
		}

		void* addr;
		if (p->ai_family == AF_INET) { // IPv4
			struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
			addr = &(ipv4->sin_addr);
		}
		else { // IPv6
			struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
			addr = &(ipv6->sin6_addr);
		}
		inet_ntop(p->ai_family, addr, Desthost, sizeof(Desthost));
		break;
	}
   //Create Socket Structure
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = inet_addr(Desthost);
  
  // 为socket设置接收超时， 超时时间为2s
  struct timeval timeout = {2, 0};
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));


    /* 初始化message 
    htons(), htonl(), ntohs(), ntohl() 进行大小端转换
    防止两台主机CPU不同导致字节顺序不同 (同一台主机可以不用加)
    htonl()--"Host to Network Long"
    ntohl()--"Network to Host Long"
    htons()--"Host to Network Short"
    ntohs()--"Network to Host Short"   
    */
    // message.version = htons(2);
    message.type = htons(22);
    message.message = htonl(0);
    message.protocol = htons(17);
    message.major_version = htons(1);
    message.minor_version = htons(0);

    // 强制转换message并发送报文
    sendto(sockfd, (char *)&message, sizeof(calcMessage), 0, (struct sockaddr *)&server, sizeof(server));
    printf("Send the calcMessage. Wait for server response\n");

    // 没有收到服务器回复，重发2次后断开连接
    while (count < 2)
    {
      num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&peer, &sin_size);
      if (num < 0)
      {
        count++;
        printf("Server no response. Resend %d times\n", count);
        sendto(sockfd, (char *)&message, sizeof(calcMessage), 0, (struct sockaddr *)&server, sizeof(server));
      }
      else
      {
        break;
      }
    }
    if (count == 2)
    {
      printf("Server timeout\n");
      close(sockfd);
      exit(0); 
      // break;
    }
    //如果支持该协议
    if (num == sizeof(calcProtocol))
    {
      // copy报文内容到protocol对象
      memcpy(&protocol, buf, num);
      // double flvalue1_rec=ntohl(protocol.flValue1);
      // double flvalue2_rec=ntohl(protocol.flValue2);
      // double flvaule1,flvalue2;
      // memcpy(&flvaule1, &flvalue1_rec, sizeof(double));
      // memcpy(&flvalue2, &flvalue2_rec, sizeof(double));
      printf("calcProtocol type=%d version=%d.%d id=%d arith=%d \n",ntohs(protocol.type),ntohs(protocol.major_version),ntohs(protocol.minor_version),ntohl(protocol.id), ntohl(protocol.arith));
      switch (ntohl(protocol.arith))
      {
      case 1:
        printf("add %d %d\n",ntohl(protocol.inValue1),ntohl(protocol.inValue2));
        protocol.inResult = htonl(ntohl(protocol.inValue1) + ntohl(protocol.inValue2));
        printf("%d\n",ntohl(protocol.inResult));
        break;
      case 2:
        printf("sub %d %d\n",ntohl(protocol.inValue1),ntohl(protocol.inValue2));
        protocol.inResult = htonl(ntohl(protocol.inValue1)- ntohl(protocol.inValue2));
        printf("%d\n",ntohl(protocol.inResult));
        break;
      case 3:
        printf("mul %d %d\n",ntohl(protocol.inValue1),ntohl(protocol.inValue2));
        protocol.inResult = htonl(ntohl(protocol.inValue1) * ntohl(protocol.inValue2));
        printf("%d\n",ntohl(protocol.inResult));
        break;
      case 4:
        printf("div %d %d\n",ntohl(protocol.inValue1),ntohl(protocol.inValue2));
        protocol.inResult = htonl(ntohl(protocol.inValue1) / ntohl(protocol.inValue2));
        printf("%d\n",ntohl(protocol.inResult));
        break;
      case 5:
        printf("fadd %lf %lf\n",protocol.flValue1,protocol.flValue2);
        protocol.flResult = protocol.flValue1+protocol.flValue2;
        printf("%lf\n",protocol.flResult);
        break;
      case 6:
        printf("fsub %lf %lf\n",protocol.flValue1,protocol.flValue2);
        protocol.flResult = protocol.flValue1-protocol.flValue2;
        printf("%lf\n",protocol.flResult);
        break;
      case 7:
        printf("fmul %lf %lf\n",protocol.flValue1,protocol.flValue2);
        protocol.flResult = protocol.flValue1*protocol.flValue2;
        break;
      case 8:
        printf("fdiv %lf %lf\n",protocol.flValue1,protocol.flValue2);
        protocol.flResult = protocol.flValue1/protocol.flValue2;
        printf("%lf\n",protocol.flResult);
        break;
      }
    
      sleep(sleepTime);
      printf("Send back a protocol to server\n");
      // 发送计算结果报文给服务器
      sendto(sockfd, (char *)&protocol, sizeof(calcProtocol), 0, (struct sockaddr *)&server, sizeof(server));
      printf("Wait for server response\n");
      count = 0;
      // 没有收到客户端消息则重传2次
      while (count < 2)
      {
        num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&peer, &sin_size);
        if (num < 0)
        {
          count++;
          printf("Server no response. Resend %d times\n", count);
          sendto(sockfd, (char *)&protocol, sizeof(calcProtocol), 0, (struct sockaddr *)&server, sizeof(server));
        }
        else
        {
          break;

        }
      }
      if (count == 2)
      {
        printf("Server timeout\n");
        close(sockfd);
        exit(0);
        // break;
      }
      count = 0;
      // // 判断报文是否符合接收要求
      if (num == sizeof(calcMessage))
      {
        // 将报文信息拷贝到message
        memcpy(&message, buf, num);
        if (ntohl(message.message) == 1)
        {
          printf("OK\n");
        }
        else if (ntohl(message.message) == 2)
        {
          printf("NOT OK\n");
        }
        else
        {
          printf("Server error\n");
        }
      }
      else
      {
        printf("Error server response\n");
        close(sockfd);
      }
    }
    else
    {
      printf("The protocol is not supported\n");
      close(sockfd);
      exit(1);
    }
  
  close(sockfd);
}