#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define IP_ADDRESS unsigned int

typedef struct _STREAM_CONFIGURATION {
	int width;
	int height;
	int fps;
	int bitrate;
	int packetSize;
	char remoteInputAesKey[16];
	char remoteInputAesIv[16];
} STREAM_CONFIGURATION, *PSTREAM_CONFIGURATION;

typedef struct _LENTRY {
	struct _LENTRY *next;
	char* data;
	int length;
} LENTRY, *PLENTRY;

typedef struct _DECODE_UNIT {
	int fullLength;
	PLENTRY bufferList;
} DECODE_UNIT, *PDECODE_UNIT;

typedef void(*DecoderRendererSetup)(int width, int height, int redrawRate, void* context, int drFlags);
typedef void(*DecoderRendererStart)(void);
typedef void(*DecoderRendererStop)(void);
typedef void(*DecoderRendererRelease)(void);
typedef void(*DecoderRendererSubmitDecodeUnit)(PDECODE_UNIT decodeUnit);

typedef struct _DECODER_RENDERER_CALLBACKS {
	DecoderRendererSetup setup;
	DecoderRendererStart start;
	DecoderRendererStop stop;
	DecoderRendererRelease release;
	DecoderRendererSubmitDecodeUnit submitDecodeUnit;
} DECODER_RENDERER_CALLBACKS, *PDECODER_RENDERER_CALLBACKS;

typedef void(*AudioRendererInit)(void);
typedef void(*AudioRendererStart)(void);
typedef void(*AudioRendererStop)(void);
typedef void(*AudioRendererRelease)(void);
typedef void(*AudioRendererDecodeAndPlaySample)(char* sampleData, int sampleLength);

typedef struct _AUDIO_RENDERER_CALLBACKS {
	AudioRendererInit init;
	AudioRendererStart start;
	AudioRendererStop stop;
	AudioRendererRelease release;
	AudioRendererDecodeAndPlaySample decodeAndPlaySample;
} AUDIO_RENDERER_CALLBACKS, *PAUDIO_RENDERER_CALLBACKS;

// Subject to change in future releases
// Use LiGetStageName() for stable stage names
#define STAGE_NONE 0
#define STAGE_PLATFORM_INIT 1
#define STAGE_RTSP_HANDSHAKE 2
#define STAGE_CONTROL_STREAM_INIT 3
#define STAGE_VIDEO_STREAM_INIT 4
#define STAGE_AUDIO_STREAM_INIT 5
#define STAGE_INPUT_STREAM_INIT 6
#define STAGE_CONTROL_STREAM_START 7
#define STAGE_VIDEO_STREAM_START 8
#define STAGE_AUDIO_STREAM_START 9
#define STAGE_INPUT_STREAM_START 10
#define STAGE_MAX 11

typedef void(*ConnListenerStageStarting)(int stage);
typedef void(*ConnListenerStageComplete)(int stage);
typedef void(*ConnListenerStageFailed)(int stage, int errorCode);

typedef void(*ConnListenerConnectionStarted)(void);
typedef void(*ConnListenerConnectionTerminated)(int errorCode);

typedef void(*ConnListenerDisplayMessage)(char* message);
typedef void(*ConnListenerDisplayTransientMessage)(char* message);

typedef struct _CONNECTION_LISTENER_CALLBACKS {
	ConnListenerStageStarting stageStarting;
	ConnListenerStageComplete stageComplete;
	ConnListenerStageFailed stageFailed;
	ConnListenerConnectionStarted connectionStarted;
	ConnListenerConnectionTerminated connectionTerminated;
	ConnListenerDisplayMessage displayMessage;
	ConnListenerDisplayTransientMessage displayTransientMessage;
} CONNECTION_LISTENER_CALLBACKS, *PCONNECTION_LISTENER_CALLBACKS;

int LiStartConnection(IP_ADDRESS host, PSTREAM_CONFIGURATION streamConfig, PCONNECTION_LISTENER_CALLBACKS clCallbacks,
	PDECODER_RENDERER_CALLBACKS drCallbacks, PAUDIO_RENDERER_CALLBACKS arCallbacks, void* renderContext, int drFlags);
void LiStopConnection(void);
const char* LiGetStageName(int stage);

int LiSendMouseMoveEvent(short deltaX, short deltaY);

#define BUTTON_ACTION_PRESS 0x07
#define BUTTON_ACTION_RELEASE 0x08
#define BUTTON_LEFT 0x01
#define BUTTON_MIDDLE 0x02
#define BUTTON_RIGHT 0x03
int LiSendMouseButtonEvent(char action, int button);

#define KEY_ACTION_DOWN 0x03
#define KEY_ACTION_UP 0x04
#define MODIFIER_SHIFT 0x01
#define MODIFIER_CTRL 0x02
#define MODIFIER_ALT 0x04
int LiSendKeyboardEvent(short keyCode, char keyAction, char modifiers);

#define A_FLAG     0x1000
#define B_FLAG     0x2000
#define X_FLAG     0x4000
#define Y_FLAG     0x8000
#define UP_FLAG    0x0001
#define DOWN_FLAG  0x0002
#define LEFT_FLAG  0x0004
#define RIGHT_FLAG 0x0008
#define LB_FLAG    0x0100
#define RB_FLAG    0x0200
#define PLAY_FLAG  0x0010
#define BACK_FLAG  0x0020
#define LS_CLK_FLAG  0x0040
#define RS_CLK_FLAG  0x0080
#define SPECIAL_FLAG 0x0400
int LiSendControllerEvent(short buttonFlags, char leftTrigger, char rightTrigger,
	short leftStickX, short leftStickY, short rightStickX, short rightStickY);

int LiSendScrollEvent(char scrollClicks);

#ifdef __cplusplus
}
#endif