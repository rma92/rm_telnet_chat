#undef UNICODE

#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 8192 
#define DEFAULT_PORT "28000"
#define MAX_CLIENT_SOCKETS 10

#define DBG_SOCKETS 1
int __cdecl srv()
{
  WSADATA wsaData;
  int iResult;
  int res;
  int i;
  int Total; //holds the results of select.
  int num_socks = 1;
  ULONG iNonBlock;
  DWORD iFlags;
  DWORD iRecvBytes;
  DWORD iSendBytes;

  SOCKET socks[MAX_CLIENT_SOCKETS];
  FD_SET ReadSet;
  FD_SET WriteSet;

  SOCKET sAccept = INVALID_SOCKET;
  //SOCKET socks[0] = INVALID_SOCKET;

  
  //SOCKET sClients[MAX_CLIENT_SOCKETS];
  //SOCKET sClient = INVALID_SOCKET;

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
  socks[0] = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
  if( socks[0] == INVALID_SOCKET )
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
  iResult = bind( socks[0], result->ai_addr, (int)result->ai_addrlen );
  if( iResult == SOCKET_ERROR )
  {
    printf( "bind failed with error: %d\n", WSAGetLastError() );
    freeaddrinfo( result );
    closesocket( socks[0] );
    WSACleanup();
    return 1;
  }

#ifdef DBG_SOCKETS
  printf("freeaddrinfo, listen\n");
#endif

  freeaddrinfo( result );

  iResult = listen( socks[0], SOMAXCONN );
  if( iResult == SOCKET_ERROR )
  {
    printf( "listen failed with error: %d\n", WSAGetLastError() );
    closesocket( socks[0] );
    WSACleanup();
    return 1;
  }

#ifdef DBG_SOCKETS
  printf("making listen socket non-blocking.\n");
#endif
  
  iNonBlock = 1;
  if( ioctlsocket( socks[0], FIONBIO, &iNonBlock ) == SOCKET_ERROR )
  {
    printf("ioctlsocket() failed with error %d\n", WSAGetLastError() );
    return 1;
  }
#ifdef DBG_SOCKETS
  else
  {
    printf("ioctlsocket() is okay!\n");
  }
#endif
  
  while(1)
  {
    FD_ZERO( &ReadSet );
    FD_ZERO( &WriteSet );
    
    //always add listening socket to FD_SET.
    FD_SET( socks[0], &ReadSet );

    for( i = 1; i < num_socks; ++i )
    {
      if( socks[i]->BytesRECV > socks[i]->BytesSEND )
      {
        FD_SET( socks[i]->Socket, &WriteSet );
      }
      else
      {
        FD_SET( socks[i]->Socket, &ReadSet );
      }
    }

    if( (Total = select( 0, &ReadSet, &WriteSet, NULL, NULL )) == SOCKET_ERROR )
    {
      printf("select() returned error %d\n", WSAGetLastError() );
    }
    else
    {
      printf("select() is fine.");

      //check for arriving connections on the listening socket.
      if( FD_ISSET( socks[0], &ReadSet ))
      {
        --Total;
        if( (sAccept = accept( socks[0], NULL, NULL )) != INVALID_SOCKET )
        {
          //set the accepted socket to nonblocked.
          iNonBlock = 1;
          if( ioctlsocket( sAccept, FIONBIO, &iNonBlock) == SOCKET_ERROR )
          {
            printf("ioctlsocket(FIONBIO) on accept socket failed with error %d\n", WSAGetLastError());
            return 1;
          }
          printf("ioctlsocket(FIONBIO) on accept socket is okay!\n");

          /*
          if( CreateSocketInformation( sAccept ) == FALSE )
          {
            printf("CreateSocketInformation(sAccept) failed!\n");
            return 1;
          }
          else
          {
            printf("Create Socket Information is Okay\n");
          }
          */

          socks[num_socks] = sAccept;
          ++num_socks;
        }//if( sAccept = accept...

      }//if( FD_ISSET( socks[0] // is set listen socket.

      //Check each socket for R and W notification until the number of sockets in Total is satisfied.
      for( i = 0; Total > 0 && i < num_socks; ++i )
      {
        LPSOCKET_INFORMATION SocketInfo = socks[i];

        if( FD_ISSET( SocketInfo->Socket, &ReadSet ) )
        {
          --Total;

          SocketInfo->DataBuf.buf = SocketInfo->Buffer;
          SocketInfo->DataBuf.len = DEFAULT_BUFLEN;

          Flags = 0;

          if( WSARecv( SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &iRecvBytes, &iFlags, NULL, NULL ) == SOCKET_ERROR )
          {
            if( WSAGetLastError() != WSAEWOULDBLOCK )
            {
              printf("WSARecv() failed on socks[%d] with error %d\n", i, WSAGetLastError() );
              //TODO: purge socket.
            }
            else
            {
              printf("WSARecv() is OK!\n");
            }
          }//if( WSARecv(...) == SOCKET_ERROR)
          else
          {
            SocketInfo->BytesRECV = iRecvBytes);
          }
        }
      }

    }//else for select failed

  }//while 1 for socket select.
/*
#ifdef DBG_SOCKETS
  printf("wait for client\n");
#endif

  // Accept a client socket
  socks[ num_socks ] = accept( socks[0], NULL, NULL );
  if( socks[ num_socks ] == INVALID_SOCKET )
  {
    printf( "accept failed with error: %d\n", WSAGetLastError() );
    closesocket( socks[0] );
    WSACleanup();
    return 1;
  }

#ifdef DBG_SOCKETS
  printf("loop\n");
#endif

  // No longer need server socket
  closesocket(socks[0]);
  
  while( res = select( 0, socks, 0, 0, NULL ) )
  {
    //something happened
    //listener socket:
    //if( FD_ISSET( socks[0]
    //todo: see http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancediomethod5a.html
  }
  
  //loop
  do
  {
    iResult = recv( sClient, recv_buf, recv_buf_len, 0 );
    if( iResult > 0 )
    {
      printf( "Bytes received: %d\n", iResult );

      // Echo the buffer back to the sender
      iSendResult = send( sClient, recv_buf, iResult, 0 );
      if( iSendResult == SOCKET_ERROR )
      {
        printf( "send failed with error: %d\n", WSAGetLastError() );
        closesocket( sClient );
        WSACleanup();
        return 1;
      }
      printf( "Bytes sent: %d\n", iSendResult );
    }
    else if( iResult == 0 )
    {
      printf( "Connection closing...\n" );
    }
    else  
    {
      printf( "recv failed with error: %d\n", WSAGetLastError() );
      closesocket( sClient );
      WSACleanup();
      return 1;
    }
  }while( iResult > 0 );
  

  iResult = shutdown( sClient, SD_SEND );
  if( iResult == SOCKET_ERROR ) {
    printf( "shutdown failed with error: %d\n", WSAGetLastError() );
    closesocket( sClient );
    WSACleanup();
    return 1;
  }

  // cleanup
  closesocket( sClient );
*/
  WSACleanup();

  return 0;
} //srv function

int __cdecl main( void )
{
  printf( "srv result = %d\n", srv() );
  return 0;
}
