/*! \file
 *
 * \brief TCP communication implementation
 */
#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
#else
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <netdb.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "mdi.h"
#include "mdi_tcp.h"
#include "communicator.h"
#include "mdi_global.h"

static int sigint_sockfd;
void sigint_handler(int dummy) {
  close(sigint_sockfd);
}

// TCP Method
int tcp_socket = -1;

int MDI_Listen_TCP(int port) {
  int ret;
  int sockfd;
  struct sockaddr_in serv_addr;
  int reuse_value = 1;

  // create the socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Could not create socket");
    return -1;
  }

  // ensure that the socket is closed on sigint
  sigint_sockfd = sockfd;
  signal(SIGINT, sigint_handler);

  // create the socket address
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  // enable reuse of the socket
  ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_value, sizeof(int));
  if (ret < 0) {
    perror("Could not reuse socket");
    return -1;
  }

  // bind the socket
  ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (ret < 0) {
    perror("Could not bind socket");
    return -1;
  }

  // start listening (the second argument is the backlog size)
  ret = listen(sockfd, 20);
  if (ret < 0) {
    perror("Could not listen");
    return -1;
  }

  //return sockfd;
  tcp_socket = sockfd;

  return 0;
}


int MDI_Request_Connection_TCP(int port, char* hostname_ptr) {
  int ret, sockfd;

  struct sockaddr_in driver_address;
  struct hostent* host_ptr;

  // get the address of the host
  host_ptr = gethostbyname((char*) hostname_ptr);
  if (host_ptr == NULL) {
    perror("Error in gethostbyname");
    return -1;
  }
  if (host_ptr->h_addrtype != AF_INET) {
    perror("Unkown address type");
    return -1;
  }

  bzero((char *) &driver_address, sizeof(driver_address));
  driver_address.sin_family = AF_INET;
  driver_address.sin_addr.s_addr = 
    ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
  driver_address.sin_port = htons(port);

  // create the socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Could not create socket");
    return -1;
  }

  // connect to the driver
  // if the connection is refused, try again
  //   this allows the production code to start before the driver
  int try_connect = 1;
  while (try_connect == 1) {
    ret = connect(sockfd, (const struct sockaddr *) &driver_address, sizeof(struct sockaddr));
    if (ret < 0 ) {
      if ( errno == ECONNREFUSED ) {

	// close the socket, so that a new one can be created
	ret = close(sockfd);
	if (ret < 0) {
	  perror("Could not close socket");
	  return -1;
	}

	// create the socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
	  perror("Could not create socket");
	  return -1;
	}

      }
      else { // only error out for errors other than "connection refused"
	perror("Could not connect to the driver");
	return -1;
      }

    }
    else {
      try_connect = 0;
    }
  }

  //Communicator* new_communicator = new CommunicatorTCP( MDI_TCP, sockfd );
  communicator new_comm;
  new_comm.method = MDI_TCP;
  new_comm.sockfd = sockfd;
  vector_push_back( &communicators, &new_comm );

  return 0;
}


int On_Accept_Communicator() {
  int connection;

  connection = accept(tcp_socket, NULL, NULL);
  if (connection < 0) {
    perror("Could not accept connection");
    exit(-1);
  }

  //Communicator* new_communicator = new CommunicatorTCP( MDI_TCP, connection );
  communicator new_comm;
  new_comm.method = MDI_TCP;
  new_comm.sockfd = connection;
  vector_push_back( &communicators, &new_comm );

  return 0;
}