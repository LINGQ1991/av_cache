#include <stdio.h>
#include <iostream>
#include "ffmpeg.h"

using namespace std;
using namespace evideo::multimedia;

int main(int argc, char** argv)
{
	string url = "ffmpeg://";
	url += argv[1];
	unsigned char buffer[32768];
	int nread = 0;
	int ret = 0;
	FILE* fp_write = NULL;
	fp_write = fopen("output.ts", "w+");
	CFfmpeg ffmpeg;
	ffmpeg.Open(url.c_str());
	do
	{
		ret = ffmpeg.ReadData(sizeof(buffer), buffer, &nread);
		fwrite(buffer, 1, nread, fp_write);
		//printf("hello\n");
		//usleep(1);
	}while(ret > 0);
	ffmpeg.Close();
	fclose(fp_write);
	return 0;
}
