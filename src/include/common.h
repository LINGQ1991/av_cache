#ifndef MULTIMEDIA_COMMON_H
#define MULTIMEDIA_COMMON_H

//#include <base/evlog.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <curl/curl.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern char* _multimedia_env;
extern int _multimedia_debug;

static inline void query_vsprotocol_env(void)
{
	_multimedia_env = getenv("cbb_debug_multimedia");
	if(_multimedia_env)
		_multimedia_debug = atoi(_multimedia_env);
	else
		_multimedia_env = (char*)"0";
}

#define MULTIMEDIA_DEBUG(fmt, arg...) \
{\
	if(!_multimedia_env) \
	{ \
		query_vsprotocol_env(); \
	} \
	if(_multimedia_debug) \
		LOG_DEBUG(fmt, ##arg); \
}
#define MULTIMEDIA_INFO(fmt, arg...) LOG_INFO(fmt, ##arg)
#define MULTIMEDIA_WARN(fmt, arg...) LOG_WARN(fmt, ##arg)
#define MULTIMEDIA_ERROR(fmt, arg...) LOG_ERROR(fmt, ##arg)
#define MULTIMEDIA_FATAL(fmt, arg...) LOG_FATAL(fmt, ##arg)

#define WRTAIL				0x01
#define WRRING			0x02
#define TAILFULL			0x04
#define WRFULL			0x08
#define REFRESHRING 	0x10

#define RING_BUFFER_SIZE 1024*1024

typedef struct {
	pthread_mutex_t lock;
	uint8_t* buffer_base;
	uint8_t* buffer_end;
	int buffer_size;
	uint8_t* data_ptr;
	int data_size;
	int64_t d_begin;
	int64_t d_end;
}RingBufferContext;

typedef struct {
	uint8_t *data_ptr;
	uint8_t *data_ptr_r;
	uint8_t *data_base;
	uint8_t *data_end; 
	int data_size;
	int64_t d_begin;
	int64_t d_end;
}TailBufferContext;

typedef struct {
	RingBufferContext ringbuffer;
	TailBufferContext tailbuffer;
	int64_t cur_read_pos; //当前位置
	//int w_cur_pos;
	int64_t data_size; //缓存数据大小
	int64_t size; //数据大小
	int flags;
	pthread_mutex_t iolock;
}BufferContext;

int ring_buffer_write(RingBufferContext* ctx, uint8_t* buffer, int size);
int ring_buffer_read(RingBufferContext* ctx, uint8_t* buffer, int size);
int ring_buffer_discard_head(RingBufferContext* ctx, int size);
int ring_buffer_readoffset(RingBufferContext* ctx, uint8_t* buffer, int size, int offset);
int ring_buffer_init(RingBufferContext* ctx, int size);
int ring_buffer_free(RingBufferContext* ctx);
int ring_buffer_datasize(RingBufferContext* ctx);
int ring_buffer_buffersize(RingBufferContext* ctx);
int ring_buffer_freesize(RingBufferContext* ctx);
int ring_buffer_clear(RingBufferContext* ctx);

int tail_buffer_init(TailBufferContext *ttx, int size);
int tail_buffer_clear(TailBufferContext *ttx);
int tail_buffer_write(TailBufferContext *ttx, uint8_t *buffer, int size);
int tail_buffer_read(TailBufferContext *ttx, uint8_t* buffer, int size);
int tail_buffer_datasize(TailBufferContext *ttx);
int tail_buffer_free(TailBufferContext *ttx);
int tail_buffer_readoffset(TailBufferContext* ctx, uint8_t* buffer, int size, int offset);

int buffer_init(BufferContext *tx, int rsize, int tsize);
int buffer_read(BufferContext *tx, uint8_t *buffer, int size);
int buffer_write(BufferContext *tx, uint8_t *buffer, int size, int flag);
int buffer_seek(BufferContext *tx, int64_t offset, int whence);
int buffer_free(BufferContext *tx);
int64_t buffer_cur_read_pos(BufferContext *tx);
void buffer_cur_read_pos_set(BufferContext *tx, int64_t size);
void buffer_flags_set(BufferContext *tx, int flag);
int buffer_flags(BufferContext *tx);

enum fcurl_type_e {
  CFTYPE_NONE=0,
  CFTYPE_FILE=1,
  CFTYPE_CURL=2
};

struct fcurl_data
{
  CURLM *multi_handle;
  enum fcurl_type_e type;     /* type of handle */
  union {
    CURL *curl;
    FILE *file;
  } handle;                   /* handle */

  char *buffer;               /* buffer to store cached data*/
  size_t buffer_len;          /* currently allocated buffers length */
  size_t buffer_pos;          /* end of data in buffer*/
  int still_running;          /* Is background url fetch still in progress */
  int64_t file_size;
  int64_t cur_pos;
};

typedef struct fcurl_data URL_FILE;

/* exported functions */
URL_FILE *curl_fopen(const char *url,const char *operation);
int curl_fclose(URL_FILE *file);
int curl_feof(URL_FILE *file);
size_t curl_fread(void *ptr, size_t size, size_t nmemb, URL_FILE *file);
char * curl_fgets(char *ptr, size_t size, URL_FILE *file);
int curl_fseek(URL_FILE *file, long offset, int whence);
int64_t curl_fsize(URL_FILE *file);
void curl_rewind(URL_FILE *file);

#ifdef __cplusplus
}
#endif

#endif
