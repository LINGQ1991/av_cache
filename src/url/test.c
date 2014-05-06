#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "av_string.h"
#include "url.h"

URLProtocol test_protocol = {
    .name         		= "test",
    .url_open      	= NULL,
    .url_read        	= NULL,
    .url_write        	= NULL,
    .url_close       	= NULL,
    .url_get_file_handle = NULL,
};

URLProtocol test_protocol2 = {
    .name         		= "test2",
    .url_open      	= NULL,
    .url_read        	= NULL,
    .url_write        	= NULL,
    .url_close       	= NULL,
    .url_get_file_handle = NULL,
};


int main(void)
{
	printf("\n########parse protocol test##########\n\n");
	
	char *url = "test2://root:root@61.233.202.10:8080/gsp/1.rmvb";
	//char *file = "/opt/nfsroot/cache/db/test";
	char *tcp = "tcp://192.168.74.162:6000/test.txt";
	char proto[8], hostname[256], path[1024], authorization[256];
	int port = 0;
	
	av_url_split(	proto, sizeof(proto),
						authorization, sizeof(authorization),
						hostname, sizeof(hostname),
						&port,
						path, sizeof(path),
						url);
		
	printf("%s\n%s\n%s\n%d\n%s\n",
			proto,
			authorization,
			hostname,
			port,
			path
			);
	
	printf("\n################ end ################\n");
			
	printf("\n########register protocol test########\n\n");
	
	//ffurl_register_protocol(&test_protocol, sizeof(test_protocol));
	//ffurl_register_protocol(&test_protocol2, sizeof(test_protocol2));
	REGISTER_PROTOCOL(&FILE, file);
	REGISTER_PROTOCOL(&TCP, tcp);
	
	URLProtocol *pro = NULL;
	pro = av_protocol_next(pro);
	
	while(pro) {
		printf("%s\n", pro->name);
		pro = av_protocol_next(pro);
	}

	printf("\n################ end ################\n");

	
	printf("\n########file protocol test########\n\n");
	
	URLContext *puc;
	int ret;
	unsigned char buf[128] = "hello world. seek.";
	//memset(buf, 0, sizeof(buf));
	//printf("%d\n", *puc);
	//ffurl_alloc(puc, file, 0);
	//printf("%s\n", (*puc)->filename);
	//printf("%d\n", *puc);
/*	ret = ffurl_open(&puc, tcp, AVIO_FLAG_READ_WRITE);
	if(ret) {
		printf("Open failed.\n");
	}
	
	//printf("%d\n", puc->is_connected);
	ret = ffurl_write(puc, buf, strlen(buf));
	if(ret < 0) {
		printf("Write failed = %d\n", ret);
	} 
	
	ffurl_close(puc);
*/	
	ret = ffurl_open(&puc, tcp, AVIO_FLAG_READ);
	if(ret) {
		printf("Open failed.\n");
	}
	
	memset(buf, 0, sizeof(buf));
/*	
	ret = ffurl_seek(puc, 13, 0);
	if(ret == -1) {
		printf("Seek failed.\n");
	} else {
		printf("Seek = %d\n", ret);
	}
*/	
	ret = ffurl_read(puc, buf, sizeof(buf));
	if(ret < 0) {
		printf("Read failed = %d\n", ret);
	} else {
		printf("ret = %d\n", ret);
		printf("%s\n", buf);
	}	

	printf("\n################ end ################\n");
	
	//printf("%d\n", AVERROR(EIO));
	
	return 0;
}