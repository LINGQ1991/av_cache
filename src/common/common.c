#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "common.h"

/*  ring_buffer API */
int ring_buffer_write(RingBufferContext* ctx, uint8_t* buffer, int size)
{
	pthread_mutex_lock(&ctx->lock);
	uint8_t* wptr = ctx->data_ptr + ctx->data_size;
	uint8_t* wptr2 = NULL;
	int size2 = 0;
	if(size <= (ctx->buffer_size - ctx->data_size))//space available
	{
		if(wptr >= ctx->buffer_end)//rrrrrwwwwwwrrrrrr
		{
			int skip = wptr - ctx->buffer_end;
			wptr = ctx->buffer_base + skip;
		}
		else if((wptr + size) > ctx->buffer_end)//wwwwwrrrrrrww
		{
			wptr2 = ctx->buffer_base;
			size2 = size - (ctx->buffer_end - wptr);
			size -= size2;
		}
		else//rrrrwwwww
		{
			//nothing
		}
		memcpy(wptr, buffer, size);
		if(wptr2)
		{
			memcpy(wptr2, buffer+size, size2);
		}
		ctx->data_size += (size+size2);
		pthread_mutex_unlock(&ctx->lock);
		return 0;
	}
	else
	{
		pthread_mutex_unlock(&ctx->lock);
		return -1;
	}
}

int ring_buffer_read(RingBufferContext* ctx, uint8_t* buffer, int size)
{
	pthread_mutex_lock(&ctx->lock);
	uint8_t* rptr = ctx->data_ptr + size;
	if(size <= ctx->data_size)
	{
		if(rptr > ctx->buffer_end)
		{
			int size0 = ctx->buffer_end - ctx->data_ptr;
			int size1 = rptr - ctx->buffer_end;
			memcpy(buffer, ctx->data_ptr, size0);
			memcpy(buffer+size0, ctx->buffer_base, size1);
			ctx->data_ptr = ctx->buffer_base + size1;
			ctx->data_size -= size;
		}
		else
		{
			memcpy(buffer, ctx->data_ptr, size);
			ctx->data_ptr = ctx->data_ptr + size;
			ctx->data_size -= size;
			if(ctx->data_ptr >= ctx->buffer_end)
			{
				ctx->data_ptr = ctx->buffer_base;
			}
		}
		pthread_mutex_unlock(&ctx->lock);
		return 0;
	}
	else
	{
		pthread_mutex_unlock(&ctx->lock);
		return -1;
	}
}

int ring_buffer_discard_head(RingBufferContext* ctx, int size)
{
	pthread_mutex_lock(&ctx->lock);
	uint8_t* rptr = ctx->data_ptr + size;
	if(size <= ctx->data_size)
	{
		if(rptr > ctx->buffer_end)
		{
			//int size0 = ctx->buffer_end - ctx->data_ptr;
			int size1 = rptr - ctx->buffer_end;
			ctx->data_ptr = ctx->buffer_base + size1;
			ctx->data_size -= size;
		}
		else
		{
			ctx->data_ptr = ctx->data_ptr + size;
			ctx->data_size -= size;
			if(ctx->data_ptr >= ctx->buffer_end)
			{
				ctx->data_ptr = ctx->buffer_base;
			}
		}
		pthread_mutex_unlock(&ctx->lock);
		return 0;
	}
	else
	{
		pthread_mutex_unlock(&ctx->lock);
		return -1;
	}
}

int ring_buffer_readoffset(RingBufferContext* ctx, uint8_t* buffer, int size, int offset)
{
	pthread_mutex_lock(&ctx->lock);
	int tmpsize = 0;
	uint8_t* destptr = ctx->data_ptr + offset; //dest read ptr
	if (destptr > ctx->buffer_end)
	{
		tmpsize = destptr - ctx->buffer_end;
		destptr = ctx->buffer_base + tmpsize;
	}
	int datasize_from_offset = ctx->data_size - offset;
	if (size <= datasize_from_offset)
	{
		if(destptr+size <= ctx->buffer_end)
			memcpy(buffer, destptr, size);
		else
		{
			int size0 = ctx->buffer_end - destptr;
			memcpy(buffer, destptr, size0);
			memcpy(buffer+size0, ctx->buffer_base, size-size0);
		}
		pthread_mutex_unlock(&ctx->lock);
		return size;
	}
	else if(datasize_from_offset > 0)
	{
		if(destptr+datasize_from_offset <= ctx->buffer_end)
			memcpy(buffer, destptr, datasize_from_offset);
		else
		{
			int size0 = ctx->buffer_end - destptr;
			memcpy(buffer, destptr, size0);
			memcpy(buffer+size0, ctx->buffer_base, datasize_from_offset-size0);
		}
		pthread_mutex_unlock(&ctx->lock);
		return datasize_from_offset;
	}
	else
	{
		pthread_mutex_unlock(&ctx->lock);
		return 0;
	}
}

int ring_buffer_init(RingBufferContext* ctx, int size)
{
	if(size <= 0) {
		return 0;
	}
	pthread_mutex_init(&ctx->lock, NULL);
	ctx->buffer_base = (uint8_t*)malloc(size);
	if(ctx->buffer_base)
	{
		memset(ctx->buffer_base, 0, size);
		ctx->buffer_end = ctx->buffer_base + size;
		ctx->buffer_size = size;
		ctx->data_ptr = ctx->buffer_base;
		ctx->data_size = 0;
		ctx->d_begin = 0;
		ctx->d_end = size;
	}
	return ctx->buffer_base ? 0 : -1;
}

int ring_buffer_free(RingBufferContext* ctx)
{
	pthread_mutex_destroy(&ctx->lock);
	free(ctx->buffer_base);
	ctx->buffer_base = NULL;
	return 0;
}

int ring_buffer_datasize(RingBufferContext* ctx)
{
	pthread_mutex_lock(&ctx->lock);
	int ret = ctx->data_size;
	pthread_mutex_unlock(&ctx->lock);
	return ret;
}

int ring_buffer_buffersize(RingBufferContext* ctx)
{
	pthread_mutex_lock(&ctx->lock);
	int ret = ctx->buffer_size;
	pthread_mutex_unlock(&ctx->lock);
	return ret;
}

int ring_buffer_freesize(RingBufferContext* ctx)
{
	pthread_mutex_lock(&ctx->lock);
	int ret = ctx->buffer_size - ctx->data_size;
	pthread_mutex_unlock(&ctx->lock);
	return ret;
}

int ring_buffer_clear(RingBufferContext* ctx)
{
	pthread_mutex_lock(&ctx->lock);
	ctx->data_ptr = ctx->buffer_base;
	ctx->data_size = 0;
	pthread_mutex_unlock(&ctx->lock);
	return 0;
}

int64_t ring_buffer_d_begin(RingBufferContext* ctx) 
{
	pthread_mutex_lock(&ctx->lock);
	int64_t ret = ctx->d_begin;
	pthread_mutex_unlock(&ctx->lock);
	return ret;
}

void ring_buffer_d_begin_set(RingBufferContext* ctx, int64_t)
{
	pthread_mutex_lock(&ctx->lock);
	int64_t ret = ctx->d_begin;
	pthread_mutex_unlock(&ctx->lock);
} 

/* tail_buffer API */
int tail_buffer_init(TailBufferContext *ttx, int size)
{
	if(size <= 0) {
		//ttx->data_size = 0;
		return 0;
	}
	ttx->data_base = (uint8_t *)malloc(size);
	if(ttx->data_base)
	{
		memset(ttx->data_base, 0, size);
		ttx->data_end = ttx->data_base + size;
		ttx->data_ptr = ttx->data_base;
		ttx->data_ptr_r = ttx->data_base;
		ttx->data_size = 0;
	}
	
	return ttx->data_base ? 0 : -1;
}

int tail_buffer_clear(TailBufferContext *ttx)
{
	return 0;
}

int tail_buffer_free(TailBufferContext *ttx)
{
	memset(ttx->data_base, 0, ttx->data_size);
	free(ttx->data_base);
	ttx->data_base = NULL;
	ttx->data_ptr = NULL;
	ttx->data_ptr_r = NULL;
	ttx->data_end = NULL;
	ttx->data_size = 0;
	
	return 0;
}

int tail_buffer_write(TailBufferContext *ttx, uint8_t *buffer, int size)
{
	int nsize = size;
	int freesize = ttx->data_end - ttx->data_ptr;
	if(nsize <= freesize) {
		memcpy(ttx->data_ptr, buffer, nsize);
		ttx->data_ptr += nsize;
		ttx->data_size += nsize;
		return 0;
	} else {
		printf("t -- write size = %d\t freesize = %d\n", nsize, freesize);
		return -1;
	}
}

//Ë³Ðò¶ÁÈ¡
int tail_buffer_read(TailBufferContext *ttx, uint8_t* buffer, int size)
{
	int nsize = size;
	int datasize = ttx->data_size;
	int tsize = ttx->data_end - ttx->data_ptr_r;
	if(nsize <= tsize) {
			memcpy(buffer, ttx->data_ptr_r, nsize);
			ttx->data_ptr_r += nsize;
			//ttx->data_size -= nsize;
			printf("tb read nsize = %d\t tsize = %d\n", nsize, tsize);
			return 0;
	} else {
		printf("tb -- read size = %d \t\t freesize = %d\n", nsize, tsize);
		return -1;
	}
}

int tail_buffer_readoffset(TailBufferContext* ttx, uint8_t* buffer, int size, int offset)
{
	int nsize = size;
	uint8_t *destptr = ttx->data_base + offset; //offset from tail_base
	//uint8_t *destptr = ttx->data_ptr_r + offset;
	int datasize = ttx->data_end - destptr;
	
	if(nsize <= datasize) {
		memcpy(buffer, destptr, nsize);
		//ttx->data_ptr_r += nsize;
		return 0;
	} else {
		printf("t -- read size = %d \t\t freesize = %d\n", nsize, datasize);
		return -1;
	}
}

int tail_buffer_datasize(TailBufferContext *ttx)
{
	return ttx->data_size;
}

/* buffer API */
int buffer_init(BufferContext *tx, int rsize, int tsize)
{
	int ret;
	RingBufferContext *rtx = &tx->ringbuffer;
	TailBufferContext *ttx = &tx->tailbuffer;
	
	ret = ring_buffer_init(rtx, rsize);
	if(ret != 0) {
		printf("Ringbuffer init error.\n");
		return -1;
	}
	ret = tail_buffer_init(ttx, tsize);
	if(ret != 0) {
		printf("Tailbuffer init error.\n");
		return -1;
	}
	
	pthread_mutex_init(&tx->iolock, NULL);
	tx->cur_read_pos = 0;
	tx->flags = 0;
	tx->data_size = 0;
	tx->size = 0;
	
	return 0;
}

int buffer_free(BufferContext *tx)
{
	//int ret;
	RingBufferContext *rtx = &tx->ringbuffer;
	TailBufferContext *ttx = &tx->tailbuffer;
	
	ring_buffer_free(rtx);
	tail_buffer_free(ttx);
	pthread_mutex_destroy(&tx->iolock);
	return 0;
}

//first write data to tail buffer and write data to ring buffer
int buffer_write(BufferContext *tx, uint8_t *buffer, int size, int flag)
{
	pthread_mutex_lock(&tx->iolock);
	int ret;
	RingBufferContext *rtx = &tx->ringbuffer;
	TailBufferContext *ttx = &tx->tailbuffer;
	int data_size = tx->data_size; 
	
	if(flag & WRTAIL) {
		ret = tail_buffer_write(ttx, buffer, size);
		if(ret != 0) {
			pthread_mutex_unlock(&tx->iolock);
			return -1;//freesize is not enough
		} else {
			data_size += size;
		}
	} 
	if (flag & WRRING) {
		ret = ring_buffer_write(rtx, buffer, size);
		if(ret != 0) {
			pthread_mutex_unlock(&tx->iolock);
			return -1; //freesize is not enough
		} else {
			data_size += size; 
		}
	} else {
		//nothing
	}
	tx->data_size = data_size;
	pthread_mutex_unlock(&tx->iolock);
	return 0;
}

int buffer_read(BufferContext *tx, uint8_t *buffer, int size)
{
	pthread_mutex_lock(&tx->iolock);
	int ret;
	int nsize = size;
	RingBufferContext *rtx = &tx->ringbuffer;
	TailBufferContext *ttx = &tx->tailbuffer;
	int cur_size = tx->cur_read_pos;
	int tail_size = tail_buffer_datasize(ttx);
	int data_size = tx->data_size;
	int tsize = tx->size;
	
	if(size < 0) {
		pthread_mutex_unlock(&tx->iolock);
		return -3;
	}
	
	if(cur_size >= ttx->d_begin && cur_size < ttx->d_end) {
		ttx->data_ptr_r = ttx->data_base + (cur_size - ttx->d_begin);
		ret = tail_buffer_read(ttx, buffer, nsize);
		if(ret != 0){
			pthread_mutex_unlock(&tx->iolock);
			return -1; //size > tail buffer data size
		}
	} else if(cur_size >= rtx->d_begin) {
		if(cur_size + nsize < rtx->d_end) {
			ret = ring_buffer_read(rtx, buffer, nsize);
			if(ret != 0) {
				pthread_mutex_unlock(&tx->iolock);
				return -2;
			} else {
				rtx->d_begin += nsize;
				rtx->d_end += nsize;
			}
		} else if (cur_size + nsize >= rtx->d_end && rtx->d_end == ttx->d_end) {
			int size0 = cur_size + nsize - ttx->d_begin;
			int size1 = nsize - size0;
			ret = ring_buffer_read(rtx, buffer, size1);
			if(ret != 0) {
				pthread_mutex_unlock(&tx->iolock);
				return -2; //size > tail buffer data size
			} else {
				rtx->d_begin += size1;
				rtx->d_end += size1;
			}
			ttx->data_ptr_r = ttx->data_base;
			ret = tail_buffer_read(ttx, buffer+size1, size0);
			if(ret != 0) {
				pthread_mutex_unlock(&tx->iolock);
				return -1;
			}
		} else {
			tx->flags |= REFRESHRING;
			return -3;
		}
	} else {
		tx->flags |= REFRESHRING;
		return -3;
	}
	
	/*
	if(cur_size >= (tsize - tail_size) && cur_size < tsize) {
		ttx->data_ptr_r = ttx->data_base + (cur_size+tail_size-tsize);
		ret = tail_buffer_read(ttx, buffer, nsize);
		if(ret != 0){
			pthread_mutex_unlock(&tx->iolock);
			return -1; //size > tail buffer data size
		}
	} else if((cur_size + nsize) >= (tsize - tail_size)){  
		int size0 = cur_size + nsize - (tsize - tail_size);
		int size1 = nsize - size0;
		ret = ring_buffer_read(rtx, buffer, size1);
		if(ret != 0) {
			pthread_mutex_unlock(&tx->iolock);
			return -2; //size > tail buffer data size
		} else {
			rtx->d_begin += size1;
			rtx->d_end += size1;
		}
		ttx->data_ptr_r = ttx->data_base;
		ret = tail_buffer_read(ttx, buffer+size1, size0);
		if(ret != 0) {
			pthread_mutex_unlock(&tx->iolock);
			return -1;
		}
	} else if((cur_size + nsize) < (tsize - tail_size)) {
		ret = ring_buffer_read(rtx, buffer, nsize);
		if(ret != 0) {
			pthread_mutex_unlock(&tx->iolock);
			return -2;
		} else {
			rtx->d_begin += nsize;
			rtx->d_end += nsize;
		}
	} else {
		//nothing
	}
	*/
	cur_size += nsize;
	data_size -= nsize;
	tx->cur_read_pos = cur_size;
	tx->data_size = data_size;
	
	pthread_mutex_unlock(&tx->iolock);
	
	return 0;
}

int buffer_seek(BufferContext *tx, int64_t offset, int whence)
{
	pthread_mutex_lock(&tx->iolock);
	tx->cur_read_pos = offset;
	pthread_mutex_unlock(&tx->iolock);
	return 0;
}

int64_t buffer_cur_read_pos(BufferContext *tx) 
{
	pthread_mutex_lock(&tx->iolock);
	int64_t size = tx->cur_read_pos;
	pthread_mutex_unlock(&tx->iolock);
	return size;
}

void buffer_cur_read_pos_set(BufferContext *tx, int64_t size) 
{
	pthread_mutex_lock(&tx->iolock);
	tx->cur_read_pos = size;
	pthread_mutex_unlock(&tx->iolock);
	//return size;
}

void buffer_flags_set(BufferContext *tx, int flag)
{
	pthread_mutex_lock(&tx->iolock);
	tx->flags |= flag;
	pthread_mutex_unlock(&tx->iolock);
}

int buffer_flags(BufferContext *tx)
{
	int flags;
	pthread_mutex_lock(&tx->iolock);
	flags = tx->flags;
	pthread_mutex_unlock(&tx->iolock);
	return flags;
}

/* curl API */

/* we use a global one for convenience */
//static CURLM *multi_handle;

/* curl calls this routine to get more data */
static size_t write_callback(char *buffer,
                             size_t size,
                             size_t nitems,
                             void *userp)
{
  char *newbuff;
  size_t rembuff;

  URL_FILE *url = (URL_FILE *)userp;
  size *= nitems;

  rembuff=url->buffer_len - url->buffer_pos; /* remaining space in buffer */

  if(size > rembuff) {
    /* not enough space in buffer */
    newbuff=(char *)realloc(url->buffer,url->buffer_len + (size - rembuff));
    if(newbuff==NULL) {
      fprintf(stderr,"callback buffer grow failed\n");
      size=rembuff;
    }
    else {
      /* realloc suceeded increase buffer size*/
      url->buffer_len+=size - rembuff;
      url->buffer=newbuff;
    }
  }

  memcpy(&url->buffer[url->buffer_pos], buffer, size);
  url->buffer_pos += size;

  return size;
}

/* use to attempt to fill the read buffer up to requested number of bytes */
static int fill_buffer(URL_FILE *file, size_t want)
{
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcep;
  struct timeval timeout;
  int rc;
  //CURLMcode code;

  /* only attempt to fill buffer if transactions still running and buffer
   * doesnt exceed required size already
   */
  if((!file->still_running) || (file->buffer_pos > want))
    return 0;

  /* attempt to fill buffer */
  do {
    int maxfd = -1;
    long curl_timeo = -1;

    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* set a suitable timeout to fail on */
    timeout.tv_sec = 5; /* 5 seconds */
    timeout.tv_usec = 0;

    curl_multi_timeout(file->multi_handle, &curl_timeo);
    if(curl_timeo >= 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1)
        timeout.tv_sec = 1;
      else
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }

    /* get file descriptors from the transfers */
    curl_multi_fdset(file->multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

    /* In a real-world program you OF COURSE check the return code of the
       function calls.  On success, the value of maxfd is guaranteed to be
       greater or equal than -1.  We call select(maxfd + 1, ...), specially
       in case of (maxfd == -1), we call select(0, ...), which is basically
       equal to sleep. */

    rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

    switch(rc) {
    case -1:
      /* select error */
      break;

    case 0:
    default:
      curl_multi_perform(file->multi_handle, &file->still_running);
	  if(rc == 0)
	  	return 0;
      break;
    }
  } while(file->still_running && (file->buffer_pos < want));
  
  /*
  static int size = 0;
  if(file->still_running == 0) {
	printf("want = %d\n", want);
  }  else {
	size += want;
  }
  */
  return 1;
}

/* use to remove want bytes from the front of a files buffer */
static int use_buffer(URL_FILE *file,int want)
{
  /* sort out buffer */
  if((file->buffer_pos - want) <=0) {
    /* ditch buffer - write will recreate */
    if(file->buffer)
      free(file->buffer);

    file->buffer=NULL;
    file->buffer_pos=0;
    file->buffer_len=0;
  }
  else {
    /* move rest down make it available for later */
    memmove(file->buffer,
            &file->buffer[want],
            (file->buffer_pos - want));

    file->buffer_pos -= want;
  }
  return 0;
}

URL_FILE *curl_fopen(const char *url,const char *operation)
{
  /* this code could check for URLs or types in the 'url' and
     basicly use the real fopen() for standard files */

  URL_FILE *file;
  (void)operation;

  file = (URL_FILE *)malloc(sizeof(URL_FILE));
  if(!file)
    return NULL;

  memset(file, 0, sizeof(URL_FILE));
  file->file_size = -1;

  if((file->handle.file=fopen(url,operation)))
    file->type = CFTYPE_FILE; /* marked as URL */

  else {
    file->type = CFTYPE_CURL; /* marked as URL */
    file->handle.curl = curl_easy_init();

    curl_easy_setopt(file->handle.curl, CURLOPT_URL, url);
    curl_easy_setopt(file->handle.curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(file->handle.curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(file->handle.curl, CURLOPT_WRITEFUNCTION, write_callback);

    //if(!multi_handle)
    file->multi_handle = curl_multi_init();

    curl_multi_add_handle(file->multi_handle, file->handle.curl);

    /* lets start the fetch */
    curl_multi_perform(file->multi_handle, &file->still_running);
	 //printf("fopen still running is %d\n", file->still_running);

    if((file->buffer_pos == 0) && (!file->still_running)) {
      /* if still_running is 0 now, we should return NULL */

      /* make sure the easy handle is not in the multi handle anymore */
      curl_multi_remove_handle(file->multi_handle, file->handle.curl);

      /* cleanup */
      curl_easy_cleanup(file->handle.curl);

	  curl_multi_cleanup(file->multi_handle);

      free(file);

      file = NULL;
    }
  }
  return file;
}

int curl_fclose(URL_FILE *file)
{
  int ret=0;/* default is good return */

  switch(file->type) {
  case CFTYPE_FILE:
    ret=fclose(file->handle.file); /* passthrough */
    break;

  case CFTYPE_CURL:
    /* make sure the easy handle is not in the multi handle anymore */
    curl_multi_remove_handle(file->multi_handle, file->handle.curl);

    /* cleanup */
    curl_easy_cleanup(file->handle.curl);

	curl_multi_cleanup(file->multi_handle);

	//curl_global_cleanup();
    break;

  default: /* unknown or supported type - oh dear */
    ret=EOF;
    errno=EBADF;
    break;
  }

  if(file->buffer)
    free(file->buffer);/* free any allocated buffer space */

  free(file);

  return ret;
}

int curl_feof(URL_FILE *file)
{
  int ret=0;

  switch(file->type) {
  case CFTYPE_FILE:
    ret=feof(file->handle.file);
    break;

  case CFTYPE_CURL:
    if((file->buffer_pos == 0) && (!file->still_running))
      ret = 1;
    break;

  default: /* unknown or supported type - oh dear */
    ret=-1;
    errno=EBADF;
    break;
  }
  //if(ret == 1)
  //	printf("eof, buffer_pos=%d, still_running=%d\n", file->buffer_pos, file->still_running);
  return ret;
}

size_t curl_fread(void *ptr, size_t size, size_t nmemb, URL_FILE *file)
{
  size_t want;

  switch(file->type) {
  case CFTYPE_FILE:
    want=fread(ptr,size,nmemb,file->handle.file);
    break;

  case CFTYPE_CURL:
    want = nmemb * size;

    fill_buffer(file,want);

    /* check if theres data in the buffer - if not fill_buffer()
     * either errored or EOF 
    */
    if(!file->buffer_pos) {
	  //printf("fread still running is %d\n", file->still_running);
	  //if(!file->still_running) {
		  //printf("=======================curl is not running\n");
	     // return -1;
	    //}
      return 0;
    }

    /* ensure only available data is considered */
    if(file->buffer_pos < want)
      want = file->buffer_pos;

    /* xfer data to caller */
    memcpy(ptr, file->buffer, want);

    use_buffer(file,want);

    want = want / size;     /* number of items */
    break;

  default: /* unknown or supported type - oh dear */
    want=0;
    errno=EBADF;
    break;

  }

  file->cur_pos += want;
  return want;
}

char *curl_fgets(char *ptr, size_t size, URL_FILE *file)
{
  size_t want = size - 1;/* always need to leave room for zero termination */
  size_t loop;

  switch(file->type) {
  case CFTYPE_FILE:
    ptr = fgets(ptr,size,file->handle.file);
    break;

  case CFTYPE_CURL:
    fill_buffer(file,want);

    /* check if theres data in the buffer - if not fill either errored or
     * EOF */
    if(!file->buffer_pos)
      return NULL;

    /* ensure only available data is considered */
    if(file->buffer_pos < want)
      want = file->buffer_pos;

    /*buffer contains data */
    /* look for newline or eof */
    for(loop=0;loop < want;loop++) {
      if(file->buffer[loop] == '\n') {
        want=loop+1;/* include newline */
        break;
      }
    }

    /* xfer data to caller */
    memcpy(ptr, file->buffer, want);
    ptr[want]=0;/* allways null terminate */

    use_buffer(file,want);

    break;

  default: /* unknown or supported type - oh dear */
    ptr=NULL;
    errno=EBADF;
    break;
  }

  return ptr;/*success */
}

int curl_fseek(URL_FILE *file, long offset, int whence)
{
	int ret = -1;
	char range[128] = {0};
	switch(file->type) {
	case CFTYPE_FILE:
	  ret = fseek(file->handle.file, offset, whence); /* passthrough */
	  break;
	
	case CFTYPE_CURL:
	  /* halt transaction */
	  curl_multi_remove_handle(file->multi_handle, file->handle.curl);

	  if(whence == SEEK_SET)
	  	sprintf(range, "%ld-", offset);
	  else if(whence == SEEK_END)
	  	sprintf(range, "%" PRId64 "-", file->file_size + offset);
	  else if(whence == SEEK_CUR)
	  	sprintf(range, "%" PRId64 "-", file->cur_pos + offset);

	  curl_easy_setopt(file->handle.curl, CURLOPT_RANGE, range);
	
	  /* restart */
	  curl_multi_add_handle(file->multi_handle, file->handle.curl);
	  curl_multi_perform(file->multi_handle, &file->still_running);
	
	  /* ditch buffer - write will recreate - resets stream pos*/
	  if(file->buffer)
		free(file->buffer);
	
	  file->buffer=NULL;
	  file->buffer_pos=0;
	  file->buffer_len=0;
      //printf("fseek still running is %d\n", file->still_running);
	  ret = 0;
	  break;
	
	default: /* unknown or supported type - oh dear */
	  break;
	}
	return ret;
}

int64_t curl_fsize(URL_FILE *file)
{
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcep;
  struct timeval timeout;
  int rc;
  double fsize = -1.0f;
	long tellsize = 0;
	long curpos_backup = 0;

  switch(file->type) {
  case CFTYPE_FILE:
  	curpos_backup = ftell(file->handle.file);
	fseek(file->handle.file, 0, SEEK_END); /* passthrough */
	tellsize = ftell(file->handle.file);
	fseek(file->handle.file, curpos_backup, SEEK_SET);
	file->file_size = tellsize;
	break;
  
  case CFTYPE_CURL:

	  /* only attempt to fill buffer if transactions still running and buffer
	   * doesnt exceed required size already
	   */
	 // if(!file->still_running)
	  //  return 0;

	  /* attempt to fill buffer */
	  do {
	    int maxfd = -1;
	    long curl_timeo = -1;

	    FD_ZERO(&fdread);
	    FD_ZERO(&fdwrite);
	    FD_ZERO(&fdexcep);

	    /* set a suitable timeout to fail on */
	    timeout.tv_sec = 5; /* 5 seconds */
	    timeout.tv_usec = 0;

	    curl_multi_timeout(file->multi_handle, &curl_timeo);
	    if(curl_timeo >= 0) {
	      timeout.tv_sec = curl_timeo / 1000;
	      if(timeout.tv_sec > 1)
	        timeout.tv_sec = 1;
	      else
	        timeout.tv_usec = (curl_timeo % 1000) * 1000;
	    }

	    /* get file descriptors from the transfers */
	    curl_multi_fdset(file->multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

	    /* In a real-world program you OF COURSE check the return code of the
	       function calls.  On success, the value of maxfd is guaranteed to be
	       greater or equal than -1.  We call select(maxfd + 1, ...), specially
	       in case of (maxfd == -1), we call select(0, ...), which is basically
	       equal to sleep. */

	    rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

	    switch(rc) {
	    case -1:
	      /* select error */
	      break;

	    case 0:
	    default:
	      /* timeout or readable/writable sockets */
	      curl_multi_perform(file->multi_handle, &file->still_running);
		  curl_easy_getinfo(file->handle.curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fsize);
	      break;
	    }
	  } while(file->still_running && fsize < 0);
	  file->file_size = fsize;
	  break;
	 
	 default: /* unknown or supported type - oh dear */
	   break;
  	}
  return file->file_size;
}


void curl_rewind(URL_FILE *file)
{
  switch(file->type) {
  case CFTYPE_FILE:
    rewind(file->handle.file); /* passthrough */
    break;

  case CFTYPE_CURL:
    /* halt transaction */
	curl_multi_remove_handle(file->multi_handle, file->handle.curl);

    /* restart */
    curl_multi_add_handle(file->multi_handle, file->handle.curl);

    /* ditch buffer - write will recreate - resets stream pos*/
    if(file->buffer)
      free(file->buffer);

    file->buffer=NULL;
    file->buffer_pos=0;
    file->buffer_len=0;

    break;

  default: /* unknown or supported type - oh dear */
    break;
  }
}
