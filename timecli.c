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
#define	SERVICE_PORT	51838

int main(int argc, char *argv[])
{
	time_t 				timeofday;
	int 				soc_fd, status, no_bytes, pipe_write_end;
	char				buf[1000], *temp = NULL;
	struct sockaddr_in 	my_addr;
	
	/* Get IPC pipe write descriptor */
	pipe_write_end = atoi(argv[2]);
	
	/* Check number of arguements */
	if (argc != 3) {
		temp = "\nPlease call TIME with proper arguements : timecli <serv add> <fd>";
		status = write(pipe_write_end, temp, strlen(temp));
		if (status <= 0) {
			printf("\nCannot write to pipe on time service port ...");
			getchar();
		}
		return 0;
	}
	
	/* Socket creation, check for errors */
	soc_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (soc_fd < 0) {
		temp = "\nStatus = %d, Unable to create TIME service socket from client !!!";
		snprintf(buf, strlen(temp)+3, temp, soc_fd);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on time service port ...");
			getchar();
		}
		return 0;
	}
	
	temp = "Time socket created ...";
	snprintf(buf, strlen(temp), temp);
	status = write(pipe_write_end, buf, strlen(buf));
	if (status <= 0) {
		printf("\nCannot write to pipe on time service port ...");
		getchar();
	}
	
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SERVICE_PORT);
    inet_pton(AF_INET, argv[1], &my_addr.sin_addr);
	
	/* Socket connect, check for errors */	
	status = connect(soc_fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	if (status != 0) {
		temp = "\nStatus = %d, Unable to connect time service socket from client !!!\n";
		snprintf(buf, strlen(temp)+3, temp, status);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on time service port ...");
			getchar();
		}
		return 0;	
	}
	
	temp = "\nTime socket connect successful ...\n";
	snprintf(buf, strlen(temp), temp);
	status = write(pipe_write_end, buf, strlen(buf));
	if (status <= 0) {
		printf("\nCannot write to pipe on time service port ...");
		getchar();
	}

	while(1) {
		no_bytes = recv(soc_fd, buf, sizeof(buf),0);
		if (no_bytes > 0) {
			printf("\nReceived time from server : %s", buf);
		} else if (no_bytes == 0) {
			/* Server termination will lead to a 0 read on this socket */
			temp = "Connection terminated by time service server ...";
			snprintf(buf, strlen(temp), temp);
			status = write(pipe_write_end, buf, strlen(buf));
			if (status <= 0) {
				printf("\nCannot write to pipe on time service port ...");
				getchar();
			}
			break;
		} else {
			printf("\nCannot receive data on time service port ...");
			break;
		}
	}

/* Close all the file descriptors and sockets in case server crashes */	
cleanup:
	status = close(soc_fd);
	if (status != 0) {
		temp = "\nStatus = %d, Unable to close time service socket on client side !!!\n";
		snprintf(buf, strlen(temp)+3, temp, status);
		status = write(pipe_write_end, buf, strlen(buf));
		if (status <= 0) {
			printf("\nCannot write to pipe on time service port ...");
			getchar();
		}
	}
	temp = "\nTime socket closed ...";
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
