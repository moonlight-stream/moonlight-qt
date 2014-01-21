//
//  VideoDecoder.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/18/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "VideoDecoder.h"
#import "libs/ffmpeg/armv7/include/libavcodec/avcodec.h"
#import "swscale.h"
#include <pthread.h>

@implementation VideoDecoder
- (id) init
{
    self = [super init];
    return self;
}

// General decoder and renderer state
AVPacket pkt;
AVCodec* decoder;
AVCodecContext* decoder_ctx;
AVFrame* yuv_frame;
AVFrame* dec_frame;
pthread_mutex_t mutex;

// Color conversion and rendering
AVFrame* rgb_frame;
char* rgb_frame_buf;
struct SwsContext* scaler_ctx;
int render_pix_fmt;

#define BYTES_PER_PIXEL 4

// This function must be called before
// any other decoding functions
int nv_avc_init(int width, int height, int perf_lvl, int thread_count) {
	int err;
	int filtering;
    
	pthread_mutex_init(&mutex, NULL);
    
	// Initialize the avcodec library and register codecs
	av_log_set_level(AV_LOG_QUIET);
	avcodec_register_all();
	
	av_init_packet(&pkt);
    
	decoder = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (decoder == NULL) {
	//	__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
           //                 "Couldn't find H264 decoder");
		return -1;
	}
    
	decoder_ctx = avcodec_alloc_context3(decoder);
	if (decoder_ctx == NULL) {
		//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
          //                  "Couldn't allocate context");
		return -1;
	}
    
	// Show frames even before a reference frame
	decoder_ctx->flags2 |= CODEC_FLAG2_SHOW_ALL;
    
	if (perf_lvl & DISABLE_LOOP_FILTER) {
		// Skip the loop filter for performance reasons
		decoder_ctx->skip_loop_filter = AVDISCARD_ALL;
	}
    
	if (perf_lvl & LOW_LATENCY_DECODE) {
		// Use low delay single threaded encoding
		decoder_ctx->flags |= CODEC_FLAG_LOW_DELAY;
	}
    
	if (perf_lvl & SLICE_THREADING) {
		decoder_ctx->thread_type = FF_THREAD_SLICE;
	}
	else {
		decoder_ctx->thread_type = FF_THREAD_FRAME;
	}
    
	decoder_ctx->thread_count = thread_count;
    
	decoder_ctx->width = width;
	decoder_ctx->height = height;
	decoder_ctx->pix_fmt = PIX_FMT_YUV420P;
    
	render_pix_fmt = AV_PIX_FMT_BGR0;
    
	err = avcodec_open2(decoder_ctx, decoder, NULL);
	if (err < 0) {
		//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
          //                  "Couldn't open codec");
		return err;
	}
    
	dec_frame = av_frame_alloc();
	if (dec_frame == NULL) {
		//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
          //                  "Couldn't allocate frame");
		return -1;
	}
	
	if (!(perf_lvl & NO_COLOR_CONVERSION)) {
		rgb_frame = av_frame_alloc();
		if (rgb_frame == NULL) {
			//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
              //                  "Couldn't allocate frame");
			return -1;
		}
        
		rgb_frame_buf = (char*)av_malloc(width * height * BYTES_PER_PIXEL);
		if (rgb_frame_buf == NULL) {
			//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
              //                  "Couldn't allocate picture");
			return -1;
		}
        
		err = avpicture_fill((AVPicture*)rgb_frame,
                             rgb_frame_buf,
                             render_pix_fmt,
                             decoder_ctx->width,
                             decoder_ctx->height);
		if (err < 0) {
			//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
              //                  "Couldn't fill picture");
			return err;
		}
        
		if (perf_lvl & FAST_BILINEAR_FILTERING) {
			filtering = SWS_FAST_BILINEAR;
		}
		else if (perf_lvl & BILINEAR_FILTERING) {
			filtering = SWS_BILINEAR;
		}
		else {
			filtering = SWS_BICUBIC;
		}
        
		scaler_ctx = sws_getContext(decoder_ctx->width,
                                    decoder_ctx->height,
                                    decoder_ctx->pix_fmt,
                                    decoder_ctx->width,
                                    decoder_ctx->height,
                                    render_pix_fmt,
                                    filtering,
                                    NULL, NULL, NULL);
		if (scaler_ctx == NULL) {
			//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
              //                  "Couldn't get scaler context");
			return -1;
		}
	}
    
	return 0;
}

// This function must be called after
// decoding is finished
void nv_avc_destroy(void) {
	if (decoder_ctx) {
		avcodec_close(decoder_ctx);
		av_free(decoder_ctx);
		decoder_ctx = NULL;
	}
	if (scaler_ctx) {
		sws_freeContext(scaler_ctx);
		scaler_ctx = NULL;
	}
	if (dec_frame) {
		av_frame_free(&dec_frame);
		dec_frame = NULL;
	}
	if (yuv_frame) {
		av_frame_free(&yuv_frame);
		yuv_frame = NULL;
	}
	if (rgb_frame) {
		av_frame_free(&rgb_frame);
		rgb_frame = NULL;
	}
	if (rgb_frame_buf) {
		av_free(rgb_frame_buf);
		rgb_frame_buf = NULL;
	}

	pthread_mutex_destroy(&mutex);
}

static AVFrame* dequeue_new_frame(void) {
	AVFrame *our_yuv_frame = NULL;
    
	pthread_mutex_lock(&mutex);
    
	// Check if there's a new frame
	if (yuv_frame) {
		// We now own the decoder's frame and are
		// responsible for freeing it when we're done
		our_yuv_frame = yuv_frame;
		yuv_frame = NULL;
	}
    
	pthread_mutex_unlock(&mutex);
    
	return our_yuv_frame;
}

static int update_rgb_frame(void) {
	AVFrame *our_yuv_frame;
	int err;
    
	our_yuv_frame = dequeue_new_frame();
	if (our_yuv_frame == NULL) {
		return 0;
	}
    
	// Convert the YUV image to RGB
	err = sws_scale(scaler_ctx,
                    our_yuv_frame->data,
                    our_yuv_frame->linesize,
                    0,
                    decoder_ctx->height,
                    rgb_frame->data,
                    rgb_frame->linesize);
    
	av_frame_free(&our_yuv_frame);
    
	if (err != decoder_ctx->height) {
		//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
          //                  "Scaling failed");
		return 0;
	}
    
	return 1;
}

static int render_rgb_to_buffer(char* buffer, int size) {
	int err;
    
	// Draw the frame to the buffer
	err = avpicture_layout((AVPicture*)rgb_frame,
                           render_pix_fmt,
                           decoder_ctx->width,
                           decoder_ctx->height,
                           buffer,
                           size);
	if (err < 0) {
	//	__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
      //                      "Picture fill failed");
		return 0;
	}
    
	return 1;
}

int nv_avc_get_raw_frame(char* buffer, int size) {
	AVFrame *our_yuv_frame;
	int err;
    
	our_yuv_frame = dequeue_new_frame();
	if (our_yuv_frame == NULL) {
		return 0;
	}
    
	err = avpicture_layout((AVPicture*)our_yuv_frame,
                           decoder_ctx->pix_fmt,
                           decoder_ctx->width,
                           decoder_ctx->height,
                           buffer,
                           size);
    
	av_frame_free(&our_yuv_frame);
    
	return (err >= 0);
}

int nv_avc_get_rgb_frame(char* buffer, int size) {
	return (update_rgb_frame() && render_rgb_to_buffer(buffer, size));
}

int nv_avc_get_input_padding_size(void) {
	return FF_INPUT_BUFFER_PADDING_SIZE;
}

// packets must be decoded in order
// indata must be inlen + FF_INPUT_BUFFER_PADDING_SIZE in length
int nv_avc_decode(unsigned char* indata, int inlen) {
	int err = 0;
	int got_pic = 0;
    
	pkt.data = indata;
	pkt.size = inlen;
    
	while (pkt.size > 0) {
		got_pic = 0;
		err = avcodec_decode_video2(
                                    decoder_ctx,
                                    dec_frame,
                                    &got_pic,
                                    &pkt);
		if (err < 0) {
			//__android_log_write(ANDROID_LOG_ERROR, "NVAVCDEC",
              //                  "Decode failed");
			got_pic = 0;
			break;
		}
        
		pkt.size -= err;
		pkt.data += err;
	}
	
	// Only copy the picture at the end of decoding the packet
	if (got_pic) {
		pthread_mutex_lock(&mutex);
        
		// Only clone this frame if the last frame was taken.
		// This saves on extra copies for frames that don't get
		// rendered.
		if (yuv_frame == NULL) {
			// Clone a new frame
			yuv_frame = av_frame_clone(dec_frame);
		}
        
		pthread_mutex_unlock(&mutex);
	}
    
	return err < 0 ? err : 0;
}



@end
