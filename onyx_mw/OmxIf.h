#ifndef __OMX_IF_H__
#define __OMX_IF_H__

#include "OpenMAXAL/OpenMAXAL.h"
#include  "onyx_omxext_api.h"
#include "StrmInBridgeBase.h"

typedef enum _INPUT_TYPE_T {
	INPUT_TYPE_UNKNOWN,
	INPUT_TYPE_FILE,
	INPUT_TYPE_RTSP,
	INPUT_TYPE_HLS,
	INPUT_TYPE_CAPTURE,
} INPUT_TYPE_T;


typedef struct PLAYER_SESSION_T
{
	int  nSessionId;
	int nCmd;
	int nPort;

	char input_file_name[128];
	char vid_codec_name[128];
	char aud_codec_name[128];
	INPUT_TYPE_T nInputType;
	int  nSampleRate;
	int  nFrameRate;
	int  nLatency;
	int  nWidth;
	int  nHeight;
	int  nDeinterlace;
	int  nPcrPid;
	int  nAudPid;
	int  nVidPid;
	int  nSelectProg;

	int  fEnableAud;
	int  fEnableVid;
    XAObjectItf           playerObject;
    XAPlayItf             playerPlay;
    XAConfigExtensionsItf playerExt;

#ifdef WIN32
	HANDLE    thrdHandle;
#else
	pthread_t   thrdHandle;
#endif

	CStrmInBridgeBase *mpInStream;
} PLAYER_SESSION_T;


typedef struct MIXER_SESSION_T
{
	int  nSessionId;
	int nCmd;
	int nPort;

	int  nSampleRate;
	int  nFrameRate;
	int  nWidth;
	int  nHeight;

	XAObjectItf        mixerObject;
} MIXER_SESSION_T;

typedef struct ENGINE_T
{
	int                   nSessionId;
	XAObjectItf           engineObject;
	XAEngineItf           engineEngine;
	
	XAObjectItf           recorderObject;	
	MIXER_SESSION_T       *pMixerSession;
} ENGINE_T;

ENGINE_T *omxalInit(const char *pszConfFile,  DISP_WINDOW_LIST *pWndList, AUD_CHAN_LIST *pAChanList);
void omxalDeinit(ENGINE_T *pSession);

int omxalCreateRecorder(ENGINE_T *pEngine, void *vidDataSnk, void *audDataSnk);

int omxalPlayStream(ENGINE_T *pEngine, PLAYER_SESSION_T *pSession, XADataSource *pDataSrc1, XADataSource *pDataSrc2);
void omxalStopStream(PLAYER_SESSION_T *pSession);

#endif // __OMX_IF_H__