#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#define DEF_PORT 27000
#define DATA_BUFSIZE 8192
#define MAX_CONN 128

#define RM_DBG_WSA 1
/*
  Table of Contents
  §[ 1 ] Structures
  §[ 2 ] Prototypes and Global Variables
  §[ 3 ] Socket Server Functions
  §[ 99] Main
*/


/*
  §[ 1 ] 
*/
typedef struct _SOCKET_INFORMATION
{
  SOCKET Socket;
  OVERLAPPED Overlapped;
  DWORD dwBytesSEND;
  DWORD dwBytesRECV;
  CHAR sBuffer[ DATA_BUFSIZE ];
  WSABUF DataBuf;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

/*
  §[ 2 ] Prototypes and Global Variables
*/
BOOL CreateSocketInformation( SOCKET s );
void FreeSocketInformation( DWORD dwIndex );

DWORD dwTotalSockets = 0;
LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];

/*
  §[ 3 ] Socket Server functions
*/
int __cdecl srv( int iPort )
{
  SOCKET listenSocket;
  SOCKET acceptSocket;
  SOCKADDR_IN inetAddr;
  WSADATA wsaData;
  INT ret;
  FD_SET writeSet;
  FD_SET readSet;
  DWORD i;
  DWORD dwTotal;
  ULONG iNonBlock;
  DWORD dwFlags;
  DWORD dwSendBytes;
  DWORD dwRecvBytes;

  if( (ret = WSAStartup( 0x0202, &wsaData )) != 0 )
  {
    printf( "WSAStartup() failed with error %d\n", ret );
    WSACleanup();
  }
#ifdef RM_DBG_WSA
  printf( "WSAStartup() succeeded.\n");
#endif

  if( (listenSocket = WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED )) == INVALID_SOCKET )
  {
    printf( "WSASocket() failed with error %d\n", WSAGetLastError() );
    return 1;
  }
#ifdef RM_DBG_WSA
  printf( "WSASocket() succeeded.\n");
#endif

  inetAddr.sin_family = AF_INET;
  inetAddr.sin_addr.s_addr = htonl( INADDR_ANY );
  inetAddr.sin_port = htons( iPort );

  if( bind( listenSocket, (PSOCKADDR) &inetAddr, sizeof( inetAddr ) )
        == SOCKET_ERROR )
  {
    printf( "bind() failed with error %d\n", WSAGetLastError() );
  }

#ifdef RM_DBG_WSA
  printf( "bind() succeeded.\n");
#endif

  if( listen( listenSocket, MAX_CONN ) )
  {
    printf( "listen() failed with error %d\n", WSAGetLastError() );
    return 1;
  }
  
#ifdef RM_DBG_WSA
  printf( "listen() succeeded.\n");
#endif

  //make the socket non-blocking
  iNonBlock = 1;
  if( ioctlsocket( listenSocket, FIONBIO, &iNonBlock ) == SOCKET_ERROR )
  {
    printf( "ioctlsocket() failed with error %d\n", WSAGetLastError() );
    return 1;
  }

#ifdef RM_DBG_WSA
  printf( "ioctlsocket() succeeded.\n");
#endif

  while( TRUE )
  {
    FD_ZERO( &readSet );
    FD_ZERO( &writeSet );

    //always check for connection attempts
    FD_SET( listenSocket, &readSet );

    //Set read and write notification based on buffers.
    for( i = 0; i < dwTotalSockets; ++i )
    {
      if( SocketArray[ i ]->dwBytesRECV > SocketArray[ i ]->dwBytesSEND )
      {
        FD_SET( SocketArray[ i ]->Socket, &writeSet );
      }
      else
      {
        FD_SET( SocketArray[ i ]->Socket, &readSet );
      }
    } //for( i = 0; i < dwTotalSockets; ++i )

    if( (dwTotal = select( 0, &readSet, &writeSet, NULL, NULL ) )
        == SOCKET_ERROR )
    {
      printf("select() returned unhandled error %d\n", WSAGetLastError() );
      return 1;
    } //if( ... select( ... ) == SOCKET_ERROR )

#ifdef RM_DBG_WSA
    printf( "select() succeeded.\n" );
#endif

  if( FD_ISSET( listenSocket, &readSet ) )
  {
    --dwTotal;
    if( (acceptSocket = accept( listenSocket, NULL, NULL )) 
        != INVALID_SOCKET )
    {
      iNonBlock = 1;
      if( ioctlsocket( acceptSocket, FIONBIO, &iNonBlock )
          == SOCKET_ERROR )
      {
        printf( "ioctlsocket( FIONBIO ) failed with error %d\n", 
          WSAGetLastError() );
        return 1;
      }
#ifdef RM_DBG_WSA
      printf( "ioctlsocket( FIONBIO ) succeeded." );
#endif
  
      if( CreateSocketInformation( acceptSocket ) == FALSE )
      {
        printf( "CreateSocketInformation() failed.\n");
        return 1;
      }
#ifdef RM_DBG_WSA
      printf( "CreateSocketInformation() succeeded." );
#endif

    } //if( ... accept( ... ) != INVALID_SOCKET )
    else
    {
      if( WSAGetLastError() != WSAEWOULDBLOCK )
      {
        printf( "accept() failed with error %d\n", WSAGetLastError() );
        return 1;
      }
      
#ifdef RM_DBG_WSA
      printf( "accept() succeeded." );
#endif
    }//else for if( ... accept( ... ) != INVALID_SOCKET )
  }//if( FD_ISSET( listenSocket, &readSet ) )

  //check each socket for read until the number of sockets in dwTotal
  for( i = 0; dwTotal > 0 && i < dwTotalSockets; ++i )
  {
    printf("checking socket [%d]\n", i );
    LPSOCKET_INFORMATION SocketInfo = SocketArray[ i ];

    if( FD_ISSET( SocketInfo->Socket, &readSet ) )
    {
      --dwTotal;

      SocketInfo->DataBuf.buf = SocketInfo->sBuffer;
      SocketInfo->DataBuf.len = DATA_BUFSIZE;

      dwFlags = 0;

      if( WSARecv( SocketInfo->Socket, &( SocketInfo->DataBuf ), 1, &dwRecvBytes, &dwFlags, NULL, NULL ) == SOCKET_ERROR )
      {
        if( WSAGetLastError() != WSAEWOULDBLOCK )
        {
          printf("WSARecv() failed with error %d\n", WSAGetLastError() );
          FreeSocketInformation(i);
        }
#ifdef RM_DBG_WSA
        printf( "WSARecv() succeeded." );
#endif
        continue;
      }//if( WSARecv( ... ) != SOCKET_ERROR )
      else
      {
        SocketInfo->dwBytesRECV = dwRecvBytes;

        //if zero bytes, the peer closed the connection.
        if( dwRecvBytes == 0 )
        {
          FreeSocketInformation( i );
          continue;
        }
      }
    }//if( FD_ISSET( SocketInfo->Socket, &readSet ) )
    if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
    {
      --dwTotal;

      SocketInfo->DataBuf.buf = SocketInfo->sBuffer + SocketInfo->dwBytesSEND;
      SocketInfo->DataBuf.len = SocketInfo->dwBytesRECV - SocketInfo->dwBytesSEND;

      if( WSASend( SocketInfo->Socket, &( SocketInfo->DataBuf ), 1, &dwSendBytes, 0, NULL, NULL ) == SOCKET_ERROR )
      {
        if( WSAGetLastError() != WSAEWOULDBLOCK )
        {
          printf( "WSASend() failed with error %d.\n", WSAGetLastError() );
          FreeSocketInformation( i );
        }
#ifdef RM_DBG_WSA
        printf( "WSASend() succeeded." );
#endif
        continue;
      }// if( WSASend( ... ) != SOCKET_ERROR )
      else
      {
        SocketInfo->dwBytesSEND += dwSendBytes;

        if( SocketInfo->dwBytesSEND == SocketInfo->dwBytesRECV )
        {
          SocketInfo->dwBytesSEND = 0;
          SocketInfo->dwBytesRECV = 0;
        }
      }//else OF if( WSASend( ... ) != SOCKET_ERROR )
    }//if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
  }//for( i = 0; dwTotal > 0 && i < dwTotalSockets; ++i )

  }//while( TRUE ) //for select.
  
  return 0;
}//int __cdecl srv( int iPort )

BOOL CreateSocketInformation( SOCKET s )
{
  LPSOCKET_INFORMATION si;
  
#ifdef RM_DBG_WSA
  printf( "accepted socket number %d.\n", (int)s );
#endif

  if( (si = (LPSOCKET_INFORMATION) GlobalAlloc( GPTR, sizeof( SOCKET_INFORMATION ) ) ) == NULL )
  {
    printf( "GlobalAlloc() failed with error %d\n", GetLastError() );
    return FALSE;
  }

#ifdef RM_DBG_WSA
  printf( "GlobalAlloc() for SOCKET_INFORMATION succeeded.\n" );
#endif

  //Prepare SocketInfo structure.
  si->Socket = s;
  si->dwBytesSEND = 0;
  si->dwBytesRECV = 0;

  SocketArray[ dwTotalSockets ] = si;
  ++dwTotalSockets;
  return TRUE;
}//BOOL CreateSocketInformation( SOCKET s )

void FreeSocketInformation( DWORD dwIndex )
{
  LPSOCKET_INFORMATION si = SocketArray[ dwIndex ];
  DWORD i;

  closesocket( si->Socket );
  
  #ifdef RM_DBG_WSA
  printf( "closing socket number %d.\n", (int) (si->Socket) );
  #endif

  //vacuum socket array.
  for( i = dwIndex; i < dwTotalSockets; ++i )
  {
    SocketArray[ i ] = SocketArray[ i + 1 ];
  }

  --dwTotalSockets;
}

/*
  §[ 99] Main
*/
int __cdecl main( void )
{
  printf("§Result = %d\n", srv( DEF_PORT ) );
  return 0;
}
