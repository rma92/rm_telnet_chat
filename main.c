#undef UNICODE

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "28000"
#define MAX_CLIENT_SOCKETS 10

#define DBG_SOCKETS 1
int __cdecl srv()
{
  WSADATA wsaData;
  int iResult;

  SOCKET sListen = INVALID_SOCKET;
  SOCKET sClients[MAX_CLIENT_SOCKETS];
  SOCKET sClient = INVALID_SOCKET;

  struct addrinfo *result = NULL;
  struct addrinfo hints;

  int iSendResult;
  char recv_buf[DEFAULT_BUFLEN];
  int recv_buf_len = DEFAULT_BUFLEN;
#ifdef DBG_SOCKETS
  printf("init...\n");
#endif
  //initialize Winsock
  iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
  if( iResult != 0 )
  {
    printf( "WSAStartup failed with error: %d\n", iResult );
    return 1;
  }
#ifdef DBG_SOCKETS
  printf("setting hints\n");
#endif

  ZeroMemory( &hints, sizeof(hints) );
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

#ifdef DBG_SOCKETS
  printf("getaddrinfo\n");
#endif

  // Resolve the server address and port
//  iResult = getaddrinfo( (PCSTR) NULL, (PCSTR) DEFAULT_PORT, &hints, &result );
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);

  if( iResult != 0 )
  {
    printf( "getaddrinfo failed with error: %d\n", iResult );
    WSACleanup();
    return 1;
  }

#ifdef DBG_SOCKETS
  printf("create socket\n");
#endif

  // Create a SOCKET for connecting to server
  sListen = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
  if( sListen == INVALID_SOCKET )
  {
    printf( "socket failed with error: %ld\n", WSAGetLastError() );
    freeaddrinfo( result );
    WSACleanup();
    return 1;
  }

#ifdef DBG_SOCKETS
  printf("create listening socket\n");
#endif

  // Setup the TCP listening socket
  iResult = bind( sListen, result->ai_addr, (int)result->ai_addrlen );
  if( iResult == SOCKET_ERROR ){
      printf( "bind failed with error: %d\n", WSAGetLastError() );
      freeaddrinfo( result );
      closesocket( sListen );
      WSACleanup();
      return 1;
  }

#ifdef DBG_SOCKETS
  printf("freeaddrinfo, listen\n");
#endif

  freeaddrinfo( result );

  iResult = listen( sListen, SOMAXCONN );
  if( iResult == SOCKET_ERROR )
  {
      printf( "listen failed with error: %d\n", WSAGetLastError() );
      closesocket( sListen );
      WSACleanup();
      return 1;
  }

#ifdef DBG_SOCKETS
  printf("wait for client\n");
#endif

  // Accept a client socket
  sClient = accept( sListen, NULL, NULL );
  if( sClient == INVALID_SOCKET )
  {
      printf( "accept failed with error: %d\n", WSAGetLastError() );
      closesocket( sListen );
      WSACleanup();
      return 1;
  }

#ifdef DBG_SOCKETS
  printf("loop\n");
#endif

  // No longer need server socket
  closesocket(sListen);

  //loop

  return 0;
}

int __cdecl main( void )
{
  printf( "srv result = %d\n", srv() );
  return 0;
}
