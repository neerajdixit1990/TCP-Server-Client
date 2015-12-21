#include<stdio.h>
#include<time.h>
#include<ctype.h>
#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<netdb.h>

/* SIGCHLD signal handler registered in main function 
   Wait for all dead children but do not HANG as some children
   might still be in runnning state.
   
   The OS will generate a SIGCHLD once they are terminated and
   they will be handled by that call to handler */
   
static void
child_process_exit(int sig)
{
	int 	ret = 0;
	while(waitpid(-1, &ret, WNOHANG) > 0);
}

int main(int argc, char *argv[])
{
	int 				no_bytes, ret_status, i;
	char				msg[100], buf[1000], pipe_des[100], option;
	char				ip_addr[INET_ADDRSTRLEN];
	const char			*temp = NULL;
	struct hostent 		*name_check = NULL;
	struct sockaddr_in 	addr;
	pid_t   			status;
	struct sigaction 	handler;
	struct in_addr 		**addr_list = NULL;

	/* Check number of arguements */
	if (argc != 2) {
		printf("\nPlease call client with proper arguements : client <IPaddrr>\n");
		return 0;
	}

	/* Validate the IP address of the server. Connect will fail if wrong
	   server IP address is given */
	ret_status = inet_pton(AF_INET, argv[1], &(addr.sin_addr)); 
	if (ret_status <= 0) {
		printf("\nCommand line has hostname as arguement");
		name_check = gethostbyname(argv[1]);
		if (name_check == NULL) {
			printf("\nNo IP address associated with %s\n", argv[1]);
		} else {
			printf("\nOfficial IP address are:\n");
		    addr_list = (struct in_addr **)name_check->h_addr_list;
			for(i = 0; addr_list[i] != NULL; i++) {
				printf("%s\n", inet_ntoa(*addr_list[i]));
			}
		}
	} else {
		printf("\nCommand line has IP address as arguement");
		name_check = gethostbyaddr(&addr, sizeof(addr), AF_INET);
		if (name_check == NULL) {
			printf("\nNo hostname associated with %s\n", argv[1]);
		} else {
			printf("\nOfficial name is: %s\n", name_check->h_name);
		}
 	}
	
	/* Register for SIGCHLD signal */
    /*sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    handler.sa_handler = child_process_exit;
    ret_status = sigaction(SIGCHLD, &handler, NULL);
	if (ret_status != 0) {
		printf("\nStatus = %d, Error in registering child handler !!! Continuing ...",ret_status);
	}*/

	while (1) {
		int		ipc_pipe[2];
		
		/* Initiate pipe communication */
		ret_status = pipe(ipc_pipe);
		if (ret_status != 0) {
			printf("\nStatus = %d, Error in creating pipe !!! Exiting ...\n",ret_status);
			return 0;
		}
		
		printf("\n1) Echo");
		printf("\n2) Time");
		printf("\n3) Quit");
		printf("\nEnter any of the following options:\t");
		scanf(" %c", &option);
		
		if (option == '1' || option == '2') {
			printf("===== CHILD Status messages =====");
			status = fork();
			if (status == 0) {
				/* === Child process === */
				/* Close the read end */
				ret_status = close(ipc_pipe[0]);
				if (status != 0) {
					printf("\nStatus = %d, Unable to close IPC pipe on client side !!!",status);
				}
				
				/* convert pipe descriptor to string so that we can send it as arguement */
				snprintf(pipe_des, 100, "%d", ipc_pipe[1]);
				if (option == '1') {
					/* Spawn xterm window with appropriate executable */
        			execlp("xterm", "xterm", "-e", "./echocli", argv[1], pipe_des, (char *)0);
				} else {
					execlp("xterm", "xterm", "-e", "./timecli", argv[1], pipe_des, (char *)0);
				}
			} else if (status > 0) {
				/* === Parent process === */
				/* Close the write end */
				ret_status = close(ipc_pipe[1]);
				
				/* Pipe messages from client child window */
				while(1) {
                	no_bytes = read(ipc_pipe[0], buf, 1000);
                	if (no_bytes > 0) {
						buf[no_bytes] = '\0';
						printf("\n%s", buf);
					} else if (no_bytes == 0) {
						printf("\nClient child exiting ...");
						break;
					}
				}
				printf("\n=================================\n");				
				ret_status = close(ipc_pipe[0]);
			} else {
				printf("\nStatus = %d, fork() failed !!!",status);
			}
		} else if (option == '3') {
			break;
		} else {
			printf("\nPlease enter the correct choice(1-3):\t");
		}
	}
	return 0;
}
