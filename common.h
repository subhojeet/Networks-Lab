#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>

#define PORT "5000" 	 // the port client will be connecting to 
#define MAXDATASIZE 1024 // max number of bytes we can get at once 
#define DIVISOR 0x07
#define N 8  		 	// for crc-8
#define MAXSEQNUM 7

//Positions
#define SRC_ADDRESS 0
#define DEST_ADDRESS 1
#define CONTROL 2
#define PROTOCOL_ID 3

#define HEADER_LENGTH 4

#define LOWER_LAYER 0x01
#define UPPER_LAYER 0x10

#define SABM 0x34
#define SNRM 0x01
#define UA 0x06
#define DISC 0x02
#define MF_MASK 0x37


#define RR 0x00
#define RNR 0x10
#define REJ 0x20
#define SREJ 0x30

/*++++++++++++++++++++++++++++++++++++++++++++++++

SRC_ADD   DEST_ADD   CONT   PROTOCOL  MSG   CRC

++++++++++++++++++++++++++++++++++++++++++++++++*/



//CRC function returns remainder
char crc(unsigned char *msg, int msglen)
{
	if (msg == NULL || msglen == 0)
		return 0x00;
	unsigned char remainder = msg[0];
	int end_byte = 1, end_ptr = 8;
	printf("\n Calculating crc.\n");
	int i;	
	for (i = 0; i < msglen; i++)
		printf("%hhx ",msg[i]);	
	
	while(1)
	{
		//printf("\n%hhx",remainder);
		if (end_ptr == 8 && end_byte == msglen)
		{
			printf("\n CRC is %hhx",remainder); fflush(stdout);
			return remainder;
		}		
		if ( (char)(remainder & 0x80) == (char)0x80 )
		{	
				remainder = ( remainder << 1 ) + ( ( msg[end_byte] & (char)(1 << (end_ptr-1)) ) >> (end_ptr-1) );		
			//printf("   Shifting and dividing - %hhx  %d %d %hhx %d %d %d %d", remainder, end_byte, end_ptr, msg[end_byte], msg[end_byte], ( msg[end_byte] & (char)(1 << (end_ptr-1)) ), (1 << (end_ptr-1)), ( msg[end_byte] & (char)(1 << (end_ptr-1)) ) >> (end_ptr-1));
			remainder = remainder ^ DIVISOR;
		}
		else
		{	
			remainder = ( remainder << 1 ) + ( ( msg[end_byte] & (char)(1 << (end_ptr-1)) ) >> (end_ptr-1) );				
			//printf("  %hhx  %d %d %hhx %d %d %d %d", remainder, end_byte, end_ptr, msg[end_byte], msg[end_byte], ( msg[end_byte] & (char)(1 << (end_ptr-1)) ), (1 << (end_ptr-1)), ( msg[end_byte] & (char)(1 << (end_ptr-1)) ) >> (end_ptr-1));
		}
		end_ptr--;
		if (end_ptr == 0)
		{
			end_ptr = 8;
			end_byte++;
		}
	}
}


int check_error(char *msg, int msglen)
{
	//return 0;
	char remainder = crc(msg, msglen);
	if ( (char)remainder == (char)0x00 )
	{
		printf("Passed CRC Check\n"); fflush(stdout);
		return 0;
	}
	else
	{
		fprintf(stderr, "CRC Check failed\n" );
		return 1;
	}
}

void insert_error(char* msg, int length, float ber)
{
	int i = 0;
	float r;
	for (i = 0; i < length; i++)
	{
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x80;

		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x40;
		
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x20;
		
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x10;
		
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x08;
			
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x04;
		
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x02;
		
		r = (float)rand()/(float)(RAND_MAX/1);
		if (r < ber) msg[i]  = msg[i] ^ 0x01;	
	}
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int send_sframe(int fd, char control, float error_rate)
{
	printf("\n   Sending S-frame. Control is : %hhx ",control); fflush(stdout);
	char msg[5];
	msg[SRC_ADDRESS] = 0x5f;
	msg[DEST_ADDRESS] = 0x5f;
	msg[CONTROL] = control;
	msg[PROTOCOL_ID] = LOWER_LAYER;
	msg[HEADER_LENGTH] = 0x00;
	msg[HEADER_LENGTH] = crc(msg,5);

	insert_error(msg, 5, error_rate);

	if(send(fd, msg, 5, 0) == -1)
	{
		fprintf(stderr,"Sending S frame Failed");
		return 1;
	}
	return 0;
}

int send_msg(int fd, char *msg_to_send, int length, char control, float error_rate)
{
	printf("\n    Sending message. Control is : %hhx ",control); fflush(stdout);
	char *msg = (char *)malloc(length+HEADER_LENGTH+1);
	msg[SRC_ADDRESS] = 0x5f;
	msg[DEST_ADDRESS] = 0x5f;
	msg[CONTROL] = control;
	msg[PROTOCOL_ID] = UPPER_LAYER;
	strcpy(msg+HEADER_LENGTH, msg_to_send);
	msg[length + HEADER_LENGTH] = 0x00;
	msg[length + HEADER_LENGTH] = crc(msg, length + HEADER_LENGTH + 1);

	insert_error(msg, length + HEADER_LENGTH + 1 , error_rate);

	if(send(fd, msg, length + HEADER_LENGTH + 1, 0) == -1)
	{
		fprintf(stderr,"Sending Msg Failed");
		return 1;
	}
	return 0;
}

char inc(char seq)
{
	seq++;
	if (seq > MAXSEQNUM)
		return 0;
	else
		return seq;
}
