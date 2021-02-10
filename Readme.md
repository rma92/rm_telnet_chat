## Telnet Chat Applicaion

I was asked some time ago to produce a demonstration application that utilizes the network select() function to handle many network connections on a single thread.

I have provided a single threaded telnet chat server.  This implementation should run on any version of Windows > 5.0 (Windows 2000), and also on POSIX systems.

This should compile cleanly with a C++ compiler.  The application can also be compiled with a C compiler, and basic functionality will work, but persistant storage of usernames will not work (as this is implemented using an std Map for simplicity, as it was uninteresting for the original demo).

### Functionality

Upon connection, a welcome message is provided to users (see queueWelcomeMessage() ).  All users see messages from all users.   

The main network processing is in a single long function (it’s difficult to split this into smaller functions).  Most other activity that gets called a lot (e.g. message queuing) is in additional functions. The basic structure, the main function calls the srv() function and provides the port number on which to listen.  This function then sets up the WinSock listener, sets the socket to non-blocking, and begins the select loop (the infinite while loop).

All connections listen for something to read.  Sockets only listen for write activity when there is a message to be sent.   

Data for each socket, as well as application level data such as usernames, are stored in the SOCKET_INFORMATION structure.

If the application is compiled with a C++ compiler, storage of a username and password list across server runs is supported.

### Code documentation

The code is well documented with comments, and explanations about each function.

Use the table of contents at the top of main.c to search for a particular function in question, by string searching for the section number in the brackets, e.g. "[ 99]" (within the quotes) to jump to the main function.
