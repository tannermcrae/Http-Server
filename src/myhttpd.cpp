
const char * usage =
"                                                               \n"
"myhttpd                                                        \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"myhttpd [-f|-t|-p] [<port>]                                    \n"
"    -f to run with processes                                   \n"
"    -t to run with threads                                     \n"
"    -p to run with thread pools                                \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                        \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server   \n"
"is running. <port> is the port number you used when you run    \n"
"daytime-server.                                                \n"
"                                                               \n"
"Then GET <document> <crlf><crlf> to have the doc returned      \n"
"You can also type <machine_name>:<port>/<document> in a browser\n"
"to have the file show up in the browser                        \n"
"                                                               \n";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h> 
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <string>
#include "send-response.h"

using std::string;

void sigHandler(int sig);
void listenForRequests(int mastersocket, int mode);
void processRequestThread( int socket);
void poolSlave(int socket);
void processRequest( int socket );
string getAbsoluteFilePath(string docPath);
string getContentType(string filePath);
int cmpReversedString(char *revString, char *fileExt);

pthread_mutex_t mutex;
struct sigaction sigAction;
int QueueLength = 5;

void sigHandler(int sig) {
   int status;
   if (sig == SIGCHLD) {
      while(waitpid(-1, &status, WNOHANG) > 0);
   }
}

int main( int argc, char ** argv ) {

	// Create signal handler for zombie processes
	sigAction.sa_handler = sigHandler;
	sigemptyset(&sigAction.sa_mask);
	sigAction.sa_flags = SA_RESTART;

  // Default normal mode
	int mode = 0; 
  // default?
	int port = 1025;

	// Check for a flag. If it's not a flag then argv[1] is the port number.
	if (argc > 1) {
	  if (strcmp(argv[1], "-f") == 0)
			mode = 1;
	  else if (strcmp(argv[1], "-t") == 0)
	  	mode = 2;
	  else if (strcmp(argv[1], "-p") == 0)
	  	mode = 3;
	  else
	    port = atoi( argv[1] );
	}
	// Argv[2] specifies the port number. 
	if(argc == 3)
		port = atoi(argv[2]);

	fprintf(stderr, "mode %d\n", mode);
 	fprintf(stderr, "port %d\n", port);
  
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if (masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
											(char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind(masterSocket,
		    					(struct sockaddr *)&serverIPAddress,
		    					sizeof(serverIPAddress));

  if (error) {
    perror("bind");
    exit( -1 );
  }

  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen(masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }
  // Start the listeners
  listenForRequests(masterSocket, mode);
  return 0;
}

/* This function listen for a request depending on the concurrency mode */
void listenForRequests(int masterSocket, int mode) {
	if (mode == 0) {
	  while (1) {
			// Accept incoming connections
			struct sockaddr_in clientIPAddress;
			int alen = sizeof( clientIPAddress );
			int slaveSocket = accept( masterSocket,
			                        (struct sockaddr *)&clientIPAddress,
			                        (socklen_t*)&alen);

			if (slaveSocket < 0) {
			  perror("accept");
			  exit(-1);
			}
			processRequest(slaveSocket);
			close(slaveSocket);
	  }
	}

	else if (mode == 1) {
	  while (1) {
  		// Accept incoming connections
      struct sockaddr_in clientIPAddress;
  		int alen = sizeof( clientIPAddress );
  		int slaveSocket = accept( masterSocket,
  		                        (struct sockaddr *)&clientIPAddress,
  		                        (socklen_t*)&alen);

  		if (slaveSocket < 0) {
  		  if (slaveSocket == -1 && errno == EINTR) continue;
  		  perror( "accept" );
  		  exit( -1 );
  		}
      pid_t slave = fork();
      if (slave==0) {
        processRequest(slaveSocket);
        close(slaveSocket);
        exit(0);
      }
      // Fork error
      else if (slave < 0) {
        perror("fork");
        exit(-1);
      }
      // Clean up zombie processes
      int z_error = sigaction(SIGCHLD, &sigAction, NULL);
      if ( z_error ) {
        perror( "sigaction" );
        exit(-1);
      }
      close(slaveSocket);
	  }
	}
	else if (mode == 2) {
	  while (1) {
	     // Accept incoming connections
	     struct sockaddr_in clientIPAddress;
	     int alen = sizeof( clientIPAddress );
	     int slaveSocket = accept( masterSocket,
	                           (struct sockaddr *)&clientIPAddress,
	                           (socklen_t*)&alen);

	     if (slaveSocket < 0) {
	        perror("accept");
	        exit(-1);
	     }
	     pthread_t thr;
	     pthread_attr_t attr;
	     pthread_attr_init(&attr);
	     pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	     pthread_create(&thr, &attr, 
                     (void * (*) (void *))processRequestThread,
                     (void *)slaveSocket);
	  }
	}

	else if (mode == 3) {
	  // Using poolthreads
	  pthread_mutex_init(&mutex, NULL);
	  pthread_attr_t attr;
	  pthread_attr_init(&attr);
	  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	  pthread_t tid[5];
	  for(int i=0; i<5; i++) {
      pthread_create(&tid[i], &attr, (void *(*)(void *))poolSlave, (void *)masterSocket);
      pthread_join(tid[i], NULL);
	  }
	}
}

void processRequestThread(int socket) {
  processRequest(socket);
  close(socket);
}

void poolSlave(int socket){
  while(1) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( socket,
                            (struct sockaddr *)&clientIPAddress,
                            (socklen_t*)&alen);

    if (slaveSocket < 0) {
       perror("accept");
       exit(-1);
    }
    pthread_mutex_unlock(&mutex);
    processRequest(slaveSocket);
    close(slaveSocket);
   }
} 

void processRequest(int socket) {
	int crlf_flag = 0;
	int iteration_count = 0;
  char newChar;
  char lastChar = 0;
	string docPath;
	string request_type;
  string filePath;
  string contentType;
  string currString = "";
  string query_string = "";
	// The client should send <name><cr><lf>
	// Read the name of the client character by character until a
	// <CR><LF> is found.
	//
  while (read(socket, &newChar, sizeof(newChar)) > 0) {
   	iteration_count++;
    // Spaces indicate different parts of the request.
    if (newChar == ' ') {
      if (request_type.empty())
        request_type = currString;
			else if(docPath.empty())
        docPath = currString;
      currString = "";
    }
    // Check for two crlf's in a row which signals end of request.
    else if ( lastChar == '\015' && newChar == '\012' ) {
      if (iteration_count-2 == crlf_flag)
        break;
      else
        crlf_flag = iteration_count;
    }
    else {
			currString += newChar;
			lastChar = newChar;
	  }
  }

  // Make the default file server send index.html
  if (docPath == "/")
    docPath = (char *)"/index.html";
  // Check for variables for cgi scripts
  int delimeter = docPath.find("?",0);
  if (delimeter!=-1) {
    query_string = docPath.substr(delimeter+1);
    docPath = docPath.substr(0,delimeter);
  }

  filePath = getAbsoluteFilePath(docPath);
	// Test if filePath points to a directory.
	DIR *dir = opendir(filePath.c_str());
	if (dir) {
    browseDirectory(socket, dir, docPath, filePath, query_string);
  }
  else {
    contentType = getContentType(docPath);
    int file = open(filePath.c_str(), O_RDONLY);
    // Return 404 if the file is not found
    if (file == -1)
      write404(socket, "404 File not Found");
    else if (docPath.find("/cgi-bin")!=-1)
      writeCGI(socket, filePath, request_type, query_string);
    else
      writeRequest(socket, file, contentType);
  }
}

string getAbsoluteFilePath(string docPath) {
  char *cwd = (char *)malloc(sizeof(char)*256);
  cwd = getcwd(cwd,256);
  string filePath = cwd;
  if (docPath.find("/icon")!=-1 || docPath.find("/htdocs")!=-1 || docPath.find("/cgi-bin")!=-1)
    filePath = strdup(cwd) + (string)"/../http-root-dir" + docPath;
  else 
    filePath = strdup(cwd) + (string)"/../http-root-dir/htdocs" + (string)docPath;
  free(cwd);
  return filePath;
}

string getContentType(string docPath) {
  if(docPath.find(".html") != -1)
    return (string)"text/html";
  else if(docPath.find(".gif") != -1)
    return (string)"image/gif"; 
  else
    return (string)"text/plain";
}









