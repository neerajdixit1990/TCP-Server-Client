#include<stdio.h>
#include<time.h>
#include<ctype.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<netinet/in.h>
#include<errno.h>

#define TIME_SERVICE_PORT   51838
#define	ECHO_SERVICE_PORT	51839
#define	BACKLOG_LIMIT		20

/* Enum to distinguish between TIME service and ECHO service */
typedef enum {
	ECHO_SERVICE,
	TIME_SERVICE,
	MAX_SERVICE
} type_of_service;

/* Service node given to threads 
   We can add extra data that is to be passed tp thread here */
typedef struct service_nodes {
	int		service_type;
	int		fd;
	char	ip_addr[129];
} service_node;

/* Signal handler for SIGPIPE */
static void
server_sigpipe_handler(int sig)
{
	printf("\nIgnoring the SIGPIPE signal : signal %d",sig);
}

/* Thread handler when new thread for any service is created */
void *service_handler( void 	*ptr )
{
	char			msg[100];
	service_node	*data = (service_node *)ptr;
	time_t          timeofday;
	int				no_bytes, status;
	struct timeval 	timeout;
	
	if (data->service_type == ECHO_SERVICE) {
		printf("\nRecived request for echo service from @ %s", data->ip_addr);
		while(1) {
			no_bytes = recv(data->fd, msg, sizeof(msg), 0);
			if (no_bytes > 0) {
				status = send(data->fd, msg, sizeof(msg), 0);
				if (status <= 0) {
					/* Handle some signals from send/recv */
					if (errno == EPIPE) {
						printf("\nServer received EPIPE signal ... Exiting\n");
					} else if (errno == EINTR) {
						printf("\nServer received EINTR signal ... Exiting\n");
					} else {
						printf("\nCannot send data on echo service port ...");
					}
					break;
				}
			} else if (no_bytes == 0) {
				/* Server termination will lead to a 0 read on this socket */
				printf("\nConnection terminated by echo service @ %s", data->ip_addr);
				break;
			} else {
				printf("\nCannot receive data on echo service port ...");
				if (errno == EINTR) {
					printf("\nServer received EINTR signal ... Exiting\n");
				} else {
					printf("\nCannot send data on echo service port ...");
				}
				break;
			}
		}
	} else if (data->service_type == TIME_SERVICE) {
		printf("\nRecived request for time service from @ %s", data->ip_addr);
		while(1) {
			fd_set 		read_fd;
			
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
		
			FD_ZERO(&read_fd);
			FD_SET(data->fd, &read_fd);
			
			/* Monitor for server termination on the socket 
			   else wait for 5 seconds to give time again */
			status = select(data->fd + 1, &read_fd, NULL, NULL, &timeout);
			if (status < 0) {
				printf("\nStatus = %d, Unable to monitor sockets !!! Exiting ...",status);
				return 0;
			}
			
			if (FD_ISSET(data->fd, &read_fd)) {
				status = recv(data->fd, msg, sizeof(msg),0);
				if (status == 0) {
					printf("\nConnection terminated by time service @ %s", data->ip_addr);
					break;
				} else if (status > 0) {
					printf("\nUn-expected write from time service client ...");
				} else {
					if (errno == EINTR) {
						printf("\nServer received EINTR signal ... Exiting\n");
					} else {
						printf("\nCannot send data on time service port ...");
					}
				}
			} else {
				/* Generate time for request */
				timeofday = time(NULL);
				snprintf(msg, 100, "%s", ctime(&timeofday));
				status = send(data->fd, msg, sizeof(msg), 0);
				if (status <= 0) {
					if (errno == EPIPE) {
						printf("\nServer received EPIPE signal ... Exiting\n");
					} else if (errno == EINTR) {
						printf("\nServer received EINTR signal ... Exiting\n");
					} else {
						printf("\nCannot send data on time service port ...");
					}
					break;
				}
			}
		}
	} else {
		printf("\nWrong context data passed to thread handler !!!");
	}
	
	/* Close socket after use */
	status = close(data->fd);
	if (status != 0) {
		printf("\nStatus = %d, Unable to close socket on server side !!!",status);
	}
}
int main(int argc, char *argv[])
{
	int						time_soc, echo_soc, status, new_fd, soc_option = 1;
	struct sockaddr_storage	in_data;
	struct sockaddr_in 		server;
	socklen_t				in_len;
	char                    str_out[100];
	pthread_t 				thread_id;
	service_node			data;
	struct sigaction 		handler;
	
	/* Register for SIGPIPE signal */
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    handler.sa_handler = server_sigpipe_handler;
    status = sigaction(SIGCHLD, &handler, NULL);
	if (status != 0) {
		printf("\nStatus = %d, Error in registering for SIGPIPE handler !!! Continuing ...",status);
	}
	
	printf("\n ===== SERVER Status ===== \n");
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (TIME_SERVICE_PORT);

	/* Create time socket, check for errors */
    time_soc = socket(AF_INET, SOCK_STREAM, 0);
	if (time_soc < 0) {
		printf("\nStatus = %d, Unable to create time service socket !!!",time_soc);
		return 0;
	}
	printf("\nTime socket created ...");
	
	/* Set socket to non blocking mode */
	status = fcntl(time_soc, F_SETFL, O_NONBLOCK);
	if (status != 0) {
		printf("Status = %d, Unable to set port to non-blocking mode !!! Continuing ...",status);
	}
	
	/* Set REUSE flag for time socket */
	soc_option = 1;
	status = setsockopt(time_soc, SOL_SOCKET, SO_REUSEADDR, &soc_option, sizeof(int));
	if (status != 0) {
		printf("Status = %d, Unable to set time port to REUSE_ADDR mode !!! Continuing ...",status);
	}

	/* Create echo socket, check for errors */
    echo_soc = socket(AF_INET, SOCK_STREAM, 0);
	if (echo_soc < 0) {
		printf("\nStatus = %d, Unable to create echo service socket !!!",echo_soc);
		return 0;
	}
	printf("\nEcho socket created ...");
	
	// If we set this to non blocking mode, the echo_soc would not stop for input
	// so dont set this socket to non-blocking 
	
	/*status = fcntl(echo_soc, F_SETFL, O_NONBLOCK);
	if (status != 0) {
		printf("Status = %d, Unable to set port to non-bloacking mode !!! Continuing ...",status);
	}*/

	/* Set REUSE flag for echo socket */
	soc_option = 1;
	status = setsockopt(echo_soc, SOL_SOCKET, SO_REUSEADDR, &soc_option, sizeof(int));
	if (status != 0) {
		printf("Status = %d, Unable to set echo port to REUSE_ADDR mode !!! Continuing ...",status);
	}
	
	/* Bind time socket, check for errors */
    status = bind(time_soc, (struct sockaddr *)&server, sizeof(server));
	if (status != 0) {
		printf("\nStatus = %d, Unable to bind time service socket !!!",status);
		return 0;
	}
	printf("\nTime socket bind successful ...");
	
	/* Listen time socket, check for errors */
	status = listen(time_soc, BACKLOG_LIMIT);
	if (status != 0) {
		printf("\nStatus = %d, Unable to listen time service socket !!!",status);
		return 0;
	}
	printf("\nTime socket listen successful ...");
	
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (ECHO_SERVICE_PORT);
	
	/* Bind echo socket, check for errors */
    status = bind(echo_soc, (struct sockaddr *)&server, sizeof(server));
	if (status != 0) {
		printf("\nStatus = %d, Unable to bind echo service socket !!!",status);
		return 0;			
	}
	printf("\nEcho socket bind successful ...");

	/* Listen echo socket, check for errors */
    status = listen(echo_soc, BACKLOG_LIMIT);
	if (status != 0) {
		printf("\nStatus = %d, Unable to listen echo service socket !!!",status);
		return 0;			
	}
	printf("\nEcho socket listen successful ...");

	printf("\nServer up .. Waiting for connections ...\n");
	printf("\n =========================\n");
	while(1) {
		fd_set 		read_fd, except_fd;

		FD_ZERO(&read_fd);
		FD_SET(echo_soc, &read_fd);
		FD_SET(time_soc, &read_fd);

		FD_ZERO(&except_fd);
		FD_SET(echo_soc, &except_fd);
		FD_SET(time_soc, &except_fd);
		
		/* Monitor the time & echo socket for reads */
		if (echo_soc > time_soc)
			status = select(echo_soc + 1, &read_fd, NULL, &except_fd, NULL);
		else
			status = select(time_soc + 1, &read_fd, NULL, &except_fd, NULL);
		
		if (status < 0) {
			printf("\nStatus = %d, Unable to monitor sockets !!! Exiting ...",status);
			return 0;
		}

		if (FD_ISSET(echo_soc, &read_fd)) {
			/* New request for echo service */
			in_len = sizeof(in_data);
           	new_fd = accept(echo_soc, (struct sockaddr *)&in_data, &in_len);
			if (new_fd < 0) {
				printf("\nStatus = %d, Unable to accept connections on echo socket !!! Exiting ...",new_fd);
				return 0;
			}
			/* Fill in the service structure */
			data.service_type = ECHO_SERVICE;
			data.fd = new_fd;
			inet_ntop(AF_INET, &((struct sockaddr *)&in_data)->sa_data, &data.ip_addr, INET_ADDRSTRLEN);
			
			/* Create separate thread to act on service */
			status = pthread_create( &thread_id, NULL, service_handler, (void*) &data);
			if (status != 0) {
				printf("\nStatus = %d, Unable to create thread for echo socket !!! Exiting ...",status);
				return 0;
			}
		} else if (FD_ISSET(time_soc, &read_fd)) {
			/* New request for time service */
			in_len = sizeof(in_data);
            new_fd = accept(time_soc, (struct sockaddr *)&in_data, &in_len);
			if (new_fd < 0) {
				printf("\nStatus = %d, Unable to accept connections on time socket !!! Exiting ...",new_fd);
				return 0;
			}
			/* Fill in the service structure */
			data.service_type = TIME_SERVICE;
			data.fd = new_fd;
			inet_ntop(AF_INET, &((struct sockaddr *)&in_data)->sa_data, &data.ip_addr, INET_ADDRSTRLEN);
			
			/* Create separate thread to act on service */
			status = pthread_create( &thread_id, NULL, service_handler, (void*) &data);
			if (status != 0) {
				printf("\nStatus = %d, Unable to create thread for time socket !!! Exiting ...",status);
				return 0;
			}
		} else if (FD_ISSET(echo_soc, &except_fd) || FD_ISSET(time_soc, &except_fd)) {
			printf("\nException in socket ...Exiting ...\n");
			return 0;
		} else {
			printf("\nTimed out... Trying again");
		}
	}

	/* Close the time and echo sockets in case of graceful exit */
	status = close(echo_soc);
	if (status != 0) {
		printf("\nStatus = %d, Unable to close echo service socket on server side !!!",status);
	}
	
	status = close(time_soc);
	if (status != 0) {
		printf("\nStatus = %d, Unable to close time service socket on server side !!!",status);
	}
	return 0;
}
