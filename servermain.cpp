#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <exception>
#include <netdb.h>
#include <map>
#include <mutex>
#include <calcLib.h>
#include "protocol.h"

using namespace std;

#define MAXDATASIZE 100
#define DEBUG

/* You will to add includes here */

// Included to get the support library

// 创建互斥锁
std::mutex mtx;
/* Needs to be global, to be rechable by callback and main */
int loopCount = 0;     // 记录有多久没有收到客户端的报文了
int id = 1;          // sessoin id
int flag = 0;          // 是否在处理客户端报文flag
map<int, int> session; // 记录对各个session id客户端的等待时间

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobList(int signum)
{
  // 当前没有报文正在处理，记录时间
  if (flag == 0)
  {
    loopCount++;
  }

  //修改session，上互斥锁
  mtx.lock();
  //遍历session
  for (auto it = session.begin(); it != session.end();)
  {
    // 等待时间 ++
    it->second++;
    // 判断这个客户端是否超时, 超时则从session中删除
    if (it->second >= 10)
    {
      it = session.erase(it);
      printf("Client timeout. The client id= %d is lost.\n",it->first+1);
    }
    else
      ++it;
  }
  // 释放互斥锁
  mtx.unlock();

  // 10s没收到报文了，抱怨一下 (一个服务器的时候我孤独啊)
  // if (loopCount >= 10)
  // {
  //   printf("Wait for client`s message.\n");
  //   loopCount = 0;
  // }
  return;
}

int main(int argc, char *argv[])
{
  // 初始化定时器，每1s给signal发送一个信号，执行checkJobList函数
  struct itimerval alarmTime;
  alarmTime.it_interval.tv_sec = 1;
  alarmTime.it_interval.tv_usec = 0;
  alarmTime.it_value.tv_sec = 1;
  alarmTime.it_value.tv_usec = 0;

  /* Regiter a callback function, associated with the SIGALRM signal, which will be raised when the alarm goes of */
  signal(SIGALRM, checkJobList);
  setitimer(ITIMER_REAL, &alarmTime, NULL); // Start/register the alarm.

  // 初始化参数
  int sockfd;
  struct addrinfo server;
  struct sockaddr_storage client;
  socklen_t addrlen;
  int num;
  calcMessage message;
  calcMessage send_message;
  calcProtocol protocol;
  calcProtocol protocolRes;
  char *Desthost;
  char *Destport;
  char *op;
  struct addrinfo *servinfo, *p;
  int session_id;
  char buf[MAXDATASIZE];
  addrlen = sizeof(client);
  int rv;
  int choice = 1;// Integer for setting socket option
  int iv1,iv2;
  double fv1,fv2;
  int inResult;
  double flResult;
  char client_ip[INET6_ADDRSTRLEN]; // Assuming INET6_ADDRSTRLEN is defined somewhere
  
  if(argc != 2){
    fprintf(stderr, "Please enter the correct form: <ip>:<port> \n");
    exit(1);
  }
  else{
    char *lastColon = strrchr(argv[1], ':'); 
    if(lastColon == NULL) {
        fprintf(stderr, "Invalid format. Please enter the correct form: <ip>:<port> \n");
        exit(1);
    }

    *lastColon = '\0'; 
    Desthost = argv[1];
    Destport = lastColon + 1; 
  }
  char *Desthost_1 = strdup(Desthost);
  int port=atoi(Destport);
  #ifdef DEBUG  
      printf("Host %s, and port %d.\n",Desthost,port);
  #endif

    // 初始化服务器信息
  memset(&server, 0, sizeof(server));
  server.ai_family = AF_UNSPEC; //supports both IPv4 and IPv6
  server.ai_socktype = SOCK_DGRAM;//Socket type is UDP
  server.ai_flags = AI_PASSIVE;// Use passive mode for server

  if((rv = getaddrinfo(NULL, Destport, &server, &servinfo)) != 0){
    fprintf(stderr, "getaddrinfo : %s \n", gai_strerror(rv));
    return 1;
  }

  for(p = servinfo; p != NULL; p = p->ai_next){
    //Get the address of the server, create a listening socket, and bind it to the specified address
    if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
      //Create a socket based on the current address information struct, if the creation fails, print the error message and move on to the next struct
      perror("Server: socket");
      continue;
    }
    if((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &choice, sizeof(int))) == -1){
      //Set the socket option to allow rebinding of addresses. If the setup fails, an error message is printed and the program is exited.
      perror("setsockopt ");
      exit(1);
    }
    if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
      //Bind the socket to the specified address
      close(sockfd);
      perror("Server: bind");
      continue;
    }
    void *addr;
    if (p->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        addr = &(ipv4->sin_addr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
        addr = &(ipv6->sin6_addr);
    }
    inet_ntop(p->ai_family, addr, Desthost, sizeof(Desthost));
    break;
  }
  freeaddrinfo(servinfo);

  // 一直等待接收来自客户端的请求
  while ((num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&client, &addrlen)))
  {
    if (num < 0)
    {
      printf("Bad receive\n");
      break;
    }
    else
    {
      flag = 1;
      // printf("Get a message form client\n");
      if (num == sizeof(calcMessage))
      {
        // 创建一个session id
        session_id = id;
        // if (client.ss_family == AF_INET6) {
        //   struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&client;
        //   inet_ntop(AF_INET6, &(addr_in6->sin6_addr), client_ip, INET6_ADDRSTRLEN);
        //   printf("The message type is calcMessage from IPv6 client %s:%d id=%d\n", client_ip, ntohs(addr_in6->sin6_port), session_id);
        // } else if (client.ss_family == AF_INET) {
        //   struct sockaddr_in *addr_in = (struct sockaddr_in *)&client;
        //   inet_ntop(AF_INET, &(addr_in->sin_addr), client_ip, INET_ADDRSTRLEN);
        //   printf("The message type is calcMessage from IPv4 client %s:%d id=%d\n", client_ip, ntohs(addr_in->sin_port), session_id);
        // }
        printf("The message type is calcMessage from client %s:%d id=%d\n",Desthost_1,ntohs(((struct sockaddr_in *)&client)->sin_port),session_id);
        // copy报文内容到message对象
        memcpy(&message, buf, num);
        printf("calcMessage type=%d message=%d.%d message=%d protocol=%d \n",ntohs(message.type),ntohs(message.major_version),ntohs(message.minor_version),ntohl(message.message), ntohs(message.protocol));
        if (ntohs(message.type) == 22 && ntohl(message.message) == 0 && ntohs(message.protocol) == 17 && ntohs(message.major_version) == 1 && ntohs(message.minor_version) == 0)
        {
          
        }
        else
        {
          send_message.type = htons(2);
          send_message.message = htonl(2);
          send_message.major_version = htons(1);
          send_message.minor_version = htons(0);
          printf("The client's protocol is not supported. ");
          sendto(sockfd, (char *)&send_message, sizeof(calcMessage), 0, (struct sockaddr *)&client, addrlen);
          flag = 0;
          loopCount = 0;
          continue;
        }
        // 将session id 放入session
        session.insert(make_pair(session_id, 0));
        // 初始化那个随机生成数组的类, 开始初始化protocol对象
        initCalcLib();
        loopCount = 0;
        op = randomType();
        iv1 = randomInt();
        iv2 = randomInt();
        fv1=randomFloat();
        fv2=randomFloat();
        if (op[0] == 'f')
        {
          if(strcmp(op,"fadd")==0){
            protocol.arith = htonl(5);
            flResult=fv1+fv2;
          } 
          else if (strcmp(op, "fsub")==0){
            protocol.arith = htonl(6);
            flResult=fv1-fv2;
          } 
          else if (strcmp(op, "fmul")==0){
            protocol.arith = htonl(7);
            flResult=fv1*fv2;
          } 
          else if (strcmp(op, "fdiv")==0){
            protocol.arith = htonl(8);
            flResult=fv1/fv2;
          }
          protocol.flValue1 = fv1;
          protocol.flValue2 = fv2;
          printf("%s %8.8lf %8.8lf\n",op,fv1,fv2);
        }
        else
        {
          if(strcmp(op,"add")==0){
            protocol.arith = htonl(1);
            inResult=iv1+iv2;
          }
          else if (strcmp(op, "sub")==0){
            protocol.arith = htonl(2);
            inResult=iv1-iv2;
          }
          else if (strcmp(op, "mul")==0){
            protocol.arith = htonl(3);
            inResult=iv1*iv2;
          }
          else if (strcmp(op, "div")==0){
            protocol.arith = htonl(4);
            inResult=iv1/iv2;
          }
          // protocol.arith = htonl(1 + rand() % 4);
          protocol.inValue1 = htonl(iv1);
          protocol.inValue2 = htonl(iv2);
          printf("%s %d %d\n",op,iv1,iv2);
        }
        protocol.type = htons(1);
        protocol.major_version = htons(1);
        protocol.minor_version = htons(0);
        protocol.id = htonl(id);
        id++;
        printf("Send protocol to the client\n");
        sendto(sockfd, (char *)&protocol, sizeof(calcProtocol), 0, (struct sockaddr *)&client, addrlen);
        printf("Wait for client response\n");
      }
      else if (num == sizeof(calcProtocol))
      {
        memcpy(&protocolRes, buf, num);
        session_id = htonl(protocolRes.id);
        printf("The message type is calcProtocol from client %s:%d id=%d\n",Desthost_1,ntohs(((struct sockaddr_in *)&client)->sin_port),session_id);
        printf("calcProtocol type=%d version=%d.%d id=%d arith=%d \n",ntohs(protocolRes.type),ntohs(protocolRes.major_version),ntohs(protocolRes.minor_version),ntohl(protocolRes.id), ntohl(protocolRes.arith));
        // 读取来自客户端的session id
        
        // 判断这个客户端是否连接超时
        if (session.count(session_id) == 0)
        {
          printf("The client id=%d is already lost. The message is rejected.\n",session_id);
          flag = 0;
          loopCount = 0;
          continue;
        }
        // 收到信息 耐心恢复至0
        session[session_id] = 0;
        
        // 开始计算客户端算的对不对，并发送判断结果到客户端
        printf("Start checking\n");
        if ((ntohl(protocolRes.arith) <= 4 && ntohl(protocolRes.inResult) == ntohl(htonl(inResult))) ||
            (ntohl(protocolRes.arith) > 4 && protocolRes.flResult == flResult))
        {
          message.type = htons(2);
          message.message = htonl(1);
          message.major_version = htons(1);
          message.minor_version = htons(0);
          message.protocol = htons(17);
          sendto(sockfd, (char *)&message, sizeof(calcMessage), 0, (struct sockaddr *)&client, addrlen);
          if(ntohl(protocolRes.arith) <= 4 ){
            printf("Got anser %d Expected answer: %d \n", ntohl(protocolRes.inResult), inResult);
          }
          else{
            printf("Got anser %lf Expected answer: %lf \n", protocolRes.flResult, flResult);
          }
          printf("The answer is right\n");
        }
        else
        {
          message.type = htons(2);
          message.message = htonl(2);
          message.major_version = htons(1);
          message.minor_version = htons(0);
          message.protocol = htons(17);
          sendto(sockfd, (char *)&message, sizeof(calcMessage), 0, (struct sockaddr *)&client, addrlen);
          if(ntohl(protocolRes.arith) <= 4 ){
            printf("Got anser %d Expected answer: %d \n", ntohl(protocolRes.inResult), inResult);
          }
          else{
            printf("Got anser %lf Expected answer: %lf \n", protocolRes.flResult, flResult);
          }
          printf("The answer is wrong\n");
        }
        // 处理完毕，将客户端session id从session中抹去 上互斥锁
        mtx.lock();
        session.erase(session_id);
        // 释放互斥锁
        mtx.unlock();
        printf("\n");
        flag = 0;
        loopCount = 0;
      }
      // 这不是我想要的报文
      else
      {
        printf("\n");
        message.type = htons(2);
        message.message = htonl(2);
        message.major_version = htons(1);
        message.minor_version = htons(0);
        message.protocol = htons(17);
        // 发送错误信息message到客户端
        sendto(sockfd, (char *)&message, sizeof(calcMessage), 0, (struct sockaddr *)&client, addrlen);
        printf("Error response type\n");
        flag = 0;
        loopCount = 0;
      }
    }
    flag = 0;
    loopCount = 0;
  }
}