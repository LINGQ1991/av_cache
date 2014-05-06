#include "ffmpeg.h"
#include <iostream>
#include <string.h>

//#include <evlog/evlog.h>

#define FFMPEG_DEBUG(fmt, arg...) printf("[%d] "fmt"\n", __LINE__, ##arg)
#define FFMPEG_INFO(fmt, arg...) printf("[%d] "fmt"\n", __LINE__, ##arg)
#define FFMPEG_WARN(fmt, arg...) printf("[%d] "fmt"\n", __LINE__, ##arg)
#define FFMPEG_ERROR(fmt, arg...) printf("[%d] "fmt"\n", __LINE__, ##arg)
#define FFMPEG_FATAL(fmt, arg...) printf("[%d] "fmt"\n", __LINE__, ##arg)

#ifndef INT64_MIN
#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#endif

#ifndef INT64_MAX
#define INT64_MAX INT64_C(9223372036854775807)
#endif

#define MAIN_BUFFER_SIZE 32768

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

static int write_data(void *opaque, uint8_t *buf, int buf_size)
{
	int ret = 0;
	RingBufferContext *ctx = (RingBufferContext*)opaque;
	int freesize = ring_buffer_freesize(ctx);
	
	//printf("\nfreesize = %d\n\n", freesize);
	
	int size = freesize;
	if(size <= 0)
		return 0;
	else if(size >= buf_size)
		size = buf_size;
	else
		FFMPEG_WARN("avio write %p to %p, writesize=%d, freesize=%d, rest=%d", buf, opaque, buf_size, freesize, freesize-buf_size);
	ret = ring_buffer_write(ctx, buf, size);
	if(ret == 0)
		ret = size;
	else
		ret = 0;
	return ret;
}

static int seek_data(void *opaque, int64_t offset, int whence)
{
	if(whence == AVSEEK_SIZE)
		return 0;
	if(offset==0 && whence==0)
		return 0;
	return -1;
}

static int i_read_data(void *opaque, uint8_t *buf, int buf_size)
{
	//printf("i_read_data run \n");
	
	BufferContext *tx = (BufferContext *)opaque;
	int64_t dsize = tx->size;
	int64_t rsize = tx->cur_read_pos;
	int64_t nsize = (int64_t)buf_size;
	int ret = 0;
redo:	
	if(nsize >= dsize - rsize) //未处理
		nsize = dsize - rsize;
	
	ret = buffer_read(tx, buf, nsize);
	if(ret == -2 || ret == -3) {
		usleep(2);
		goto redo;
	} else if(ret == -1) {
		printf("o---size > tail size\n");
		return 0;
	} else {
		
	}
	
	return nsize;
}

//ffmpeg seek 实现
static int64_t i_seek_data(void *opaque, int64_t offset, int whence)
{
	printf("i_seek_data run, offset:%lld \n", offset);
	BufferContext *tx = (BufferContext *)opaque;
	int64_t cur_pos =  buffer_cur_read_pos(tx);
	if(offset==0 && whence==0)
		return 0;
		
	if(whence == AVSEEK_SIZE) {
		return tx->data_size;
	}
	if(whence == SEEK_CUR) {
		//tx->cur_read_pos += offset;
		buffer_cur_read_pos_set(tx, cur_pos+offset);
	}
	if(whence == SEEK_END) {
		//tx->cur_read_pos = tx->size + offset;
		buffer_cur_read_pos_set(tx, tx->size + offset);
	}
	if(whence == SEEK_SET) {
		//tx->cur_read_pos = offset;
		buffer_cur_read_pos_set(tx, offset);
	}
	
	return tx->cur_read_pos;
}

static const char *ret_str(int v)
{
    static char buffer[256];
    switch (v) {
    case AVERROR_EOF:     return "-EOF";
    case AVERROR(EIO):    return "-EIO";
    case AVERROR(ENOMEM): return "-ENOMEM";
    case AVERROR(EINVAL): return "-EINVAL";
    default:
        snprintf(buffer, sizeof(buffer), "%2d", v);
        return buffer;
    }
}

namespace evideo
{
namespace multimedia
{

CFfmpeg::CFfmpeg()
{
	m_pFp = NULL;
	transcode = 0;
	infmt_ctx = NULL;
	video = -1;
	audio1 = -1;
	audio2 = -1;
	ring_buffer_init(&outputringbuffer, 1024*1024);
	buffer_init(&cbuffer, 1024*1024, 0); //+
	oc = NULL;
	vst = NULL; 
	ast1 = NULL;
	ast2 = NULL;
	bsfc = NULL;
	filesize = 0;
	curpos = 0;
	eof = 0;
	pthread_mutex_init(&iolock, NULL);
	pthread_mutex_init(&iolock2, NULL);
	vdts_base = 0;
	a1dts_base = 0;
	a2dts_base = 0;
	acodec1 = NULL;
	acodec2 = NULL;
	av_register_all();
	
	inputbuffer = (unsigned char*)malloc(MAIN_BUFFER_SIZE); //+
}

CFfmpeg::~CFfmpeg()
{
	pthread_mutex_destroy(&iolock);
	ring_buffer_free(&outputringbuffer);
	pthread_mutex_destroy(&iolock2);
	
	buffer_free(&cbuffer);
}

int CFfmpeg::Open(const char* pszUrl)
{
	unsigned int i;

	m_sUrl = pszUrl;
	m_sUrl.erase(0, strlen("ffmpeg://"));
	
	//+
	infmt_ctx = avformat_alloc_context();
	//ring_buffer_write(&cbuffer.ringbuffer, inputbuffer, sizeof(inputbuffer));
	//unsigned char* inputbuffer = NULL;
	//inputbuffer = (unsigned char*)malloc(MAIN_BUFFER_SIZE);
	init_put_byte(&inputpb, inputbuffer, MAIN_BUFFER_SIZE, 0, &cbuffer, i_read_data, NULL, i_seek_data );
	//inputpb.buf_end = inputpb.buf_ptr;
	infmt_ctx->pb = &inputpb;
	//av_read_frame(infmt_ctx, &pkt);
	//+
	avformat_open_input(&infmt_ctx, m_sUrl.c_str(), NULL, NULL);
	if(!infmt_ctx)
	{
		FFMPEG_ERROR("unknown url: %s", pszUrl);
		return -1;
	}
	
	av_find_stream_info(infmt_ctx);
	av_dump_format(infmt_ctx, 0, m_sUrl.c_str(), 0);
	
	filesize = avio_size(infmt_ctx->pb);
	printf("filesize = %d\n", filesize);
	
	check_transcode();

	if(!transcode)
	{
		if(infmt_ctx)
		{
			av_close_input_file(infmt_ctx);
			infmt_ctx = NULL;
		}
		m_pFp = fopen(m_sUrl.c_str(), "rb");
		if(!m_pFp)
		{
			//perror("fopen");
			FFMPEG_ERROR("error fopen: %s", strerror(errno));
			return -1;
		}
	}
	else
	{
		FFMPEG_DEBUG("transcode or remux");
		avformat_alloc_output_context2(&oc, NULL, "mpegts", NULL);

		unsigned int pid = 0x100;
		for(i=0; i<infmt_ctx->nb_streams; i++)
		{
			AVStream *stream = infmt_ctx->streams[i];
			if(stream->codec->codec_type==AVMEDIA_TYPE_VIDEO && video==-1)
			{
				video = i;
				FFMPEG_DEBUG("video index: %d, pid: 0x%x", i, pid++);
				vst = av_new_stream(oc, 0);
				avcodec_copy_context(vst->codec, infmt_ctx->streams[video]->codec); 
				//vst->codec->time_base = infmt_ctx->streams[video]->time_base;
				vst->codec->sample_aspect_ratio = vst->sample_aspect_ratio = infmt_ctx->streams[video]->codec->sample_aspect_ratio;
				vst->stream_copy = 1;
				vst->avg_frame_rate = infmt_ctx->streams[video]->avg_frame_rate;
				vst->discard = AVDISCARD_NONE;
				vst->disposition = infmt_ctx->streams[video]->disposition;
				vst->duration = infmt_ctx->streams[video]->duration;
				vst->first_dts = infmt_ctx->streams[video]->first_dts;
				vst->r_frame_rate = infmt_ctx->streams[video]->r_frame_rate;
				vst->time_base = infmt_ctx->streams[video]->time_base;
				vst->quality = infmt_ctx->streams[video]->quality;
				vst->start_time = infmt_ctx->streams[video]->start_time;
			}
			else if(stream->codec->codec_type==AVMEDIA_TYPE_AUDIO && audio1==-1)
			{
				audio1 = i;
				FFMPEG_DEBUG("audio1 index: %d, pid: 0x%x", i, pid++);
				ast1 = av_new_stream(oc, 0);
				if(stream->codec->codec_id == CODEC_ID_AC3
					|| stream->codec->codec_id == CODEC_ID_DTS
					|| stream->codec->codec_id == CODEC_ID_PCM_S16BE
					|| stream->codec->codec_id == CODEC_ID_PCM_S16LE)
				{
					acodec1 = stream->codec;
					AVCodec *inAcodec = avcodec_find_decoder(stream->codec->codec_id);     
					avcodec_open(stream->codec, inAcodec);     
					AVCodec *outAcodec = avcodec_find_encoder(CODEC_ID_MP2);
					//ast1->codec = avcodec_alloc_context3(outAcodec);
					ast1->codec->bit_rate = 128000;
					ast1->codec->sample_rate = stream->codec->sample_rate;
					if(stream->codec->channels > 2)
					{
						stream->codec->request_channels = 2;
					}
					ast1->codec->channels = 2;
					ast1->codec->sample_fmt = AV_SAMPLE_FMT_S16;
					avcodec_open(ast1->codec, outAcodec);
					ast1->codec->time_base = infmt_ctx->streams[audio1]->time_base;
					ring_buffer_init(&adecrbuffer1, 524288);
				}
				else
				{
					avcodec_copy_context(ast1->codec, infmt_ctx->streams[audio1]->codec);
					//ast1->codec->time_base = infmt_ctx->streams[audio1]->time_base;
					ast1->stream_copy = 1;
					ast1->first_dts = infmt_ctx->streams[audio1]->first_dts;
					ast1->r_frame_rate = infmt_ctx->streams[audio1]->r_frame_rate;
					ast1->time_base = infmt_ctx->streams[audio1]->time_base;
					ast1->quality = infmt_ctx->streams[audio1]->quality;
					ast1->start_time = infmt_ctx->streams[audio1]->start_time;
					ast1->duration = infmt_ctx->streams[audio1]->duration;
				}
			}
			else if(stream->codec->codec_type==AVMEDIA_TYPE_AUDIO && audio1!=i && audio2==-1)
			{
				audio2 = i;
				FFMPEG_DEBUG("audio2 index: %d, pid: 0x%x", i, pid++);
				ast2 = av_new_stream(oc, 0);
				if(stream->codec->codec_id == CODEC_ID_AC3
					|| stream->codec->codec_id == CODEC_ID_DTS
					|| stream->codec->codec_id == CODEC_ID_PCM_S16BE
					|| stream->codec->codec_id == CODEC_ID_PCM_S16LE)
				{
					acodec2 = stream->codec;
					AVCodec *inAcodec = avcodec_find_decoder(stream->codec->codec_id);     
					avcodec_open(stream->codec, inAcodec);     
					AVCodec *outAcodec = avcodec_find_encoder(CODEC_ID_MP2);
					//ast2->codec = avcodec_alloc_context3(outAcodec);
					ast2->codec->bit_rate = 128000;
					ast2->codec->sample_rate = stream->codec->sample_rate;
					if(stream->codec->channels > 2)
					{
						stream->codec->request_channels = 2;
					}
					ast2->codec->channels = 2;
					ast2->codec->sample_fmt = AV_SAMPLE_FMT_S16;
					avcodec_open(ast2->codec, outAcodec);
					ast2->codec->time_base = infmt_ctx->streams[audio2]->time_base;
					ring_buffer_init(&adecrbuffer2, 524288);
				}
				else
				{
					avcodec_copy_context(ast2->codec, infmt_ctx->streams[audio2]->codec);
					//ast2->codec->time_base = infmt_ctx->streams[audio2]->time_base;
					ast2->stream_copy = 1;
					ast2->first_dts = infmt_ctx->streams[audio2]->first_dts;
					ast2->r_frame_rate = infmt_ctx->streams[audio2]->r_frame_rate;
					ast2->time_base = infmt_ctx->streams[audio2]->time_base;
					ast2->quality = infmt_ctx->streams[audio2]->quality;
					ast2->start_time = infmt_ctx->streams[audio2]->start_time;
					ast2->duration = infmt_ctx->streams[audio2]->duration;
				}
			}
		}
		
		init_put_byte(&outputpb, outputbuffer, MAIN_BUFFER_SIZE, 1, &outputringbuffer, NULL, write_data, NULL );
		oc->pb = &outputpb;
		avformat_write_header(oc, NULL);
		//av_dump_format(oc, 0, "output.ts", 1);

		if(infmt_ctx->streams[video]->codec->codec_id == CODEC_ID_H264)
		{
			FFMPEG_DEBUG("open h264_mp4toannexb filter");
			bsfc = av_bitstream_filter_init("h264_mp4toannexb");
			if (!bsfc)
			{
				FFMPEG_ERROR("Cannot open the h264_mp4toannexb BSF!");
				return -1;
			}
		}
	}
	return 0;
}

int CFfmpeg::Close(void)
{
	if(acodec1)
	{
		avcodec_close(acodec1);
		avcodec_close(ast1->codec);
		ring_buffer_free(&adecrbuffer1);
	}
	if(acodec2)
	{
		avcodec_close(acodec2);
		avcodec_close(ast2->codec);
		ring_buffer_free(&adecrbuffer2);
	}
	if(infmt_ctx)
	{
		av_close_input_file(infmt_ctx);
		infmt_ctx = NULL;
	}
	if(oc)
	{
		av_write_trailer(oc);
		avformat_free_context(oc);
		oc = NULL;
	}
	if(bsfc)
	{
		av_bitstream_filter_close(bsfc);
	}
	
	return 0;
}

int CFfmpeg::Play(void)
{
	
	return 0;
}

int CFfmpeg::Stop(void)
{
	return 0;
}

int CFfmpeg::Pause(void)
{
	return 0;
}

int CFfmpeg::ReadData(int nNeed, unsigned char* pBuffer, int* pnRead)
{
	int buffer_read = 0;
	int ret;
	AVPacket pkt;

	pthread_mutex_lock(&iolock);
	if(!transcode)
	{
		ret = fread(pBuffer, 1, nNeed, m_pFp);
		if(ret<=0)
		{
			FFMPEG_ERROR("fread: %s", strerror(errno));
		}
		else
		{
			curpos += ret;
			buffer_read = ret;
		}
	}
	else
	{
		while(1)
		{
			ret = ring_buffer_datasize(&outputringbuffer);
			if(ret > 0)
			{
				int reqsize = nNeed - buffer_read;
				if(ret < reqsize)
					reqsize = ret;
				ret = ring_buffer_read(&outputringbuffer, pBuffer+buffer_read, reqsize);
				if(ret == 0)
				{
					buffer_read += reqsize;
				}
			}
			if(buffer_read >= nNeed)
				break;
			ret = av_read_frame(infmt_ctx, &pkt);
			if(ret == AVERROR_EOF)
			{
				FFMPEG_INFO("=== AVERROR_EOF");
				eof = 1;
				*pnRead = buffer_read;
				//pthread_mutex_lock(&iolock);
				curpos = avio_seek(infmt_ctx->pb, 0, SEEK_CUR);
				pthread_mutex_unlock(&iolock);
				return buffer_read;
			}
			else if(ret == AVERROR(EAGAIN))
			{
				continue;
			}
			else if(ret < 0)
			{
				FFMPEG_WARN("av_read_frame return %d", ret);
				*pnRead = buffer_read;
				//pthread_mutex_lock(&iolock);
				curpos = avio_seek(infmt_ctx->pb, 0, SEEK_CUR);
				pthread_mutex_unlock(&iolock);
				return buffer_read;
			}
			else if(ret >= 0)
			{
				AVStream *ost, *ist;
				AVFrame avframe;
				AVPacket opkt;
				int64_t pts_base;
				av_init_packet(&opkt);
				//opkt = pkt;
				if(pkt.stream_index == video)
				{
					ost = vst;
					ist = infmt_ctx->streams[video];
					opkt.stream_index = vst->index;
					pts_base = vdts_base;
					//printf("pts=%lld, dts=%lld, duration=%d\n", pkt.pts, pkt.dts, pkt.duration);
				}
				else if(pkt.stream_index == audio1)
				{
					ost = ast1;
					ist = infmt_ctx->streams[audio1];
					opkt.stream_index = ast1->index;
					pts_base = a1dts_base;

					if(acodec1)
					{
						uint8_t outbuffer[4096];
						int sample_size = sizeof(adecbuffer1);
						int frame_size = ost->codec->frame_size * 4;
						//FFMPEG_DEBUG("before decode, pts=0x%llx, dts=0x%llx", pkt.pts, pkt.dts);
						avcodec_decode_audio3(ist->codec, adecbuffer1, &sample_size, &pkt);
						//FFMPEG_DEBUG("after decode, pts=0x%llx, dts=0x%llx", pkt.pts, pkt.dts);
						ring_buffer_write(&adecrbuffer1, (uint8_t*)adecbuffer1, sample_size);
						while(ring_buffer_datasize(&adecrbuffer1) > frame_size)
						{
							av_init_packet(&opkt);
							ring_buffer_read(&adecrbuffer1, (uint8_t*)adecbuffer1, frame_size);
							ret = avcodec_encode_audio(ost->codec, outbuffer, sizeof(outbuffer), adecbuffer1);
							//printf("ret=%d\n", ret);
							opkt.data = outbuffer;
							opkt.size = ret;
							opkt.stream_index = ast1->index;
							if(pkt.pts != AV_NOPTS_VALUE)
								opkt.pts = av_rescale_q(pkt.pts, ist->time_base, ost->time_base)+pts_base;
							else
								opkt.pts = AV_NOPTS_VALUE;
							if (pkt.dts != AV_NOPTS_VALUE)
								opkt.dts = av_rescale_q(pkt.dts, ist->time_base, ost->time_base)+pts_base;
							else
								opkt.dts = av_rescale_q(pkt.dts, AV_TIME_BASE_Q, ost->time_base)+pts_base;
							opkt.duration = av_rescale_q(pkt.duration, ist->time_base, ost->time_base);
							opkt.flags |= AV_PKT_FLAG_KEY;
							//FFMPEG_DEBUG("audio1 rescaled, pts=0x%llx, dts=0x%llx", opkt.pts, opkt.dts);
							ret = av_interleaved_write_frame(oc, &opkt);
							if(ret != 0)
							{
								FFMPEG_ERROR("av_interleaved_write_frame ret %d", ret);
							}
							ost->codec->frame_number++;
							av_free_packet(&opkt);
						}
						av_free_packet(&pkt);
						continue;
					}
				}
				else if(pkt.stream_index == audio2)
				{
					ost = ast2;
					ist = infmt_ctx->streams[audio2];
					opkt.stream_index = ast2->index;
					pts_base = a2dts_base;

					if(acodec2)
					{
						uint8_t outbuffer[4096];
						int sample_size = sizeof(adecbuffer1);
						int frame_size = ost->codec->frame_size * 4;
						avcodec_decode_audio3(ist->codec, adecbuffer2, &sample_size, &pkt);
						ring_buffer_write(&adecrbuffer2, (uint8_t*)adecbuffer2, sample_size);
						while(ring_buffer_datasize(&adecrbuffer2) > frame_size)
						{
							av_init_packet(&opkt);
							ring_buffer_read(&adecrbuffer2, (uint8_t*)adecbuffer2, frame_size);
							ret = avcodec_encode_audio(ost->codec, outbuffer, sizeof(outbuffer), adecbuffer2);
							//printf("ret=%d\n", ret);
							opkt.data = outbuffer;
							opkt.size = ret;
							opkt.stream_index = ast2->index;
							if(pkt.pts != AV_NOPTS_VALUE)
								opkt.pts = av_rescale_q(pkt.pts, ist->time_base, ost->time_base)+pts_base;
							else
								opkt.pts = AV_NOPTS_VALUE;
							if (pkt.dts != AV_NOPTS_VALUE)
								opkt.dts = av_rescale_q(pkt.dts, ist->time_base, ost->time_base)+pts_base;
							else
								opkt.dts = av_rescale_q(pkt.dts, AV_TIME_BASE_Q, ost->time_base)+pts_base;
							opkt.duration = av_rescale_q(pkt.duration, ist->time_base, ost->time_base);
							opkt.flags |= AV_PKT_FLAG_KEY;
							ret = av_interleaved_write_frame(oc, &opkt);
							if(ret != 0)
							{
								FFMPEG_ERROR("av_interleaved_write_frame ret %d", ret);
							}
							ost->codec->frame_number++;
							av_free_packet(&opkt);
						}
						av_free_packet(&pkt);
						continue;
					}
				}
				else
				{
					av_free_packet(&pkt);
					continue;
				}

				avcodec_get_frame_defaults(&avframe);
				ost->codec->coded_frame = &avframe;
				avframe.key_frame = pkt.flags & AV_PKT_FLAG_KEY;
				if(pkt.pts != AV_NOPTS_VALUE)
					opkt.pts = av_rescale_q(pkt.pts, ist->time_base, ost->time_base)+pts_base;
				else
					opkt.pts = AV_NOPTS_VALUE;
				if (pkt.dts != AV_NOPTS_VALUE)
					opkt.dts = av_rescale_q(pkt.dts, ist->time_base, ost->time_base)+pts_base;
				else
					opkt.dts = av_rescale_q(pkt.dts, AV_TIME_BASE_Q, ost->time_base)+pts_base;
				opkt.duration = av_rescale_q(pkt.duration, ist->time_base, ost->time_base);
				opkt.data = pkt.data;
				opkt.size = pkt.size;
				if(bsfc && pkt.stream_index == video)
				{
					//printf("rescale pts=%lld, dts=%lld, duration=%d\n", opkt.pts, opkt.dts, opkt.duration);
					AVPacket new_pkt = opkt;
					int a = av_bitstream_filter_filter(bsfc, ost->codec, NULL, &new_pkt.data, &new_pkt.size, opkt.data, opkt.size, opkt.flags & AV_PKT_FLAG_KEY);
					if(a>0)
					{
						av_free_packet(&opkt);
						new_pkt.destruct = av_destruct_packet;
					}
					else if(a<0)
					{
						FFMPEG_ERROR("av_bitstream_filter_filter ret %d", a);
					}
					opkt = new_pkt;
				}
				ret = av_interleaved_write_frame(oc, &opkt);
				if(ret != 0)
				{
					FFMPEG_ERROR("av_interleaved_write_frame ret %d", ret);
				}
				ost->codec->frame_number++;
			}
			av_free_packet(&pkt);
		}
		curpos = avio_seek(infmt_ctx->pb, 0, SEEK_CUR);
	}
	*pnRead = buffer_read;
	pthread_mutex_unlock(&iolock);
	return buffer_read;
}

int CFfmpeg::FastForward(void)
{
	FFMPEG_WARN("NOT SUPPORTED!");
	return -1;
}

int CFfmpeg::FastReverse(void)
{
	FFMPEG_WARN("NOT SUPPORTED!");
	return -1;
}

int CFfmpeg::Seek(int64_t pos)
{
	int ret;
	double rate;
      int64_t seek_target;
	pthread_mutex_lock(&iolock);
	if(transcode)
	{
		rate = ((double)pos)/((double)filesize);
		seek_target = infmt_ctx->duration * rate;
		FFMPEG_DEBUG("rate=%f, target = %lld, duration=%lld", rate, seek_target, infmt_ctx->duration);
		int defaultStreamIndex = av_find_default_stream_index(infmt_ctx);
		seek_target = av_rescale_q(seek_target, AV_TIME_BASE_Q, infmt_ctx->streams[defaultStreamIndex]->time_base);
		ret = av_seek_frame(infmt_ctx, defaultStreamIndex, seek_target, AVSEEK_FLAG_ANY);
		FFMPEG_DEBUG("ret = %s", ret_str(ret));
	      eof= 0;
		if(curpos > pos)
		{
			if(vst)
				vdts_base = vst->cur_dts;
			if(ast1)
				a1dts_base = ast1->cur_dts;
			if(ast2)
				a2dts_base = ast2->cur_dts;
		}
		outputringbuffer.data_ptr = outputringbuffer.buffer_base;
		outputringbuffer.data_size = 0;
	}
	else
	{
		ret = fseek(m_pFp, pos, SEEK_SET);
		if(ret == 0)
		{
			curpos = pos;
		}
		FFMPEG_DEBUG("pos=%lld, m_nCurpos=%lld", pos, curpos);
	}
	pthread_mutex_unlock(&iolock);
	return 0;
}

int CFfmpeg::GetParam(int nIndex, int nSize, void* pValue)
{
	switch(nIndex)
	{
	case VSP_URL:
		strncpy((char *)pValue, m_sUrl.c_str(), (unsigned int)nSize);
		if (m_sUrl.length() > (unsigned int)nSize)
			((char *)pValue)[nSize] = 0;
		break;
	case VSP_DURATION:
		*(int64_t *)pValue = filesize;
		break;
	case VSP_CURPOS:
		*(int64_t *)pValue = curpos;
		break;
	case VSP_TEST_EOF:
		*(int *)pValue = eof;
	default:
		break;
	}
	return 0;
}

void CFfmpeg::check_transcode(void)
{
	int i;
	AVStream *stream;
	FFMPEG_DEBUG("probed type: %s", infmt_ctx->iformat->name);
	if((strcmp(infmt_ctx->iformat->name,"mpegts") == 0)
		|| (strcmp(infmt_ctx->iformat->name,"avi") == 0)
		|| (strcmp(infmt_ctx->iformat->name,"mpeg") == 0)
		|| (strcmp(infmt_ctx->iformat->name,"mp3") == 0))
	{
		transcode = 0;
	}
/*	else if((strcmp(infmt_ctx->iformat->name,"mov,mp4,m4a,3gp,3g2,mj2") == 0)
		|| (strcmp(infmt_ctx->iformat->name,"matroska,webm") == 0)
		|| (strcmp(infmt_ctx->iformat->name,"flv") == 0))
	{
		transcode = 1;
	}*/
	else
	{
		//MULTIMEDIA_WARN("unknown container format");
		transcode = 1;
	}

	for(i=0; i<infmt_ctx->nb_streams; i++)	 
	{	
		stream = infmt_ctx->streams[i];
		if(stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)//2é?òò??μá÷
		{
			switch (stream->codec->codec_id)
			{
				/*case CODEC_ID_MP2:
				case CODEC_ID_MP3:
				case CODEC_ID_AAC:
					transcode = 0;
					break;*/
				case CODEC_ID_AC3:
				case CODEC_ID_DTS:
				case CODEC_ID_PCM_S16BE:
				case CODEC_ID_PCM_S16LE:
					transcode = 1;
					break;
				default:
					break;
			}
		}
	}
}

}
}




