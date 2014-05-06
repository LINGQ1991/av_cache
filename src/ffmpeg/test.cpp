#include <stdio.h>
#include <iostream>
#include "ffmpeg.h"
#include "common.h"

using namespace std;
using namespace evideo::multimedia;

#define TAIL_SIZE_MAX 15*1024*1024	//15M
#define TAIL_SIZE_MIN 1*1024*1024 	//1M
#define TAIL_SIZE 20	//5%	

const char *url;

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
	ttx->d_begin = size - tail_size;
	ttx->d_end = ttx->d_begin + tail_size;
	
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
	int nread, i;
	uint8_t buffer[32768];
	URL_FILE *handle;
	BufferContext *tx = (BufferContext *)arg;
	TailBufferContext *ttx = &(tx->tailbuffer);
	RingBufferContext *rtx = &(tx->ringbuffer);
	//pthread_mutex_t *iolock = &(rtx->lock);
	int64_t dsize = tx->size - ttx->data_size;
	int64_t rsize,  size;
	int64_t cur_pos;

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
		int flags = buffer_flags(tx);
		if( flags & REFRESHRING) { //refresh the ringbuffer。
			ring_buffer_free(rtx);
			ring_buffer_init(rtx, RING_BUFFER_SIZE);
			cur_pos = buffer_cur_read_pos(tx);
			curl_fseek(handle, cur_pos, SEEK_SET);
			rtx->d_begin = cur_pos;
			rtx->d_end = cur_pos + RING_BUFFER_SIZE;
			buffer_flags_set(tx, flags & ~RING_BUFFER_SIZE);
		}
		//read url data
		size = sizeof(buffer);
		//if(size >= (dsize - rsize))
		//	size = dsize - rsize;
		nread = curl_fread(buffer, 1, size, handle);
		if(nread == 0) {
			if (rsize >= dsize) {
				printf("i----read full\n");
				//break;
			} else {
				printf("i---error: rsize = %lld\t\tdsize = %lld\t\tnread = %d\n", rsize, dsize, nread);
				break;
			}
		} else if (nread < 0){
			printf("curl is not running\n");
			break;
		} else {
			//nothing
		}
		
		//input data to ringbuffer
		i = 0;
	wrdo:
		ret = buffer_write(tx, buffer, nread, WRRING);
		if(ret != 0) {
			//i++;
			usleep(2);
			//if(i>10)
				//break;
			//printf("i---goto write again\n");
			goto wrdo;
		}
		rsize += nread;
	}
	
	tx->flags |= WRFULL; 
	curl_fclose(handle);
	pthread_exit(NULL);
	//return (void *)(0);
}

void *thrd_output(void *arg)
{
	CFfmpeg *ffmpeg = (CFfmpeg *)arg;
	string furl = "ffmpeg://";
	string str = url;
	furl += url;
	//printf("furl : %s\n", furl);
	//usleep(100);
	unsigned char buffer[32768];
	int nread = 0;
	int ret = 0;
	FILE* fp_write = NULL;
	fp_write = fopen("output.ts", "w+");
	ffmpeg->Open(furl.c_str());
	do
	{
		ret = ffmpeg->ReadData(sizeof(buffer), buffer, &nread);
		fwrite(buffer, 1, nread, fp_write);
		//printf("hello\n");
		//usleep(1);
	}while(ret > 0);
	printf("\nret = %d\n\n", ret);
	fclose(fp_write);
	
	pthread_exit(NULL);
	//return (void *)(0);
}

int main(int argc, char** argv)
{
	//string url = "ffmpeg://";
	int ret = 0;
	pthread_t input_id, output_id;
	CFfmpeg ffmpeg;
	BufferContext *tx = &(ffmpeg.cbuffer);
	
	if(argc < 2) {
		url = "/opt/nfsroot/cache/db/1080p.mpg";
	} else {
		url = argv[1];
	}
	
	ret = input_tail(tx);
	if(ret == -1) {
		printf("input tail failed\n");
	} else {
		printf("input tail sucessed\n");
	}	
	
	if(pthread_create(&input_id, NULL, thrd_input, (void *)(tx)) != 0) {
		printf("Create input_thread error!\n");
		return -1;
	}
	if(pthread_create(&output_id, NULL, thrd_output, (void *)(&ffmpeg)) != 0) {
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
	
	ffmpeg.Close();
	
	return 0;
}
