#include "common.h"

#define BACKLOG 10	 // how many pending connections queue will hold
#define err_prob 0.06

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int sockfd;

/* Signal Handler for SIGINT (control-c event) */
void siginthandler(int sig_num)
{
    signal(SIGINT, siginthandler);
    printf("\n Socket successfully closed.No more connections can be accepted \n"); fflush(stdout);
    fflush(stdout);

    close(sockfd);
    exit(0);
}


int main(void)
{
	int  new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN],buf[MAXDATASIZE];
	int rv,numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{

		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	signal(SIGINT,siginthandler);

	printf("server: waiting for connections...\n"); fflush(stdout);

	while(1) 
	{  
		// main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		
		if (new_fd == -1) 
		{
			fprintf(stderr,"\nError during accepting a call.");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s); fflush(stdout);

		if (!fork()) 
		{ 
			// this is the child process
			char msg[MAXDATASIZE];
			//Variables maintaining state of the connection.
			char frame_expected = 0x00;
			char next_frame_to_send = 0x00;
			char ack_expected = 0x00;
			int no_nak = 1;
			int network_layer_enabled = 1;
			int read_fifo, write_fifo;
			float error_rate = 2;
			while (error_rate > 1 || error_rate < 0)
			{
				printf("\nPlease input error rate to be used for new connection: "); fflush(stdout);
				scanf("%f",&error_rate);
			}

			int child_pid;
			if ( (child_pid = fork()) == 0)
			{
				char *newargv[3] = {NULL,NULL};
				char *newenviron[] = { NULL };

				if ( execve("/home/satya/Desktop/networks/upperlayer", newargv, newenviron) < 0 )
					fprintf(stderr, "\nProcess %d: Couldn't load child process with upper layer code.", getpid());
			}
			
			if (child_pid == -1)
				fprintf(stderr,"\nProcess %d: Couldn't create child process for new connection.", getpid());
			else
			{
				char fifoname[12]; 							//This is used for reading from lower layer.
				sprintf(fifoname, "%d_0.txt", child_pid); 		
				//printf("Fifo:%s\n",fifoname);
				
				if (mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
					fprintf(stderr, "Process %d: Error during creating write fifo", getpid());
				
				write_fifo = open(fifoname, O_WRONLY);
				if (write_fifo == -1)
					fprintf(stderr,"\nProcess %d: Error during opening write fifo", getpid());


				sprintf(fifoname, "%d_1.txt", child_pid); 
				//printf("Fifo:%s\n",fifoname);
				
				if (mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
					fprintf(stderr,"Process %d: Error during creating read fifo", getpid());

				read_fifo = open(fifoname, O_RDONLY);
				if (read_fifo == -1)
					fprintf(stderr,"\nProcess %d: Error during opening read fifo", getpid());
			}

			while(1)
			{
				if ((numbytes = recv(new_fd, msg, MAXDATASIZE-1, 0)) == -1) 
				{
			   		fprintf(stderr, "\nError during receiving data.");
			   		continue;
			  	}
			  	if (numbytes == 0)
			  	{
			  		fprintf(stderr, "\nConnection closed by client");
			  		close(new_fd);
					exit(1);
			  	}

				printf("\nserver: received %d bytes '%s'\n",numbytes,buf); fflush(stdout);
				if(check_error(msg, numbytes)==1)
				{
					fprintf(stderr,"\nError Frame recieved. Sending nak."); fflush(stdout);
					char control = 0x80 | REJ;
					send_sframe(new_fd,control,error_rate);
				}
				else
				{
					// No error in the received frame
					if ( (char)(msg[CONTROL] & 0x80) == (char)0x00 )
					{
						printf(" \n Recieved frame is Information"); fflush(stdout);
						
						//Information frame received.
						char seq_no = (msg[CONTROL] & 0x70)>>4;
						char ack_no = msg[CONTROL] & 0x07;

						if ( (char)seq_no != (char)frame_expected )
						{
							//Unexpected frame received. Sending Duplicate ack.
							fprintf(stderr,"\nSomeother frame received. Sending duplicate ack. ");
							fprintf(stderr,"\n%d frame expected where as %d frame received. control is %hhx",frame_expected, seq_no, msg[CONTROL]);
						}
						else
						{
							msg[numbytes-1] = '\0'; 
							printf("\nUnwrapped message is : %s\n", msg+HEADER_LENGTH); fflush(stdout);
							write(write_fifo, msg+HEADER_LENGTH, strlen(msg+HEADER_LENGTH));
							frame_expected = inc(frame_expected);
						}

						char control = 0x80 | RR | (0x07 & frame_expected);
						send_sframe(new_fd, control,error_rate);
					}

					else if ( (char)(msg[CONTROL] & 0xC0) == (char)0x80 )
					{
						printf(" \n Recieved frame is supervisory"); fflush(stdout);
						//supervisory frame recieved.
						if ( (char)(msg[CONTROL] & 0x30) == (char)RNR)
							network_layer_enabled = 0;
					}

					else if ( (char)(msg[CONTROL] & 0xC0) == (char)0xC0 )
					{

						printf(" \n Recieved frame is Unnumbered"); fflush(stdout);

						//Unnumbered frame received.
						if ( (char)(msg[CONTROL] & MF_MASK) == (char)DISC )
						{
							char control = 0xC0 | DISC;
							while( send_sframe(new_fd,control,error_rate) );
							break;
						}	
						else if ( (char)(msg[CONTROL] & MF_MASK) == (char)UA )
						{

						}
					}
					else
					{
						fprintf(stderr, "\nFrame of unknown type recieved\n");
					}
				}
			}
			printf("Connection has been terminated by client\n"); fflush(stdout);
			close(new_fd);
			exit(0);
		}

		//Parent process
		close(new_fd);  // parent doesn't need this
	}
	close(sockfd);
	return 0;
}
