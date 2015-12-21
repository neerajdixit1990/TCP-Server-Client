#include<stdio.h>
#include<time.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>

#define	SERVICE_PORT	51839
int main(int argc, char *argv[])
{
	int 				soc_fd, status, no_bytes, pipe_write_end;
	char				buf[1000], *ret_ptr = NULL, *temp = NULL;
	struct sockaddr_in 	my_addr;
	
	/* Get IPC pipe write descriptor */
	pipe_write_end = atoi(argv[2]);
	
	/* Check number of arguements */
	if (argc != 3) {
		temp = "\nPlease call ECHO with proper arguements : echocli <serv add> <fd>";
		snprintf(buf, strlen(temp), temp);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on echo service port ...");
			getchar();
		}
		return 0;
	}
    
	/* Socket creation, check for errors */
	soc_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (soc_fd < 0) {
		temp = "\nStatus = %d, Unable to create ECHO service socket from client !!!";
		snprintf(buf, strlen(temp)+3, temp, soc_fd);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on echo service port ...");
			getchar();
		}
		return 0;
	}
	
	temp = "Echo socket created ...";
	snprintf(buf, strlen(temp), temp);
	status = write(pipe_write_end, buf, strlen(buf));
	if (status <= 0) {
		printf("\nCannot write to pipe on echo service port ...");
		getchar();
	}
	
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SERVICE_PORT);
    inet_pton(AF_INET, argv[1], &my_addr.sin_addr);
	
	/* Socket connect, check for errors */
	status = connect(soc_fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	if (status != 0) {
		temp = "\nStatus = %d, Unable to connect echo service socket from client !!!\n";
		snprintf(buf, strlen(temp), temp, status);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on echo service port ...");
			getchar();
		}
		return 0;	
	}
	
	temp = "\nEcho socket connect successful ...\n";
	snprintf(buf, strlen(temp), temp);
	status = write(pipe_write_end, buf, strlen(buf));
	if (status <= 0) {
		printf("\nCannot write to pipe on echo service port ...");
		getchar();
	}

	while(1) {
		fd_set 		mon_fd;
		
		FD_ZERO(&mon_fd);
		FD_SET(soc_fd, &mon_fd);
		FD_SET(fileno(stdin), &mon_fd);
		
		/* Monitor keyboard input and socket input */
		status = select(soc_fd + 1, &mon_fd, NULL, NULL, NULL);
		if (status < 0) {
			printf("\nStatus = %d, Unable to monitor sockets !!! Exiting ...\n",status);
			return 0;
		}
		
		if (FD_ISSET(fileno(stdin), &mon_fd)) {
			ret_ptr = fgets(buf, 100, stdin);	
			if (ret_ptr == NULL) {
				break;
			}
			
			no_bytes = send(soc_fd, buf, strlen(buf)+1, 0);
			if (no_bytes <= 0) {
				printf("\nCannot send data on echo service port ...");
			} else if (no_bytes < strlen(buf)) {
				printf("\nPartial send occurred ...");
			}
		} else if (FD_ISSET(soc_fd, &mon_fd)) {
			no_bytes = read(soc_fd, buf, sizeof(buf));
			if (no_bytes > 0) {
				printf("Server : %s\n",buf);
			} else if (no_bytes == 0) {
				/* Server termination will lead to a 0 read on this socket */
				temp = "Connection terminated by echo service server ...";
				snprintf(buf, strlen(temp), temp);
				status = write(pipe_write_end, buf, strlen(buf));
				if (status <= 0) {
					printf("\nCannot write to pipe on time service port ...");
					getchar();
				}
				goto cleanup;
			} else {
				printf("\nCannot receive data on echo service port ...");
				goto cleanup;
			}   
		} else {
			printf("\nAbnormal return by select() function ...");
		}
	}

/* Close all the file descriptors and sockets in case server crashes */
cleanup:	
	status = close(soc_fd);
	if (status != 0) {
		temp = "\nStatus = %d, Unable to close echo service socket on client side !!!";
		snprintf(buf, strlen(temp), temp, status);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on time service port ...");
			getchar();
		}
	}	
	temp = "\nEcho socket closed ...";
	snprintf(buf, strlen(temp), temp);
	status = write(pipe_write_end, buf, strlen(buf));
	if (status <= 0) {
		printf("\nCannot write to pipe on echo service port ...");
		getchar();
	}
	
	status = close(pipe_write_end);
	if (status != 0) {
		printf("\nStatus = %d, Unable to close pipe write end !!!",status);
	}
	
 	return 0;
}
