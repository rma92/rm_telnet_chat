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
  //DWORD dwBytesSEND;
  DWORD dwBytesRECV;
  CHAR sBufferIn[ DATA_BUFSIZE ];
  CHAR sBufferSend[ DATA_BUFSIZE ];
  DWORD dwBytesToSEND;
  //CHAR sBufferOut[ DATA_BUFSIZE ];
  WSABUF DataBufIn;
  //WSABUF DataBufOut;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

/*
  §[ 2 ] Prototypes and Global Variables
*/
BOOL CreateSocketInformation( SOCKET s );
void FreeSocketInformation( DWORD dwIndex );
void printAllBuffer();
void queueMessage( int id, char * s, DWORD buf_len );
void processIncomingMessage( int id );

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
      //if there is anything to write, we prepare to send.
      //printf("Socket[%d] Bytes SenD: %d\n", i, SocketArray[ i ]->dwBytesSEND);
      
      if( (SocketArray[ i ]->dwBytesToSEND) > 0 )
      {
        printf("bytes>0\n");
        FD_SET( SocketArray[ i ]->Socket, &writeSet );
      }
      
      //always listen for incoming data
      FD_SET( SocketArray[ i ]->Socket, &readSet );

    } //for( i = 0; i < dwTotalSockets; ++i )

    printAllBuffer();

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

      SocketInfo->DataBufIn.buf = SocketInfo->sBufferIn;
      SocketInfo->DataBufIn.len = DATA_BUFSIZE;

      dwFlags = 0;

      if( WSARecv( SocketInfo->Socket, &( SocketInfo->DataBufIn ), 1, &dwRecvBytes, &dwFlags, NULL, NULL ) == SOCKET_ERROR )
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
        //socket success.
        SocketInfo->dwBytesRECV = dwRecvBytes;
        
        //do processing
        processIncomingMessage( i );
        /*
        memset( SocketInfo->sBufferSend, 0, DATA_BUFSIZE );
        sprintf( SocketInfo->sBufferSend, " You said: \"%s\"\n", SocketInfo->sBufferIn );
        SocketInfo->dwBytesToSEND = strlen( SocketInfo->sBufferSend );
        */

        //clear the socket state
        memset( SocketInfo->sBufferIn, 0, DATA_BUFSIZE );
        SocketInfo->dwBytesRECV = 0;

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
      WSABUF DataBufOut;
      DataBufOut.buf = SocketInfo->sBufferSend;
      //DataBufOut.len = SocketInfo->dwBytesToSEND;
      DataBufOut.len = (SocketInfo->dwBytesToSEND > 10)?10:SocketInfo->dwBytesToSEND;
      dwSendBytes = 0;
      //SocketInfo->DataBufOut.buf = SocketInfo->sBufferOut;
      //SocketInfo->DataBufOut.len = SocketInfo->dwBytesSEND;

      if( WSASend( SocketInfo->Socket, &DataBufOut, 1, &dwSendBytes, 0, NULL, NULL ) == SOCKET_ERROR )
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
        printf("WSASendBytes sent %d bytes.\n", dwSendBytes);
        SocketInfo->dwBytesToSEND -= dwSendBytes;
        printf("There remain %d bytes in the buffer for this socket.", SocketInfo->dwBytesToSEND);
        //move the remaining buffer bytes up if the buffer wasn't sent.
        if( SocketInfo->dwBytesToSEND > 0 )
        {
          for( i = dwSendBytes; i < DATA_BUFSIZE; ++i )
          {
            SocketInfo->sBufferSend[ i - dwSendBytes ] = SocketInfo->sBufferSend[ i ];
          }
        }
        else
        {
          memset( SocketInfo->sBufferSend, 0, DATA_BUFSIZE );
        }
        //SocketInfo->dwBytesSEND -= dwSendBytes;

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
  //si->dwBytesSEND = 0;
  si->dwBytesToSEND = 0;
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
}//void FreeSocketInformation( DWORD dwIndex )

void printAllBuffer()
{
  DWORD i;
  
  printf("--begin print all buffers--\n");
  for( i = 0; i < dwTotalSockets; ++i )
  {
    printf("[%d] socket\nread(%d): %s %s\n",i, (int) (SocketArray[i]->dwBytesRECV), SocketArray[i]->DataBufIn.buf, SocketArray[i]->sBufferIn );
    //SocketArray[i]->dwBytesSEND = 10;
    //memcpy( SocketArray[i]->DataBufOut.buf, "abcdefghijklmnopqrstuvw", 10);
    /*
    printf("  [%d] socket\nin(%d): %s\n%s\nout(%d): %s\n%s\n", i,
       SocketArray[i]->dwBytesRECV, SocketArray[i]->DataBufIn.buf, SocketArray[i]->sBufferIn,
       SocketArray[i]->dwBytesSEND, SocketArray[i]->DataBufOut.buf, SocketArray[i]->sBufferOut);
    SocketArray[i]->dwBytesSEND = 0;
    */
  }
  printf("--end print all buffers--\n");

}

//prepares to send a message to a client.
void queueMessage( int id, char * s, DWORD buf_len )
{
  printf("Queue message on [%d]. BytesToSend before: %d, Buf_len: %d\n", id, SocketArray[id]->dwBytesToSEND, buf_len );
  if( SocketArray[id]->dwBytesToSEND + buf_len < DATA_BUFSIZE )
  {    //only if it fits in buffer.
    //sprintf( SocketArray[id]->sBufferSend + SocketArray[id]->dwBytesToSEND, "msg:\"%s\"\n", s );
    memcpy( SocketArray[id]->sBufferSend + SocketArray[id]->dwBytesToSEND, s, buf_len );
    SocketArray[id]->dwBytesToSEND += buf_len;
  }
  else
  {
    printf("The buffer is full.\n");
    //TODO: Make a linked list queue for the messages.
  }
}

//processes an incoming message.  Called after WSARecv occurs, parameter is the socket who's input
//buffer contains the received message.
void processIncomingMessage( int id )
{
  printf("Incoming Message on [%d]: %s\n", id, SocketArray[id]->sBufferIn );
  queueMessage( id, "MSG:", 4);
  queueMessage( id, SocketArray[id]->sBufferIn, strlen( SocketArray[id]->sBufferIn ) );
  //memcpy( SocketArray[id]->DataBufOut.buf, st, strlen( st ) );
  //SocketArray[id]->dwBytesSEND = 4;//strlen( st );
}

/*
  §[ 99] Main
*/
int __cdecl main( void )
{
  printf("§Result = %d\n", srv( DEF_PORT ) );
  return 0;
}
