#ifndef CBB_MULTIMEDIA_FFMPEG_H_
#define CBB_MULTIMEDIA_FFMPEG_H_

#include <stdio.h>
#include <string>

#ifdef __cplusplus
extern "C" {

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <libavformat/avformat.h>

}
#endif

#define VSP_SERVERTYPE		1	/*���������� (int)*/
#define VSP_SERVERNAME		2	/*������IP (char[64])*/
#define VSP_CONTROLPORT		3	/*�������˿���port (int)*/
#define VSP_URL				4	/*�㲥URL (char[128])*/
#define VSP_USERNAME		5	/*FTPЭ���еĵ�½�û��� (char[128])*/
#define VSP_PASSWORD		6	/*FTPЭ���еĵ�½���� (char[128]))*/
#define VSP_DURATION		7	/*��Ŀ���� (int)*/
#define VSP_PROGRAMNAME		8	/*��Ŀ���� (char[128])*/
#define VSP_VIDEOCODING		9	/*������ʽ (int)*/
#define VSP_AUDIOCODING		10	/*��Ƶ��ʽ (int)*/
#define VSP_MAXBITRATE		11	/*������������� (int)*/
#define VSP_TIMEOUT			12  /*keepaliveʱ���� (int)*/
#define VSP_CURTIME			13	/*Current Play Time (int)*/
#define VSP_OFFSET			14	/*Seek��λ��ƫ���� (int)*/
#define VSP_GETLASTERROR	15	/*���һ�ε��õĴ�����Ϣ (int)*/
#define VSP_AUDIOVOLUME		16	/*Ĭ�������ͻ�׼�����Ĳ�ֵ (int)*/
#define VSP_AUDIOKLOK		17	/*VCD���� (int)*/
#define VSP_AUDIOMAXVALUE	18 	/*���������ֵ (int)*/
#define VSP_MEMORY			19	/*��ǰʣ���ڴ�ֵ (int)*/
#define VSP_GPITEM			20	/*����GetParamʱ������ITEM (int)*/
#define VSP_VIDEOWIDTH		21	/*��Ŀ�� (int)*/
#define VSP_VIDEOHEIGHT		22	/*��Ŀ�� (int)*/

#define VSP_CURPOS 100
#define VSP_TEST_EOF 101

typedef struct {
	uint8_t* buffer_base;
	uint8_t* buffer_end;
	int buffer_size;
	uint8_t* data_ptr;
	int data_size;
}RingBufferContext;

namespace evideo
{
namespace multimedia
{

class CFfmpeg
{
public:
	CFfmpeg();
	~CFfmpeg();

	int Open(const char* pszUrl);
	int Close(void);
	int Play(void);
	int Stop(void);
	int Pause(void);
	int ReadData(int nNeed, unsigned char* pBuffer, int* pnRead);
	int FastForward(void);
	int FastReverse(void);
	int Seek(int64_t pos);

	int GetParam(int nIndex, int nSize, void* pValue);

private:
	void check_transcode(void);
	FILE* m_pFp;
	std::string m_sUrl;
	int transcode;

	AVFormatContext *infmt_ctx;
	int video;
	int audio1;
	int audio2;
	AVFormatContext *oc;
	AVStream *vst;
	AVStream *ast1;
	AVStream *ast2;
	AVIOContext outputpb;
	RingBufferContext outputringbuffer;
	AVBitStreamFilterContext *bsfc;

	int64_t filesize;
	int64_t curpos;
	int eof;
	unsigned char outputbuffer[32768];

	pthread_mutex_t iolock;
	int64_t vdts_base;
	int64_t a1dts_base;
	int64_t a2dts_base;

	short adecbuffer1[288000];
	short adecbuffer2[288000];

	AVCodecContext *acodec1;
	AVCodecContext *acodec2;
	RingBufferContext adecrbuffer1;
	RingBufferContext adecrbuffer2;
};

}
}

#endif //CBB_MULTIMEDIA_FFMPEG_H_


