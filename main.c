#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#define DEF_PORT 27000
#define DATA_BUFSIZE 8192
#define MAX_CONN 128
#define USERNAME_MAX_LENGTH 255

#define RM_DBG_WSA 1
/*
  Table of Contents
  �[ 1 ] Structures
  �[ 2 ] Prototypes and Global Variables
  �[ 3 ] Socket Server Functions
  �[ 99] Main
*/


/*
  �[ 1 ] Structures
*/
typedef struct _SOCKET_INFORMATION
{
  SOCKET Socket;
  OVERLAPPED Overlapped;
  DWORD dwBytesRECV;
  CHAR sBufferIn[ DATA_BUFSIZE ];
  CHAR sBufferIncomingMessage[ DATA_BUFSIZE ];//temporarily hold the message.
  DWORD dwIncomingMessageLength;
  CHAR sBufferSend[ DATA_BUFSIZE ];
  DWORD dwBytesToSEND;
  CHAR username[ USERNAME_MAX_LENGTH ];
  CHAR room[ DATA_BUFSIZE ];  //ordered list represents the rooms a user is in.  Room names at every USERNAME_MAX_LENGTH. Not yet used.
  WSABUF DataBufIn;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

/*
  �[ 2 ] Prototypes and Global Variables
*/
BOOL CreateSocketInformation( SOCKET s );
void FreeSocketInformation( DWORD dwIndex );
void printAllBuffer();
void queueMessage( int id, char * s, DWORD buf_len );
void processIncomingMessage( int id );

DWORD dwTotalSockets = 0;
LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];

/*
  �[ 3 ] Socket Server functions
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
  DWORD k;
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
      if( (SocketArray[ i ]->dwBytesToSEND) > 0 )
      {
        printf("bytes>0\n");
        FD_SET( SocketArray[ i ]->Socket, &writeSet );
      }
      
      //always listen for incoming data
      FD_SET( SocketArray[ i ]->Socket, &readSet );

    } //for( i = 0; i < dwTotalSockets; ++i )

#ifdef RM_DBG_WSA
    printAllBuffer();
#endif

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
        //if zero bytes, the peer closed the connection.
        if( dwRecvBytes == 0 )
        {
          FreeSocketInformation( i );
          continue;
        }

        //otherwise we are good to continue.
        //socket success.
        SocketInfo->dwBytesRECV = dwRecvBytes;
        
        //do processing
        for( k = 0; k < dwRecvBytes && SocketInfo->dwIncomingMessageLength < DATA_BUFSIZE; ++k )
        {
          SocketInfo->sBufferIncomingMessage[ (SocketInfo->dwIncomingMessageLength)++ ] = SocketInfo->sBufferIn[k];
        }
        
        //if there is a new line, process the buffer.
        if( SocketInfo->sBufferIncomingMessage[ SocketInfo->dwIncomingMessageLength - 1] == '\n')
        {
          processIncomingMessage( i );
        }

        //clear the socket state
        memset( SocketInfo->sBufferIn, 0, DATA_BUFSIZE );
        SocketInfo->dwBytesRECV = 0;

        
      }
    }//if( FD_ISSET( SocketInfo->Socket, &readSet ) )

    if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
    {
      WSABUF DataBufOut;
      DataBufOut.buf = SocketInfo->sBufferSend;
      DataBufOut.len = (SocketInfo->dwBytesToSEND > 10)?10:SocketInfo->dwBytesToSEND;
      dwSendBytes = 0;

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
        SocketInfo->dwBytesToSEND -= dwSendBytes;
        printf( "WSASendBytes sent %d bytes. There remain %d bytes in the buffer for this socket.\n", dwSendBytes, SocketInfo->dwBytesToSEND );
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

/*
  BOOL CreateSocketInformation( SOCKET s )
  
  Helper function to prepare the SocketInformation structure.

  Parameters:
    SOCKET s
      The socket on which a connection has been accepted, that the S.I. structure
      will be created around.
  Returns:
    TRUE (WINAPI 1) if the call was succesful.
    FALSE (WINAPI 0) if there was a memory allocation failure.
*/
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
  si->dwBytesToSEND = 0;
  si->dwBytesRECV = 0;
  si->dwIncomingMessageLength = 0;
  sprintf( si->username, "user%d", dwTotalSockets );
  sprintf( si->room, "#global" );

  SocketArray[ dwTotalSockets ] = si;
  ++dwTotalSockets;
  return TRUE;
}//BOOL CreateSocketInformation( SOCKET s )

/*
  void FreeSocketInformation( DWORD dwIndex )

  Releases the memory of the SocketInformation structure.

  SocketArray indexes will be moved to make sure that the array is
  contiguous. 

  Parameters:
    DWORD dwIndex
      The index in the SocketArray corresponding to the S.I. structure to
      destroy.
  Returns:
    void.
*/
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

/*
  void printAllBuffer()

  Debugging function that prints the contents of buffers for all sockets.
*/
void printAllBuffer()
{
  DWORD i;
  
  printf("--begin print all buffers--\n");
  for( i = 0; i < dwTotalSockets; ++i )
  {
    printf("[%d] socket\nread(%d): %s %s\n",i, 
      (int) (SocketArray[i]->dwBytesRECV), SocketArray[i]->DataBufIn.buf, SocketArray[i]->sBufferIn );
  }
  printf("--end print all buffers--\n");

}

/*
  void queueMessage( int id, char * s, DWORD buf_len )

  Queues up a message to be sent on a socket by appending it to the end of the
    send buffer, as long as the message fits.

  TODO: If the message does not fit in the send buffer, add a linkedlist to queue
    up messages to be sent later.

  Parameters:
    int id
      The id in SocketArray that corresponds to the SocketInformation/Socket
      on which the data is to be sent.
    char* s
      A string containing the buffer
    DWORD dwBufLen
      The length of s.  This is checked to make sure that the message will fit
      in the buffer.

  Returns:
    Nothing.  TODO: Should be altered to return TRUE or FALSE later.

*/
void queueMessage( int id, char * s, DWORD dwBufLen )
{
  printf("Queue message on [%d]. BytesToSend before: %d, Buf_len: %d\n", id, SocketArray[id]->dwBytesToSEND, dwBufLen );
  if( SocketArray[id]->dwBytesToSEND + dwBufLen < DATA_BUFSIZE )
  {    //only if it fits in buffer.
    //sprintf( SocketArray[id]->sBufferSend + SocketArray[id]->dwBytesToSEND, "msg:\"%s\"\n", s );
    memcpy( SocketArray[id]->sBufferSend + SocketArray[id]->dwBytesToSEND, s, dwBufLen );
    SocketArray[id]->dwBytesToSEND += dwBufLen;
  }
  else
  {
    printf("The buffer is full.\n");
    //TODO: Make a linked list queue for the messages.
  }
}

/*
  void processIncomingMessage( int id )

  Processes the contents of sBufferIncomingMessage for the SocketInformation in
  the SocketArray specified by id.  Before calling this, it should be verified that
  it is time to send a message/command (e.g. that the last character in the buffer is
  a '\n'). 

  If the message is a special command, this is handled.

  If the message is simply a message, it is sent to all other users.

  The sBufferIncomingMessage is then cleared, and the dwIncomingMessageLength is set to 0.
*/

void processIncomingMessage( int id )
{
  int i;
  char sUsernameOut[USERNAME_MAX_LENGTH + 50];
  printf("Incoming Message on [%d]: %s\n", id, SocketArray[id]->sBufferIncomingMessage );
  
  sprintf(sUsernameOut, "%s:", SocketArray[id]->username );
#ifdef RM_DBG_WSA
  printf("[");
  for( i = 0; i < DATA_BUFSIZE; ++i )
  {
    if( i != 0 ){ printf(","); }
    printf(" (%d)(%c) ", SocketArray[id]->sBufferIncomingMessage[i], SocketArray[id]->sBufferIncomingMessage[i]);
    if( SocketArray[id]->sBufferIncomingMessage[i] == '\n') break;
  }
  printf("]\n");
#endif
  if( SocketArray[id]->sBufferIn[0] == '/') 
  {
    queueMessage( id, "Command!\r\n", 9); 
  }
  else
  {
    for( i = 0; i < dwTotalSockets; ++i )
    {
      //TODO: implement room check.
      queueMessage( i, sUsernameOut, strlen( sUsernameOut ) );
      queueMessage( i, SocketArray[id]->sBufferIncomingMessage, SocketArray[id]->dwIncomingMessageLength );
    }
  }

  //clear the message.
  memset( SocketArray[id]->sBufferIncomingMessage, 0, DATA_BUFSIZE );
  SocketArray[id]->dwIncomingMessageLength = 0;
}

/*
  �[ 99] Main
*/
int __cdecl main( void )
{
  printf("�Result = %d\n", srv( DEF_PORT ) );
  return 0;
}
