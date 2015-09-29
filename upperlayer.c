#include "common.h"

int main(int argc, char *argv[])
{
	int my_pid = getpid();
	if (my_pid == -1)
		fprintf(stderr,"Get pid command failed");
	
	char fifoname[12]; 							//This is used for reading from lower layer.
	sprintf(fifoname, "%d_0.txt", my_pid); 		
	printf("Fifo:%s\n",fifoname);
	
	if (mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
		fprintf(stderr,"mkfifo %s", fifoname);

	int read_fifo = open(fifoname, O_RDONLY);
	if (read_fifo == -1)
		fprintf(stderr,"open %s", fifoname);


	sprintf(fifoname, "%d_1.txt", my_pid); 		
	printf("Fifo:%s\n",fifoname);
	
	if (mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
		fprintf(stderr,"mkfifo %s", fifoname);

	int write_fifo = open(fifoname, O_WRONLY);
	if (write_fifo == -1)
		fprintf(stderr,"open %s", fifoname);
	

	char msg[1024]; //= "Hello world!";
	
	while(1)
	{
		int bytes_read;
		if ( (bytes_read = read(read_fifo,msg,sizeof(msg)))  != -1 )
		{
			if (bytes_read != 0)
			{
				msg[bytes_read] = '\0';
				printf("\nProcess %d: %d bytes read. Message read is : %s",getpid(),bytes_read,msg);
			}
		}
		else
		{
			fprintf(stderr,"\nProcess %d: Couldn't read message properly.", getpid());
		}

		fflush(stdout);
	}
	printf("\nChild Executed.");

	close(read_fifo);
	close(write_fifo);
	return 0;	
}