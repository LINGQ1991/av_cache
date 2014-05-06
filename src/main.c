#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "queue.h"

RingBufferContext ring_buffer;
pthread_mutex_t lock;
//char *file = "/opt/nfsroot/cache/db/test";
//char *file = "/opt/nfsroot/cache/db/1.doc";
char *file = "/opt/nfsroot/cache/db/10003196.mkv";
char buffer1[32768], buffer2[32768];
int fd = 0, fd2 = 0;
FILE *fp = NULL;
FILE *fp_r = NULL;
pthread_t input_id, output_id;
int flags = 0;

void* thrd_input()
{
	int ret = 0;
	int size = 0;
	
	fd = open(file, O_RDONLY);
	//fp_r = fopen(file, "r")
	if(fd == -1){
		printf("Cannot open file.\n");
		flags = 1;
		pthread_exit(NULL);
		//return (void*)(-1);
	}
	while(1){
		memset(buffer1, 0, sizeof(buffer1));
		
		size = read(fd, buffer1, sizeof(buffer1));
		printf("read ret = %d\n", ret);
		if(size < 0) {
			printf("Cannot read from file.\n");
			flags = 1;
			pthread_exit(NULL);
			//return (void *)(-1);
		} else if (size == 0) {
			printf("read ok.\n");
			break;
		} else {
		rewrite:
			pthread_mutex_lock(&lock);
			/*
			if(ring_buffer_datasize(&ring_buffer) == ring_buffer_buffersize(&ring_buffer))
			{
				pthread_mutex_unlock(&lock);
				usleep(1);
				goto redo;
			}
			ring_buffer_write(&ring_buffer,  (uint8_t *)buffer1, ret);
			*/
			
			ret = write_data(&ring_buffer,  (uint8_t *)buffer1, size);
			//ret = write_data(&ffmpeg.inputringbuffer,  (uint8_t *)buffer1, size);
			if(-1 == ret || 0 == ret) 
			{
				pthread_mutex_unlock(&lock);
				usleep(1);
				goto rewrite;
			}
			
			pthread_mutex_unlock(&lock);
		}
	}
	
	flags = 1;
	pthread_exit(NULL);
	//return (void *)(0);
}

void* thrd_output()
{
    int ret = 0;
	int size = 0;
	//int nread = 0;
	fd2 = open("./1.mkv", O_WRONLY | O_CREAT | O_TRUNC);
	if(fd2 < 0){
		printf("Cannot open file.\n");
		pthread_exit(NULL);
		//return (void*)(-1);
	}
	
	while(1){
		memset(buffer2, 0, sizeof(buffer2));
	reread:
		pthread_mutex_lock(&lock);
		
		ret = read_data(&ring_buffer,  (uint8_t *)buffer2, sizeof(buffer2));
		size = ret;
		if(0 == ret )
		{
			pthread_mutex_unlock(&lock);
			if(1 == flags) 
			{
				break;
			}
			else 
			{
				usleep(1);
				goto reread;
			}
		}
		else 
		{
			pthread_mutex_unlock(&lock);
			write(fd2, buffer2, size);
		}
		
	}
	/*
	pthread_mutex_lock(&lock);
	ret = ffmpeg.ReadData(sizeof(buffer2), buffer, &nread);
	fwrite(buffer2, 1, nread, fp_write);
	pthread_mutex_unlock(&lock);
	*/
	pthread_exit(NULL);
	//return (void *)(0);
}

int main(void)
{
	//int ret = 0;
	ring_buffer_init(&ring_buffer, 1024*1024);
	pthread_mutex_init(&lock, NULL);
	//CFfmpeg ffmpeg;
	
	if(pthread_create(&input_id, NULL, thrd_input, NULL) != 0) {
		printf("Create input_thread error!\n");
		return -1;
	}
	if(pthread_create(&output_id, NULL, thrd_output, NULL) != 0) {
		printf("Create output_thread error!\n");
		return -1;
	}
	
	// �ȴ��߳�input_id�˳�
	if (pthread_join(input_id,NULL) != 0) {
		printf("Join input_thread error!\n");
		return -1;
	}else
		printf("input_thread Joined!\n");
		
	// �ȴ��߳�output_id�˳�
	if (pthread_join(output_id,NULL) != 0) {
		printf("Join output_thread error!\n");
		return -1;
	}else
		printf("output_thread Joined!\n");
	
	pthread_mutex_destroy(&lock);
	ring_buffer_free(&ring_buffer);
	printf("ok.\n");
	
	return 0;
}