#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "common.h"

#define TAIL_SIZE_MAX 15*1024*1024	//15M
#define TAIL_SIZE_MIN 1*1024*1024 	//1M
#define TAIL_SIZE 20	//5%	

BufferContext buffer_context;
const char *url;

static int read_data(void *opaque, uint8_t *buf, int buf_size)
{
	int ret = 0;
	RingBufferContext *ctx = (RingBufferContext*)opaque;
	int datsize = ring_buffer_datasize(ctx);
	int size = datsize;
	if(size <= 0)
		return 0;
	else if(size > buf_size)
		size = buf_size;
	//printf("avio read %p to %p, reqsize=%d, datsize=%d, rest=%d\n", opaque, buf, buf_size, datsize, datsize-buf_size);
	ret = ring_buffer_read(ctx, buf, size);
	if(ret == 0)
		ret = size;
	else
		ret = 0;
	return ret;
}

void c_init()
{
	buffer_init(&buffer_context, 1024*1024, 0);
}

void c_destroy()
{
	buffer_free(&buffer_context);
}

/* input tail data to tail_buffer */
int input_tail(BufferContext *tx)
{
	//int ret = 0;
	int size = 0, tail_size = 0, rsize = 0;
	int nread;
	uint8_t buffer[32768];
	URL_FILE *handle;
	TailBufferContext *ttx = &tx->tailbuffer;
	
	handle = curl_fopen(url, "r");
	if(!handle) {
		printf("couldn't curl_fopen() url %s\n", url);
		//curl_fclose(handle);
		return -1;
	}
	
	/* input tail data to tail_buffer */
	size = curl_fsize(handle);
	tx->size = size; //缓存大小
	printf("t---size=%d\n", size);
	tail_size = size / TAIL_SIZE;
	
	if(tail_size >TAIL_SIZE_MAX) {
		tail_size = TAIL_SIZE_MAX;
		//printf("test1\n");
	} else if (tail_size < TAIL_SIZE_MIN) {
		tail_size = size;
	} else {
		//nothing;
	}
	curl_fseek(handle, size - tail_size, SEEK_SET);
	ttx->data_size = tail_size;
	
	printf("t---curl_fsize = %lld\n", curl_fsize(handle));
	
	tail_buffer_init(ttx, tail_size);
	
	rsize = 0;
	int cnt = 0;
	while(1) {
	tredo:
		memset(buffer, 0, sizeof(buffer));
		if(sizeof(buffer) <= (tail_size - rsize)) {
			nread = curl_fread(buffer, 1,sizeof(buffer), handle);
		} else {
			nread = curl_fread(buffer, 1,(tail_size - rsize), handle);
		}
		if(nread == 0) {
			//printf("tail read full\n");
			if(cnt++ < 3) {
				printf("tail read again\n");
				goto tredo;
			}
			printf("tail read full\n");
			break;
		} else if (nread < 0) {
			printf("curl is not running\n");
			break;
			//continue;
			//pthread_exit(NULL);
			//return -1;
		} else {
			int ret = buffer_write(tx, buffer, nread, WRTAIL);
			if(ret == -1)
				return -1;
			rsize += nread;
		}
		
		if(rsize >= tail_size) 
			break;
	}
	printf("t---rsize = %d\n", rsize);
	curl_fclose(handle);
	tx->flags |= TAILFULL;
	
	return 0;
}

void* thrd_input(void *arg)
{
	int ret = 0;
	int nread;
	uint8_t buffer[32768];
	URL_FILE *handle;
	BufferContext *tx = (BufferContext *)arg;
	TailBufferContext *ttx = &(tx->tailbuffer);
	RingBufferContext *rtx = &(tx->ringbuffer);
	//pthread_mutex_t *iolock = &(rtx->lock);
	int64_t dsize = tx->size - ttx->data_size;
	int64_t rsize,  size;

	handle = curl_fopen(url, "r");
	if(!handle) {
		printf("i---couldn't curl_fopen() url %s\n", url);
		//return (void *)(2);
		pthread_exit(NULL);
	}
	printf("i---size = %lld\t curl_size = %lld\n", dsize, curl_fsize(handle));
	
	/* input data to ringbuffer */
	rsize = 0;
	while(1) {
		memset(buffer, 0, sizeof(buffer));
		if (rsize >= dsize) {
			printf("i---read full\n");
			break;
		}
		//read url data
		size = sizeof(buffer);
		if(size >= (dsize - rsize))
			size = dsize - rsize;
		nread = curl_fread(buffer, 1, size, handle);
		if(nread == 0) {
			if (rsize >= dsize) {
				printf("i----read full\n");
				break;
			} else {
				printf("i---error: rsize = %lld\t\tdsize = %lld\t\tnread = %d\n", rsize, dsize, nread);
				break;
			}
		} else if (nread < 0){
			printf("curl is not running\n");
			break;
		} else {
		
		}
		
		//input data to ringbuffer
	wrdo:
		//pthread_mutex_lock(iolock);
		ret = buffer_write(tx, buffer, nread, WRRING);
		if(ret != 0) {
			//pthread_mutex_unlock(iolock);
			usleep(2);
			//printf("i---goto write again\n");
			goto wrdo;
		}
		//pthread_mutex_unlock(iolock);
		rsize += nread;
	}
	
	tx->flags |= WRFULL;
	curl_fclose(handle);
	pthread_exit(NULL);
	//return (void *)(0);
}

void *thrd_output(void *arg)
{
	int ret = 0;
	int fd;
	uint8_t buffer[32768];
	BufferContext *tx = (BufferContext *)arg;
	//int64_t cur_pos = tx->cur_read_size;
	int64_t dsize = tx->size;
	int64_t rsize, size;
	
	fd = open("./1.mkv", O_WRONLY | O_CREAT | O_TRUNC);
	if(fd < 0){
		printf("Cannot open file.\n");
		pthread_exit(NULL);
		//return (void*)(-1);
	}
	
	rsize = 0;
	while(1) {
		memset(buffer, 0, sizeof(buffer));
		if(rsize >= dsize)
			break;
	redo:
		size = sizeof(buffer);
		if(size >= dsize - rsize)
			size = dsize - rsize;
		//if(size >= tx->data_size)
		//	size = tx->data_size;
			
		ret = buffer_read(tx, buffer, size);
		if(ret == -1) {
			printf("o---size > tail size\n");
			break;
		} else if (ret == -2) {
			usleep(2);
			//printf("goto read again\n");
			goto redo;
		} else if (ret == -3){
			printf("o---size < 0\n");
			break; 
		} else {
			//nothing
		}
		write(fd, buffer, size);
		rsize += size;
	}
	
	close(fd);
	pthread_exit(NULL);
	//return (void *)(0);
}

int main(int argc, char **argv)
{
	int ret;
	pthread_t input_id, output_id;
	
	if(argc < 2)
		url="/opt/nfsroot/cache/db/10003196.mkv";/* default to testurl */
	else
		url=argv[1];/* use passed url */
	
	c_init();

	ret = input_tail(&buffer_context);
	if(ret != 0) {
		printf("input tail failed\n");
	} else {
		printf("input tail sucessed\n");
	}	

	if(pthread_create(&input_id, NULL, thrd_input, (void *)(&buffer_context)) != 0) {
		printf("Create input_thread error!\n");
		return -1;
	}
	if(pthread_create(&output_id, NULL, thrd_output, (void *)(&buffer_context)) != 0) {
		printf("Create output_thread error!\n");
		return -1;
	}
	
	// 等待线程input_id退出
	if (pthread_join(input_id,NULL) != 0) {
		printf("Join input_thread error!\n");
		return -1;
	}else
		printf("input_thread Joined!\n");
		
	// 等待线程output_id退出
	if (pthread_join(output_id,NULL) != 0) {
		printf("Join output_thread error!\n");
		return -1;
	}else
		printf("output_thread Joined!\n");
	
	//printf("tail_buffer: %s\n", tail_buffer);
	c_destroy();

	return 0;
}
