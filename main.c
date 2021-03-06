#if defined( _WIN32 ) && !defined( linux )
  #define RM_WIN32_SYSTEM
  #include <winsock2.h>
  #include <windows.h>
#elif defined( linux ) && !defined( _WIN32 )
  #define RM_POSIX_SYSTEM
  #include <sys/time.h>
  #include <sys/types.h>
  #include <string.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <malloc.h>
  typedef int SOCKET;
  typedef unsigned long DWORD;
  typedef char CHAR;
  typedef int BOOL;
  #define FALSE 0
  #define TRUE 1
#endif
#include <stdio.h>

#ifdef __cplusplus
  #include <map>
  #include <string>
#endif

#define DEF_PORT 27000
#define DATA_BUFSIZE 8192
#define MAX_CONN 128
#define USERNAME_MAX_LENGTH 255

#define RM_DBG_WSA 1
/*
  Table of Contents
  �[ 1 ] Structures
    �[1.1] SOCKET_INFORMATION
  �[ 2 ] Prototypes and Global Variables
  �[ 3 ] Socket Server Function
    �[3.1] Socket Server Function - Windows
    �[3.2] Socket Server Function - POSIX
  �[ 4 ] Socket Helper Functions
    �[4.1] BOOL CreateSocketInformation( SOCKET s )
    �[4.2] void FreeSocketInformation( DWORD dwIndex )
  �[ 5 ] Message Processing Functions
    �[5.1] void printAllBuffer()
    �[5.2] void debugShowIncomingBuffer( int id )
    �[5.3] void queueMessage( int id, char * s, DWORD buf_len )
    �[5.4] void broadcastMessage( char* s, DWORD dwBufLen )  
    �[5.5] void processIncomingMessage( int id )
    �[5.6] void processIncomingMessageCommand( int id )
    �[5.7] void queueWelcomeMessage( int id )
  �[ 6 ] User Registry Functions
    �[6.1] BOOL userRegCheck( char* username, char* password )
    �[6.2] BOOL userRegister( char* username, char* password )
  �[ 99] Main
*/


/*
  �[ 1 ] Structures
*/
/*
  �[1.1] SOCKET_INFORMATION

  A structure to contain data about a socket, it's buffers, and the 
  user in the session (e.g. user nickname).
*/
#if defined( RM_WIN32_SYSTEM )
typedef struct _SOCKET_INFORMATION
{
  SOCKET Socket;
  OVERLAPPED Overlapped;
  DWORD dwBytesRECV;
  CHAR sBufferIn[ DATA_BUFSIZE ];
  CHAR sBufferIncomingMessage[ DATA_BUFSIZE ];//stage incoming message.
  DWORD dwIncomingMessageLength;
  CHAR sBufferSend[ DATA_BUFSIZE ];
  DWORD dwBytesToSEND;
  CHAR username[ USERNAME_MAX_LENGTH ];
  CHAR room[ DATA_BUFSIZE ];  //list of rooms a user is in.
  WSABUF DataBufIn;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;
#elif defined( RM_POSIX_SYSTEM )
typedef struct _SOCKET_INFORMATION
{
  int Socket;
  int dwBytesRECV;
  char sBufferIn[ DATA_BUFSIZE ];
  char sBufferIncomingMessage[ DATA_BUFSIZE ];
  int dwIncomingMessageLength;
  char sBufferSend[ DATA_BUFSIZE ];
  int dwBytesToSEND;
  char username[ USERNAME_MAX_LENGTH ];
  char room[ DATA_BUFSIZE ]; //list of rooms a user is in, not yet used.
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;
#endif
/*
  �[ 2 ] Prototypes and Global Variables
*/
int CreateSocketInformation( SOCKET s );
void FreeSocketInformation( DWORD dwIndex );
void printAllBuffer();
void queueMessage( int id, char * s, int buf_len );
void broadcastMessage( char * s, int dwBufLen );
void processIncomingMessage( int id );
void processIncomingMessageCommand( int id );
void queueWelcomeMessage( int id );

BOOL userRegCheck( char * username, char * password );
BOOL userRegister( char * username, char * password );

#if defined( RM_WIN32_SYSTEM )
DWORD dwTotalSockets = 0;
#elif defined( RM_POSIX_SYSTEM )
unsigned int dwTotalSockets = 0;
#endif

/*
  An array of SOCKET_INFORMATION structures is used on both systems.
  However, on Windows, there is no 
*/
#if defined( RM_WIN32_SYSTEM )
LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];
#elif defined( RM_POSIX_SYSTEM )
SOCKET_INFORMATION* SocketArray[FD_SETSIZE];
#endif

#ifdef __cplusplus
  std::map<std::string, std::string> dRegisteredUsers;
#endif
/*
  �[ 3 ] Socket Server Function
*/
/*
    �[3.1] Socket Server Function -- Windows
*/
#if defined( RM_WIN32_SYSTEM )
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
  printf( "WSAStartup() succeeded.\n" );
  #endif

  if( (listenSocket = 
    WSASocket( AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED ))
    == INVALID_SOCKET )
  {
    printf( "WSASocket() failed with error %d\n", WSAGetLastError() );
    return 1;
  }
  #ifdef RM_DBG_WSA
  printf( "WSASocket() succeeded.\n" );
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
  printf( "bind() succeeded.\n" );
  #endif

  if( listen( listenSocket, MAX_CONN ) )
  {
    printf( "listen() failed with error %d\n", WSAGetLastError() );
    return 1;
  }
  
  #ifdef RM_DBG_WSA
  printf( "listen() succeeded.\n" );
  #endif

  //make the socket non-blocking
  iNonBlock = 1;
  if( ioctlsocket( listenSocket, FIONBIO, &iNonBlock ) == SOCKET_ERROR )
  {
    printf( "ioctlsocket() failed with error %d\n", WSAGetLastError() );
    return 1;
  }

  #ifdef RM_DBG_WSA
  printf( "ioctlsocket() succeeded.\n" );
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
      if( ( SocketArray[ i ]->dwBytesToSEND ) > 0 )
      {
        FD_SET( SocketArray[ i ]->Socket, &writeSet );
      }
      
      //always listen for incoming data
      FD_SET( SocketArray[ i ]->Socket, &readSet );

    } //for( i = 0; i < dwTotalSockets; ++i )

    #ifdef RM_DBG_WSA
    printAllBuffer();
    #endif

    if( ( dwTotal = select( 0, &readSet, &writeSet, NULL, NULL ) )
        == SOCKET_ERROR )
    {
      printf( "select() returned unhandled error %d\n", WSAGetLastError() );
      return 1;
    } //if( ... select( ... ) == SOCKET_ERROR )

  if( FD_ISSET( listenSocket, &readSet ) )
  {
    --dwTotal;
    if( ( acceptSocket = accept( listenSocket, NULL, NULL ) ) 
        != INVALID_SOCKET )
    {
      char outBufAppear[ DATA_BUFSIZE ];
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
      
      snprintf( outBufAppear, sizeof( outBufAppear ),
                "User [%s] has joined!\r\n", 
                SocketArray[ dwTotalSockets-1 ]->username );
      broadcastMessage( outBufAppear, strlen( outBufAppear ) );
      //Make welcome message
      queueWelcomeMessage( dwTotalSockets-1 );

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

      if( WSARecv( SocketInfo->Socket, &( SocketInfo->DataBufIn ), 1, 
          &dwRecvBytes, &dwFlags, NULL, NULL ) == SOCKET_ERROR )
      {        
        if( WSAGetLastError() != WSAEWOULDBLOCK )
        {
          printf("WSARecv() failed with error %d\n", WSAGetLastError() );
          FreeSocketInformation( i );
        }
        continue;
      }//if( WSARecv( ... ) != SOCKET_ERROR )
      else
      {
        //if zero bytes, the peer closed the connection.
        if( dwRecvBytes == 0 )
        {
          char outBufLeave[ DATA_BUFSIZE ];
          snprintf( outBufLeave, sizeof( outBufLeave ), 
                    "[%s] has left!\r\n",
                    SocketInfo->username );
          FreeSocketInformation( i );
          broadcastMessage( outBufLeave, strlen( outBufLeave ) );
          continue;
        }

        //otherwise we are good to continue.
        //socket success.
        SocketInfo->dwBytesRECV = dwRecvBytes;
        
        //Do processing.
        for( k = 0; 
            k < dwRecvBytes 
            && SocketInfo->dwIncomingMessageLength < DATA_BUFSIZE;
            ++k )
        {
          //Filter terminal characters.
          if( SocketInfo->sBufferIn[ k ] >= 0x20
              || SocketInfo->sBufferIn[ k ] == '\n'
            )
          {
            SocketInfo->sBufferIncomingMessage
                [ ( SocketInfo->dwIncomingMessageLength )++ ]
              = SocketInfo->sBufferIn[ k ];
          }
        }
        
        //If there is a new line, process the buffer.
        if( SocketInfo->sBufferIncomingMessage
            [ SocketInfo->dwIncomingMessageLength - 1]
            == '\n'
          )
        {
          processIncomingMessage( i );
        }

        //Clear the socket state.
        memset( SocketInfo->sBufferIn, 0, DATA_BUFSIZE );
        SocketInfo->dwBytesRECV = 0;
      }
    }//if( FD_ISSET( SocketInfo->Socket, &readSet ) )

    if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
    {
      WSABUF DataBufOut;
      DataBufOut.buf = SocketInfo->sBufferSend;
      //change this to force tiny packets:
      //DataBufOut.len 
      //= ( SocketInfo->dwBytesToSEND > 10 )? 10 :SocketInfo->dwBytesToSEND;
      DataBufOut.len = SocketInfo->dwBytesToSEND;
      dwSendBytes = 0;

      if( WSASend( 
            SocketInfo->Socket, 
            &DataBufOut, 
            1, 
            &dwSendBytes, 
            0, 
            NULL, 
            NULL 
            )
          == SOCKET_ERROR )
      {
        if( WSAGetLastError() != WSAEWOULDBLOCK )
        {
          printf( "WSASend() failed with error %d.\n", WSAGetLastError() );
          FreeSocketInformation( i );
        }
        continue;
      }//if( WSASend( ... ) != SOCKET_ERROR )
      else
      {
        //WSASend Success
        #ifdef RM_DBG_WSA
        printf( "WSASend() succeeded." );
        #endif

        SocketInfo->dwBytesToSEND -= dwSendBytes;
        printf( "WSASendBytes sent %d bytes, %d bytes left.\n",
              dwSendBytes, SocketInfo->dwBytesToSEND );
        //move the remaining buffer bytes up if the buffer wasn't sent.
        if( SocketInfo->dwBytesToSEND > 0 )
        {
          for( k = dwSendBytes; k < DATA_BUFSIZE; ++k )
          {
            SocketInfo->sBufferSend[ k - dwSendBytes ] 
              = SocketInfo->sBufferSend[ k ];
          }
        }
        else
        {
          //if the buffer is empty, clear it for good measure.
          memset( SocketInfo->sBufferSend, 0, DATA_BUFSIZE );
        }
      }//else OF if( WSASend( ... ) != SOCKET_ERROR )
    }//if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
  }//for( i = 0; dwTotal > 0 && i < dwTotalSockets; ++i )

  }//while( TRUE ) //for select.
  
  return 0;
}//int __cdecl srv( int iPort )

/*
    �[3.2] Socket Server Function -- POSIX
*/
#elif defined( RM_POSIX_SYSTEM )

int srv( int port )
{
  int listenSocket;
  int acceptSocket;
  struct sockaddr_in inetAddr;
  fd_set writeSet;
  fd_set readSet;
  unsigned int i;
  int k;
  int dwSendBytes;
  int dwTotal;

  listenSocket = socket( PF_INET, SOCK_STREAM, 0 );
  if( listenSocket < 0 )
  {
    perror( "socket error on create.\n" );
    return 1;
  }

#ifdef RM_DBG_WSA
  printf( "socket create is okay!\n" );
#endif
  inetAddr.sin_family = AF_INET;
  inetAddr.sin_addr.s_addr = htonl( INADDR_ANY );
  inetAddr.sin_port = htons( port );

  if( bind( listenSocket, (struct sockaddr*) &inetAddr, sizeof (inetAddr)) < 0)
  {
    perror( "bind failed on listening socket.\n" );
    return 1;
  }
#ifdef RM_DBG_WSA
  printf( "bind is okay!\n" );
#endif
 
  if( listen( listenSocket, MAX_CONN ) < 0 )
  {
    perror( "Listen failed.\n" );
    return 1;
  }

#ifdef RM_DBG_WSA
  printf( "Listen is okay!\n" );
#endif

  while( 1 )
  {
    FD_ZERO( &writeSet );
    FD_ZERO( &readSet );

    //always check for connection attempts.
    FD_SET( listenSocket, &readSet );

    //Set read and write notification based on buffers.
    for( i = 0; i < dwTotalSockets; ++i )
    {
      if( ( SocketArray[ i ]->dwBytesToSEND ) > 0 )
      {
        FD_SET( SocketArray[ i ]->Socket, &writeSet );
      }

      //always listen for incoming data.
      FD_SET( SocketArray[ i ]->Socket, &readSet );
    }
#ifdef RM_DBG_WSA
    printf( "Select wait...\n" );
#endif
    if( (dwTotal = select( FD_SETSIZE, &readSet, &writeSet, NULL, NULL )) < 0 )
    {
      perror( "Select errored.\n" );
      return 1;
    }
  #ifdef RM_DBG_WSA
    printf( "Select went...\n" );
  #endif
    //service sockets
    if( FD_ISSET( listenSocket, &readSet ) )
    {
      //new connection
      socklen_t size;
      struct sockaddr_in client_name;
      char outBufAppear[ DATA_BUFSIZE ];
      --dwTotal;
      size = sizeof( client_name );
      acceptSocket = 
        accept( listenSocket, (struct sockaddr*) &client_name, &size );
      if( acceptSocket < 0 )
      {
        perror( "failed to accept socket." );
      }
      else
      {
        fprintf( stderr, "Server connect.\n" );
        if( CreateSocketInformation( acceptSocket ) == FALSE )
        {
          printf( "CreateSocketInformation() failed.\n" );
          return 1; //this is a critical failure.
        }
        
        snprintf( outBufAppear, sizeof( outBufAppear ),
                  "User [%s] has joined!\r\n", 
                  SocketArray[ dwTotalSockets-1 ]->username );
        broadcastMessage( outBufAppear, strlen( outBufAppear ) );
        //Make welcome message
        queueWelcomeMessage( dwTotalSockets - 1 );
      }//else for acceptSocket failed.
    }//if (FD_ISSET (listenSocket, &readSet) )

    //Now iterate over other sockets.
    for( i = 0; dwTotal > 0 && i < dwTotalSockets; ++i )
    {
      SOCKET_INFORMATION* SocketInfo = SocketArray[ i ];
      if( FD_ISSET( SocketInfo->Socket, &readSet ) )
      {
        --dwTotal;
        memset( SocketInfo->sBufferIn, 0, DATA_BUFSIZE );
        SocketInfo->dwBytesRECV = 
          read( SocketInfo->Socket, SocketInfo->sBufferIn, DATA_BUFSIZE );

        if( SocketInfo->dwBytesRECV < 0 )
        {
          char outBufLeave[ DATA_BUFSIZE ];
          fprintf( stderr, "Read Error on Socket %d\n", i );
          snprintf( outBufLeave, sizeof( outBufLeave ), 
                    "[%s] has left!\r\n",
                    SocketInfo->username );
          FreeSocketInformation( i );
          #ifdef RM_DBG_WSA
          printf( "Freed socket.\n" );
          #endif
          broadcastMessage( outBufLeave, strlen( outBufLeave ) );
          continue;
        }
        else if ( SocketInfo->dwBytesRECV == 0 )
        {
          char outBufLeave[ DATA_BUFSIZE ];
          //if zero bytes, the peer closed the connection.
          fprintf( stderr, "EOF on %d, closing connection.\n", i );
          snprintf( outBufLeave, sizeof( outBufLeave ), 
                    "[%s] has left!\r\n",
                    SocketInfo->username );
          FreeSocketInformation( i );
          #ifdef RM_DBG_WSA
          printf( "Freed socket.\n" );
          #endif
          broadcastMessage( outBufLeave, strlen( outBufLeave ) );
          continue;
        }
        else
        {
          //Do processing.
          for( k = 0; k < SocketInfo->dwBytesRECV 
              && SocketInfo->dwIncomingMessageLength < DATA_BUFSIZE;
              ++k )
          {
            //Filter terminal characters.
            if( SocketInfo->sBufferIn[ k ] >= 0x20
                || SocketInfo->sBufferIn[ k ] == '\n' 
              )
            {
              SocketInfo->sBufferIncomingMessage
                  [ ( SocketInfo->dwIncomingMessageLength )++ ]
                = SocketInfo->sBufferIn[ k ];
            }
          }
          
          //If there is a new line, process the buffer.
          if( SocketInfo->sBufferIncomingMessage
              [ SocketInfo->dwIncomingMessageLength - 1 ]
              == '\n' 
            )
          {
            processIncomingMessage( i );
          }

          //Clear the socket state.
          memset( SocketInfo->sBufferIn, 0, DATA_BUFSIZE );
          SocketInfo->dwBytesRECV = 0;
        }
      }//if( FD_ISSET( i, &readSet ) );

      if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
      {
        --dwTotal;
        if( -1 == ( dwSendBytes = 
                  send( SocketInfo->Socket, 
                        SocketInfo->sBufferSend, 
                        SocketInfo->dwBytesToSEND,
                         0)
                  ))
        {
          //failure
          printf("send on socket for channel %d failed.\n", i );
          FreeSocketInformation( i );
        }
        else
        {//send success
          SocketInfo->dwBytesToSEND -= dwSendBytes;
          printf("Sent %d bytes, %d bytes left.\n", 
                dwSendBytes, SocketInfo->dwBytesToSEND );

          //move the remaining buffer bytes up if the buffer wasn't sent.
          if( SocketInfo->dwBytesToSEND > 0 )
          {
            for( k = dwSendBytes; k < DATA_BUFSIZE; ++k )
            {
              SocketInfo->sBufferSend[ k - dwSendBytes ] 
                = SocketInfo->sBufferSend[ k ];
            }
          }
          else
          {
            //if the buffer is empty, clear it for good measure.
            memset( SocketInfo->sBufferSend, 0, DATA_BUFSIZE );
          }
        }//else OF if( send( ... ) == -! ) / send success
      }//if( FD_ISSET( SocketInfo->Socket, &writeSet ) )
    }//for( i = 0; i < dwTotalSockets; ++i )
  }//while( 1 )
  return 0;
}//int srv( int port ) (unix)

#endif
/*
  �[ 4 ] Socket Helper Functions
*/
/*
    �[4.1] int CreateSocketInformation( SOCKET s )
  
  Helper function to prepare the SocketInformation structure.

  Parameters:
    SOCKET s
      The socket on which a connection has been accepted, that the 
      SocketInformation structure will be created around.
  Returns:
    TRUE (WINAPI 1) if the call was succesful.
    FALSE (WINAPI 0) if there was a memory allocation failure.
*/
int CreateSocketInformation( SOCKET s )
{
  LPSOCKET_INFORMATION si;

#if defined( RM_WIN32_SYSTEM ) 
  #ifdef RM_DBG_WSA
  printf( "accepted socket number %d.\n", (int)s );
  #endif

  if( ( si = 
    (LPSOCKET_INFORMATION)GlobalAlloc(GPTR, sizeof(SOCKET_INFORMATION)) )
    == NULL )
  {
    printf( "GlobalAlloc() failed with error %d\n", GetLastError() );
    return FALSE;
  }

  #ifdef RM_DBG_WSA
  printf( "GlobalAlloc() for SOCKET_INFORMATION succeeded.\n" );
  #endif
#elif defined( RM_POSIX_SYSTEM )
  si = (SOCKET_INFORMATION*) calloc( 1, sizeof( SOCKET_INFORMATION) );
#endif
  //Prepare SocketInfo structure.
  si->Socket = s;
  si->dwBytesToSEND = 0;
  si->dwBytesRECV = 0;
  si->dwIncomingMessageLength = 0;
  snprintf( si->username, sizeof( si->username), 
            "guest%03d", dwTotalSockets );
  snprintf( si->room, sizeof( si->room), "#global" );

  SocketArray[ dwTotalSockets ] = si;
  ++dwTotalSockets;
  return TRUE;
}//BOOL CreateSocketInformation( SOCKET s )

/*
    �[4.2] void FreeSocketInformation( DWORD dwIndex )

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

  #if defined( RM_WIN32_SYSTEM )
  closesocket( si->Socket );
  #elif defined( RM_POSIX_SYSTEM )
  close( si->Socket );
  #endif

  #ifdef RM_DBG_WSA
  printf( "closing socket number %d (Index: %d).\n",
           (int) (si->Socket), (int) dwIndex );
  #endif
  
  
  //GlobalAlloc has reference counting - no free on WIN32.
  #if defined( RM_POSIX_SYSTEM )
  free( SocketArray[ dwIndex ] );
  #endif
  //vacuum socket array.
  for( i = dwIndex; i < dwTotalSockets; ++i )
  {
    SocketArray[ i ] = SocketArray[ i + 1 ];
  }

  --dwTotalSockets;
}//void FreeSocketInformation( DWORD dwIndex )

/*
  �[ 5 ] Message Processing Functions
*/
/*
    �[5.1] void printAllBuffer()

  Debugging function that prints the contents of buffers for all sockets.
*/
void printAllBuffer()
{
  unsigned int i;
  
  printf( "--begin print all buffers--\n" );
  for( i = 0; i < dwTotalSockets; ++i )
  {
    #if defined( RM_WIN32_SYSTEM )
    printf( "[%d] socket\nread(%d): %s %s\n", 
      i, 
      (int) ( SocketArray[i]->dwBytesRECV ),
      SocketArray[ i ]->DataBufIn.buf, 
      SocketArray[ i ]->sBufferIn 
      );
    #elif defined( RM_POSIX_SYSTEM )
    printf( "[%d] socket\nread(%d): %s\n", 
      i, 
      (int) ( SocketArray[i]->dwBytesRECV ),
      SocketArray[ i ]->sBufferIn 
    );
    #endif
  }
  printf( "--end print all buffers--\n" );
} //void printAllBuffer()

/*
  �[5.2] void debugShowIncomingBuffer( int id )

  Debug function, prints the characters and IDs of sBufferIncomingMessage
  for the SocketInformation in SocketArray specified by id.

  Parameters:
    int id
      The ID of the SocketInformation
  Returns:
    void.
*/
void debugShowIncomingBuffer( int id )
{
  int i;
  printf("[");
  for( i = 0; i < DATA_BUFSIZE; ++i )
  {
    if( i != 0 )
    {
      printf(","); 
    }
    printf( " (%d)(%c) ", 
      SocketArray[id]->sBufferIncomingMessage[i], 
      SocketArray[id]->sBufferIncomingMessage[i]
      );
    if( SocketArray[id]->sBufferIncomingMessage[i] == '\n' )
    {
      break;
    }
  }
  printf( "]\n" );
}//void debugShowIncomingBuffer( int id )

/*
  �[5.3] void queueMessage( int id, char * s, DWORD buf_len )

  Queues up a message to be sent on a socket by appending it to the end
  of the send buffer, as long as the message fits.

  TODO: If the message does not fit in the send buffer, add a 
        linkedlist to queue up messages to be sent later.

  Parameters:
    int id
      The id in SocketArray that corresponds to the 
      SocketInformation/Socket on which the data is to be sent.
    char* s
      A string containing the buffer
    DWORD dwBufLen
      The length of s.  This is checked to make sure that the message
      will fit in the buffer.

  Returns:
    Nothing.  TODO: Should be altered to return TRUE or FALSE later.
*/
void queueMessage( int id, char * s, int dwBufLen )
{
  #ifdef RM_DBG_WSA
  printf( "Queue message on [%d]. BytesToSend before: %d, Buf_len: %d\n",
        id,
        SocketArray[id]->dwBytesToSEND,
        dwBufLen );
  #endif
  if( SocketArray[id]->dwBytesToSEND + dwBufLen < DATA_BUFSIZE )
  { //only if it fits in buffer.
    memcpy( SocketArray[id]->sBufferSend + SocketArray[id]->dwBytesToSEND,
            s, 
            dwBufLen 
          );
    SocketArray[ id ]->dwBytesToSEND += dwBufLen;
  }
  else
  {
    printf( "The buffer is full.\n" );
  }
} //void queueMessage( int id, char * s, int buf_len )

/*
  �[5.4] void broadcastMessage( char* s, int dwBufLen )

  Sends the specified string to all users.  Used to broadcast server events
  as well as actual user messages.

  FUTURE:  Any room handling functionality will be implemented by extending
    and/or overloading this function.

  Parameters:
    char* s
      The string to broadcast.
    DWORD dwBufLen
      The length of the string.
  Returns:
    void
*/
void broadcastMessage( char* s, int dwBufLen )
{
  unsigned int i;
  for( i = 0; i < dwTotalSockets; ++i )
  {
    queueMessage( i, s, dwBufLen );
  }
}

/*
  �[5.5] void processIncomingMessage( int id )

  Processes the contents of sBufferIncomingMessage for the 
  SocketInformation in the SocketArray specified by id.  Before calling
  this, it should be verified that it is time to send a message/command
  (e.g. that the last character in the buffer is a '\n'). 

  If the message is a special command, this is handled by calling
  processIncomingMessageCommand(int).

  If the message is simply a message, it is sent to all other users,
  via broadcastMessage(char*, DWORD).

  The sBufferIncomingMessage is then cleared, and the
  dwIncomingMessageLength is set to 0.

  Parameters:
    int id
      The id in SocketArray of the SOCKET_INFORMATION containing the 
      message to be processed.
  Returns:
    void
*/

void processIncomingMessage( int id )
{
  char * ptr;
  char outBuf[ DATA_BUFSIZE ];
  #ifdef RM_DBG_WSA
  printf( "Incoming Message on [%d]: %s\n", 
          id,
          SocketArray[ id ]->sBufferIncomingMessage
        );
  debugShowIncomingBuffer( id );
  #endif
  //TODO: Terminal characters are not corrected for here.
  if( SocketArray[ id ]->sBufferIn[ 0 ] == '/' ) 
  {
    processIncomingMessageCommand( id );
  }
  else
  {
    //use strtok to properly terminate strings, and in case multiple 
    //messages appear in a single buffer.
    ptr = strtok( SocketArray[ id ]->sBufferIncomingMessage, "\r\n" );
    while( ptr != NULL )
    {
      //terminal character corrections
      if( ptr[ 0 ] == ' ' && ptr[ 1 ] == 0x27 )
      {
        ptr += 2;
      }
      snprintf( outBuf, sizeof( outBuf ), "%s:%s\r\n",
                SocketArray[ id ]->username,
                ptr );
      broadcastMessage( outBuf, strlen( outBuf ) );

      ptr = strtok( NULL, "\r\n" );
    }
  }

  //clear the message.
  memset( SocketArray[ id ]->sBufferIncomingMessage, 0, DATA_BUFSIZE );
  SocketArray[ id ]->dwIncomingMessageLength = 0;
} //void processIncomingMessage( int id )

/*
  �[5.6] void processIncomingMessageCommand( int id )

  If the incoming message is a command (begins with '/') call this to deal
  with the command.

  Parameters:
    int id
      The ID of the socket information on which the incoming message to be
      examined resides.
  Returns:
    void
*/
void processIncomingMessageCommand( int id )
{
  char outBuf[ DATA_BUFSIZE ];
  char * ptr;
  char * ptr2; 
  int i;
  //To reduce processing, check if the first character is the same as that 
  //of a valid the command.  Also provides shortcuts. 
  if( SocketArray[ id ]->sBufferIn[ 1 ] == 'n' )
  { 
    BOOL userRegCheckResult = FALSE;
    i = 0;
    //use strtok to get the second parameter (name)
    ptr = strtok( SocketArray[ id ]->sBufferIn, " \r\n\0" );    
    while( ptr != NULL )
    {
      if( i == 1 ) ptr2 = ptr;
      if( i == 2 ) break;
      ptr = strtok ( NULL, " \r\n\0" );
      ++i;
    }

    if( ptr != NULL )
    {
      //There is a password provided
      userRegCheckResult = userRegCheck( ptr2, ptr );
    }
    else
    {
      char emptyString[4];
      snprintf( emptyString, 4, "%s", "");
      userRegCheckResult = userRegCheck( ptr2, emptyString );
    }

    if( userRegCheckResult == TRUE )
    {
      //borrow outBuf to send a broadcast message.
      snprintf( outBuf, sizeof( outBuf ), 
              "User [%s] is now known as [%s]!\r\n",
              SocketArray[ id ]->username,
              ptr2
              );
      broadcastMessage( outBuf, strlen( outBuf ) );
      
      snprintf( 
        SocketArray[ id ]->username,
        sizeof( SocketArray[ id ]->username ),
        "%s",
        ptr2
        );
      snprintf( outBuf, sizeof( outBuf ), 
                "Your nickname is now '%s' \r\n", ptr2 );
    }
    else
    {
      snprintf( outBuf, sizeof( outBuf ), 
                "That username cannot be used.\r\n" );
    }
  }//nick command
  else if( SocketArray[ id ]->sBufferIn[ 1 ] == 'r' )
  {
    char * ptr2; 
    //this will hold the first parameter, ptr will hold the second.
    i = 0;
    ptr = strtok( SocketArray[ id ]->sBufferIn, " \r\n\0" );
    while( ptr != NULL )
    {
      if( i == 1 ) ptr2 = ptr;
      if( i == 2 ) break;
      ptr = strtok ( NULL, " \r\n\0" );
      ++i;
    }
  
    if( i == 2 )
    {
      //Check to see if the name is not registered.
      if( userRegCheck( ptr2, ptr ) == TRUE )
      {
        //If it's not registered, we can register it.
        if( userRegister( ptr2, ptr ) == TRUE )
        {
          printf(" Username: '%s' Password: '%s' ", ptr2, ptr );
          snprintf( outBuf, sizeof( outBuf ), 
                    "Username is now registered.\r\n" );
        }
        else
        {
          snprintf( outBuf, sizeof( outBuf ), 
                    "Username registration failed.\r\n" );
        }
      }//if( userRegCheck( ptr2, ptr ) == FALSE )
      else
      {
        snprintf( outBuf, sizeof( outBuf ), 
                  "Username already registered.\r\n" );
      }
    }
    else
    {
      snprintf( outBuf, sizeof( outBuf ), 
                "Invalid number of parameters for register.\r\n" );
    }
  }//register command
  else if( SocketArray[ id ]->sBufferIn[ 1 ] == 'w' )
  {
    queueWelcomeMessage( id );
    snprintf( outBuf, sizeof( outBuf ), 
              "Welcome message was requested by /welcome.\r\n");
  }
  else if( SocketArray[ id ]->sBufferIn[ 1 ] == 'h' )
  {
    snprintf( outBuf, sizeof( outBuf ), 
    "The following commands are available:\r\n"
    "help\r\n\tThis message.\r\n"
    "/nick <new_name> [<password>]\r\n"
    "\tChange your name. Provide password if registered.\r\n"
    "/register <password>\r\n"
    "\tSave your name.  You cannot save a name beginning with \"guest\".\r\n"
    "/welcome\r\n"
    "\tSends you the welcome message. Shows who is online, and your name.\r\n"
    );
  }
  else
  {
    snprintf( outBuf, sizeof( outBuf ), "That command is invalid.\r\n" );
  }
  queueMessage( id, outBuf, strlen( outBuf ) );
} //void processIncomingMessageCommand( int id )

/*
  �[5.7] void queueWelcomeMessage( int id )
  
  Generates the welcome message, and then posts it on the socket
  specified.

  Parameters:
    int id
      SocketArray index corresponding to the socket on which to send
      the message.
  Returns:
    void
*/

void queueWelcomeMessage( int id )
{
  unsigned int i;
  char buf[ DATA_BUFSIZE ];
  snprintf( buf, sizeof( buf ), 
    "Welcome to chat!  You are currently known as \"%s\"."
    "%i users are online:\r\n[",
    SocketArray[id]->username, dwTotalSockets 
    );
  for( i = 0; i < dwTotalSockets; ++i )
  {
    if( i != 0 )
    {
      strcat( buf, ", " );
    }
    strcat( buf, SocketArray[i]->username );
  }
  strcat( buf, "]\r\n" );
printf("Welcome:$$%s$$\n", buf );
  queueMessage( id, buf, strlen( buf ) );
} //void queueWelcomeMessage( int id )

/*
  �[ 6 ] User Registry Functions
*/
/*
  �[6.1] BOOL userRegCheck( char* username, char* password )

  Returns TRUE if the username is allowed to be used.

  That is the case if either
  -The username is not in the database (password is ignored).
  -The username is in the database, and the password matches the
    specified password.
  TODO:
  -and the username does not begin with "guest", as "guest###"
    is reserved.

  Parameters:
    char * username
      Null terminated string representing the username
    char * password
      Null terminated string representing the password

  Return value:
    TRUE (1) if the username is okay to use
    FALSE (0) if the username is not okay to use, or if password is 
      NULL.
*/
BOOL userRegCheck( char* username, char* password )
{
  printf("userRegCheck for \"%s\" and \"%s\".\n", username, password);
  #ifdef __cplusplus
    if( password == NULL )
    {
      return FALSE;
    }
    std::string username_string = std::string( username );
    if( dRegisteredUsers.find( username_string ) == dRegisteredUsers.end() )
    {
      //not in list, username use is allowed
      return TRUE;
    }
    else
    {
      //if in the list, verify that the password matches.
      if( dRegisteredUsers[ username_string ] == std::string( password ) )
      {
        return TRUE;
      }
      else
      {
        return FALSE;
      }
    }
  #else
    return TRUE;
  #endif

  return FALSE;
}//BOOL userRegCheck( char* username, char* password )

/*
    �[6.2] BOOL userRegister( char* username, char* password )

    Registers or updates a user in the registration database.

    If compiled in C++ the database is a map.

    If compiled in C, this function is disabled and the function
    returns false.

    Parameters:
      char * username
        the username to be registered.
      char * password
        the desired password for that username.

    Return:
      TRUE (1) if the function worked (C++)
      FALSE (0) if the function failed (compiled in C)
*/
BOOL userRegister( char * username, char * password )
{
  printf( "userRegister for \"%s\" and \"%s\".\n", username, password );
  //TODO: Save user and password file
  #ifdef __cplusplus
  dRegisteredUsers[ std::string( username ) ] = std::string( password );
  return TRUE;
  #endif
  return FALSE;
}//BOOL userRegister( char* username, char* password )

/*
  �[ 99] Main
*/
int main( void )
{
  //TODO: Load user and password file

  printf("�Result = %d\n", srv( DEF_PORT ) );
  return 0;
}
