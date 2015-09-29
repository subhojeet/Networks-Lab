#include "common.h"

volatile int stop;

char next_frame_to_send = 0x00;
char msg_to_send[MAXDATASIZE];
float error_rate;
int msg_to_send_len = 0, sockfd;

void sigalrm_handler( int sig )
{
    printf("Timer has expired, Sending the packet again\n"); fflush(stdout);
	char control = (( 0x07 & next_frame_to_send) << 4);
	printf("\n Sending frame with seq no %d", next_frame_to_send); fflush(stdout);
	send_msg(sockfd, msg_to_send, msg_to_send_len, control, error_rate);
	alarm(3);
}

int main(int argc, char *argv[])
{
	int numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	srand((unsigned)time(NULL));

	// Signal handler for alarm
	struct sigaction sact;
	sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sact.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sact, NULL);


	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
 
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s); fflush(stdout);

	freeaddrinfo(servinfo); // all done with this structure
	int work = 1;   		// 1 to send message.  2 to request file.  3 to decide

	while(1)
	{
		start:
		//Taking input message
	// 	if (work == 3)
	// 	{
	// xyz:	int whattodo;
	// 		printf("\n\n\nWhat do you want to do?  1)Send message  or  2)Request file.");
	// 		scanf("%d",&whattodo);
	// 		switch(whattodo)
	// 		{
	// 			case 1:
	// 					work = 1;
	// 					break;
	// 			case 2:
	// 					work = 2;
	// 					break;
	// 			default:
	// 					goto xyz;
	// 		}
	// 	}
		
		if (work == 1)
		{
			printf("\nEnter the message to be sent to server: \n"); fflush(stdout);
			scanf("%s",msg_to_send);
			msg_to_send_len = strlen(msg_to_send);
		}

		// else if (work == 2)
		// {
		// 	printf("\nEnter file name to request. \n"); fflush(stdout);
		// 	scanf("%s",file_name);
		// }
		
		// Taking error rate
		// printf("\n Give the probability error");
		// scanf("%f",&error_rate);
		// while(error_rate>1)
		// {
		// 	printf("\n Give the probability error");
		// 	scanf("%f",&error_rate);	
		// }
		error_rate = 0.001;

		char control = (( 0x07 & next_frame_to_send) << 4);
		printf("\n Sending frame with seq no %d", next_frame_to_send); fflush(stdout);
		send_msg(sockfd, msg_to_send, msg_to_send_len, control, error_rate);

		stop = 0;  
		alarm(3);			// Initializing the timer

		while(1)
		{
					printf( "\n Waiting for ack in a loop."); fflush(stdout);
					numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
					if ( numbytes == 0) {
					    printf("\nconnection closed by server ");  fflush(stdout);
					    close(sockfd);
					    exit(1);
					}
					else if(numbytes == -1)
					{
						fprintf(stderr,"\nError during receiving a frame");
						// The client is interrupted by system call.
						continue;
					}
					printf("\nclient: received '%s' of %d bytes.\n",buf,numbytes); fflush(stdout);

					if(check_error(buf,numbytes)==1) // error detected in the frame received
					{
						printf("\nCRC Check failed  \n"); fflush(stdout);
						continue;
					}
					else // There is no error in received frame.
					{
						if ( (char)(buf[CONTROL] & 0xC0) == (char)(0x80) )
						{
							//Supervisory frame
							if( (char)(buf[CONTROL] & 0x30) == (char)REJ )
							{
								alarm(0);
								printf("\nNak was received.\n"); fflush(stdout);
								char control = (( 0x07 & next_frame_to_send) << 4);
								printf("\n Sending frame with seq no %d", next_frame_to_send); fflush(stdout);
								send_msg(sockfd, msg_to_send, msg_to_send_len, control ,error_rate);
								alarm(3);
							}
							else if ( (char)(buf[CONTROL] & 0x30) == (char)RR )
							{
								char temp = inc(next_frame_to_send);
								if ( (char)(buf[CONTROL] & 0x07) == (char)temp )
								{
									alarm(0);
									next_frame_to_send = inc(next_frame_to_send);
									printf("\ncorrect ack was received \n"); fflush(stdout);
									goto start;
								}
								else
								{
									char control = (( 0x07 & next_frame_to_send) << 4);
									printf("\n incorrect ack was received %hhx", buf[CONTROL]); fflush(stdout);
									printf("\n expected ack is %d. next frame to send is %d", temp, next_frame_to_send);
									printf("\n Sending frame with seq no %d", next_frame_to_send); fflush(stdout);
									send_msg(sockfd, msg_to_send, msg_to_send_len, control, error_rate);
									alarm(3);
								}
							}
						}
						else
						{
							fprintf(stderr, "Recieved frame format not recognized\n");
						}
					}
			}		
		}	
	close(sockfd);
	return 0;
}
