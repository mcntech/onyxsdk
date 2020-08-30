// OpenMAX AL MediaPlayer command-line player
#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#endif
#include <assert.h>
#include <string.h>

#include "OpenMAXAL/OpenMAXAL.h"
#include  "onyx_omxext_api.h"
#include "OmxIf.h"
#include "minini.h"
#include "uimsg.h"
#include "JdDbg.h"

#ifdef NO_DEBUG_LOG
#define DBG_PRINT(...)
#else
#define DBG_PRINT(...)                                                \
          if (1)                                                      \
          {                                                           \
            printf(__VA_ARGS__);                                      \
          }
#endif

#define LIBCODEC              "/usr/lib/libcodecdm81xx.so.5.2"

static int  modDbgLevel = CJdDbg::LVL_TRACE;
void*        gOMXHandle=NULL;

XAVideoStreamInformation vid_input_param = {XA_VIDEOCODEC_AVC, 1280, 720, 60, 0, 0};
XAAudioStreamInformation aud_input_param = {XA_AUDIOCODEC_AAC,0};


typedef XAresult(*xaCreateEngine_t)(XAObjectItf *pEngine,
                          XAuint32 numOptions,
                          const XAEngineOption *pEngineOptions,
                          XAuint32 numInterfaces,
                          const XAInterfaceID *pInterfaceIds,
                          const XAboolean *pInterfaceRequired);

xaCreateEngine_t g_xaCreateEngine;

// playback event callback
void playEventCallback(XAPlayItf caller, void *pContext, XAuint32 event)
{
    if (XA_PLAYEVENT_HEADATEND & event) {
        DBG_PRINT("omxal_test:playEventCallback:XA_PLAYEVENT_HEADATEND\n");
    } 

	if (XA_PLAYEVENT_HEADSTALLED & event) {
        DBG_PRINT("omxal_test:playEventCallback:XA_PLAYEVENT_HEADSTALLED\n");
    }

	if (XA_PLAYEVENT_HEADATNEWPOS & event) {
        DBG_PRINT("omxal_test:playEventCallback:XA_PLAYEVENT_HEADATNEWPOS\n");
    }

}

void StopPlayback(XAPlayItf playerPlay, int nTimeOutMs)
{
	XAuint32  status;
	XAuint32  result;	
	if(playerPlay) {
		result = (*playerPlay)->GetPlayState(playerPlay, &status);
		if(status != XA_PLAYSTATE_STOPPED) {
			JDBG_LOG(CJdDbg::LVL_TRACE,("SetPlayState:XA_PLAYSTATE_STOPPED"));
			
			(*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_STOPPED);

			while (nTimeOutMs > 0) {
				result= (*playerPlay)->GetPlayState(playerPlay, &status);
				assert(XA_RESULT_SUCCESS == result);
				if (status == XA_PLAYSTATE_STOPPED)
					break;
				#ifdef WIN32
					Sleep(100);
				#else
					usleep(100000);
				#endif
				nTimeOutMs -= 100;
			}
			JDBG_LOG(CJdDbg::LVL_TRACE,("StopPlayback:Stopped"));
		}
	}
}

int omxalPlayStream(ENGINE_T *pEngine, PLAYER_SESSION_T *pSession, XADataSource *pDataSrc1, XADataSource *pDataSrc2)
{
	XAObjectItf           engineObject = pEngine->engineObject;
	XAEngineItf           engineEngine = pEngine->engineEngine;
    XAObjectItf           playerObject;
    XAPlayItf             playerPlay;
    XAConfigExtensionsItf playerExt;
	int res = -1;
	XAresult result;
	XADataLocator_URI locUri;
	XADataFormat_MIME fmtMime;

	XAboolean req[4] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_FALSE, XA_BOOLEAN_TRUE};
	XAInterfaceID ids[3] = {XA_IID_STREAMINFORMATION, XA_IID_PREFETCHSTATUS, XA_IID_SEEK};
	
	if(pDataSrc1 == NULL || pDataSrc1->pLocator == NULL) {
		goto Exit;
	}
	vid_input_param.width = pSession->nWidth;
	vid_input_param.height = pSession->nHeight;


	result = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObject,pDataSrc1, pDataSrc2,
			NULL/*&audioSnk*/, NULL/*nativeWindow != NULL ? &imageVideoSink : NULL*/, NULL, NULL, 3, ids, req);

    assert(XA_RESULT_SUCCESS == result);

    // realize the player
    result = (*playerObject)->Realize(playerObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

    // get the play interface
    result = (*playerObject)->GetInterface(playerObject, XA_IID_PLAY, &playerPlay);
    assert(XA_RESULT_SUCCESS == result);

	result = (*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_PAUSED);
	assert(XA_RESULT_SUCCESS == result);

    // get the player extension interface
    result = (*playerObject)->GetInterface(playerObject, XA_IID_CONFIGEXTENSION, &playerExt);
    assert(XA_RESULT_SUCCESS == result);

	if(strcmp(pSession->vid_codec_name, "mpeg2") == 0) {
		vid_input_param.codecId = XA_VIDEOCODEC_MPEG2;
	} else 	if(strcmp(pSession->vid_codec_name, "h264") == 0) {
		vid_input_param.codecId = XA_VIDEOCODEC_AVC;
	} else 	if(strcmp(pSession->vid_codec_name, "null") == 0) {
		vid_input_param.codecId = 0;
	}

	
	result = (*playerExt)->SetConfiguration(playerExt, SESSION_ID, sizeof(int), &pSession->nSessionId);
	assert(XA_RESULT_SUCCESS == result);

	result = (*playerExt)->SetConfiguration(playerExt, VID_INPUT_PARAM, sizeof(vid_input_param), &vid_input_param);
	assert(XA_RESULT_SUCCESS == result);

	result = (*playerExt)->SetConfiguration(playerExt, LATENCY, sizeof(int), &pSession->nLatency);
	assert(XA_RESULT_SUCCESS == result);

	result = (*playerExt)->SetConfiguration(playerExt, DEINTERLACE, sizeof(int), &pSession->nDeinterlace);
	assert(XA_RESULT_SUCCESS == result);

	result = (*playerExt)->SetConfiguration(playerExt, DEMUX_SELECT_PROG, sizeof(int), &pSession->nSelectProg);
	assert(XA_RESULT_SUCCESS == result);

	if(pSession->nFrameRate > 0) {
		result = (*playerExt)->SetConfiguration(playerExt, FRAMERATE, sizeof(int), &pSession->nFrameRate);
		assert(XA_RESULT_SUCCESS == result);
	}

	if(strcmp(pSession->aud_codec_name, "aaclc") == 0) {
		aud_input_param.codecId = XA_AUDIOCODEC_AAC;
		aud_input_param.sampleRate = pSession->nSampleRate;
		result = (*playerExt)->SetConfiguration(playerExt, AUD_INPUT_PARAM, sizeof(aud_input_param), &aud_input_param);
		assert(XA_RESULT_SUCCESS == result);
	} else if(strcmp(pSession->aud_codec_name, "ac3") == 0) {
		aud_input_param.codecId = 10;//XA_AUDIOCODEC_AAC;
		aud_input_param.sampleRate = pSession->nSampleRate;
		result = (*playerExt)->SetConfiguration(playerExt, AUD_INPUT_PARAM, sizeof(aud_input_param), &aud_input_param);
		assert(XA_RESULT_SUCCESS == result);
	} else if(strcmp(pSession->aud_codec_name, "g711u") == 0) {
		aud_input_param.codecId = XA_AUDIOCODEC_PCM;
		aud_input_param.sampleRate = pSession->nSampleRate;
		result = (*playerExt)->SetConfiguration(playerExt, AUD_INPUT_PARAM, sizeof(aud_input_param), &aud_input_param);
		assert(XA_RESULT_SUCCESS == result);
	} else if(strcmp(pSession->aud_codec_name, "pcm") == 0) {
		aud_input_param.codecId = XA_AUDIOCODEC_PCM;
		aud_input_param.sampleRate = pSession->nSampleRate;
		result = (*playerExt)->SetConfiguration(playerExt, AUD_INPUT_PARAM, sizeof(aud_input_param), &aud_input_param);
		if(XA_RESULT_SUCCESS != result) {
			goto Exit;
		}

	}  else /*if(strcmp(pSession->aud_codec_name, "null") == 0)*/ {
		aud_input_param.codecId = 0;
		result = (*playerExt)->SetConfiguration(playerExt, AUD_INPUT_PARAM, sizeof(aud_input_param), &aud_input_param);
		if(XA_RESULT_SUCCESS != result) {
			goto Exit;
		}
	}

    // register play event callback
    result = (*playerPlay)->RegisterCallback(playerPlay, playEventCallback, NULL);
	if(XA_RESULT_SUCCESS != result) {
		goto Exit;
	}
    result = (*playerPlay)->SetCallbackEventsMask(playerPlay,
            XA_PLAYEVENT_HEADATEND | XA_PLAYEVENT_HEADATMARKER | XA_PLAYEVENT_HEADATNEWPOS);
	if(XA_RESULT_SUCCESS != result) {
		goto Exit;
	}

    result = (*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_PAUSED);
	if(XA_RESULT_SUCCESS != result) {
		goto Exit;
	}

    result = (*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_PLAYING);
	if(XA_RESULT_SUCCESS != result) {
		goto Exit;
	}

	pSession->playerObject = playerObject;
	pSession->playerPlay = playerPlay;
	pSession->playerExt = playerExt;
	res = 0;
Exit:
	return res;
}


void omxalStopStream(PLAYER_SESSION_T *pSession)
{

	JDBG_LOG(CJdDbg::LVL_TRACE,("StopPlayback"));
	StopPlayback(pSession->playerPlay, 3000);

    //sleep(3);

destroyRes:
    // destroy the player
	if(pSession->playerObject) {
		JDBG_LOG(CJdDbg::LVL_TRACE,(">Destroy PlayerObject"));
		(*pSession->playerObject)->Destroy(pSession->playerObject);
	}
}


void FreeEngineOption(XAEngineOption *_pEngOption, int nNumOptions)
{
	int i;
	XAEngineOption *pEngOption = _pEngOption;
	for (i=0; i < nNumOptions; i++){
		DISP_WINDOW_LIST *pList = (DISP_WINDOW_LIST *)pEngOption->data;
		free(pList->pWndList);
		free(pList);
		pEngOption++;
	}
	free(_pEngOption);
}

MIXER_SESSION_T *CreateOutputMixer(ENGINE_T *pEngine)
{
	XAObjectItf           engineObject = pEngine->engineObject;
	XAEngineItf           engineEngine = pEngine->engineEngine;
	XAObjectItf           mixerObject;
	XAresult result;

	MIXER_SESSION_T *pSession = (MIXER_SESSION_T *)malloc(sizeof(MIXER_SESSION_T));
	memset(pSession, 0x00, sizeof(MIXER_SESSION_T));
	XAboolean req[4] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_FALSE, XA_BOOLEAN_TRUE};
	XAInterfaceID ids[3] = {XA_IID_STREAMINFORMATION, XA_IID_PREFETCHSTATUS, XA_IID_SEEK};

    result = (*engineEngine)->CreateOutputMix(engineEngine, &mixerObject,  3, ids, req);
	pSession->mixerObject= mixerObject;
    assert(XA_RESULT_SUCCESS == result);
	return pSession;
}



int omxalCreateRecorder(ENGINE_T *pEngine, void *vidDataSnk, void *audDataSnk)
{
	XAObjectItf           engineObject = pEngine->engineObject;
	XAEngineItf           engineEngine = pEngine->engineEngine;
	RECORD_SINK_T         RecSink;
	XAObjectItf           RecorderItf;
	XAresult result;

	XAboolean req[4] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_FALSE, XA_BOOLEAN_TRUE};
	XAInterfaceID ids[3] = {XA_IID_STREAMINFORMATION, XA_IID_PREFETCHSTATUS, XA_IID_SEEK};

	RecSink.pAudSink = audDataSnk;
	RecSink.pVidSink = vidDataSnk;
    result = (*engineEngine)->CreateMediaRecorder(engineEngine, &RecorderItf, NULL, NULL,
            (XADataSink*)&RecSink,  3, ids, req);
    assert(XA_RESULT_SUCCESS == result);
	pEngine->recorderObject = RecorderItf;
	return 0;
}

ENGINE_T *omxalInit(const char *pszConfFile, DISP_WINDOW_LIST *pWndList, AUD_CHAN_LIST *pAChanList)
{
	XAresult       result;

	XAObjectItf      engineObject;
	XAEngineItf      engineEngine;
	XAEngineOption   *listEngineOptions = NULL;
	int              nNumOptions;

	int fEnableVideoMixer = ini_getl(GLOBAL_SECTION, KEY_ENABLE_VIDEO_MIXER, 1, pszConfFile);

	if(pAChanList)
		nNumOptions = 2;
	else
		nNumOptions = 1;
	ENGINE_T *pEngine = (ENGINE_T *)malloc(sizeof(ENGINE_T));
	memset(pEngine, 0x00, sizeof(ENGINE_T));
	
	if(nNumOptions)	{
		listEngineOptions = (XAEngineOption *)malloc(nNumOptions * sizeof(XAEngineOption));
		XAEngineOption *pEngineOptionVid = listEngineOptions;


		pEngineOptionVid->data = (XAuint32)pWndList;
		pEngineOptionVid->feature = XAEXT_ENGINEOPTION_SETLAYOUT;

		if(pAChanList) {
			XAEngineOption *pEngineOptionAud = listEngineOptions + 1;
			pEngineOptionAud->data = (XAuint32)pAChanList;
			pEngineOptionAud->feature = XAEXT_ENGINEOPTION_SET_AUD_LAYOUT;
		}
	}

#ifndef WIN32
	char szLibraryName[256];
	ini_gets(GLOBAL_SECTION, KEY_LIBRARY_NAME, LIBCODEC, szLibraryName, 256, pszConfFile);
	const unsigned int flags=RTLD_GLOBAL|RTLD_LAZY;
    gOMXHandle=dlopen(szLibraryName,flags);
    if(gOMXHandle==NULL) {
        DBG_PRINT("Error loading %s: %s""\n", szLibraryName, dlerror());
        exit(1);
    }
    g_xaCreateEngine=(xaCreateEngine_t)dlsym(gOMXHandle,"xaCreateEngine");
#endif

	// create engine
#ifdef WIN32
	result = xaCreateEngine(&engineObject, nNumOptions, listEngineOptions, 0, NULL, NULL);
#else
	result = g_xaCreateEngine(&engineObject, nNumOptions, listEngineOptions, 0, NULL, NULL);
#endif

	FreeEngineOption(listEngineOptions, nNumOptions);

	assert(XA_RESULT_SUCCESS == result);
	result = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
	assert(XA_RESULT_SUCCESS == result);

	result = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
	assert(XA_RESULT_SUCCESS == result);

	pEngine->engineObject = engineObject;
	pEngine->engineEngine = engineEngine;

	if(fEnableVideoMixer) {
		pEngine->pMixerSession = CreateOutputMixer(pEngine);
	}

	return pEngine;
}


void omxalDeinit(ENGINE_T *pEngine)
{
    // destroy the engine
    (*pEngine->engineObject)->Destroy(pEngine->engineObject);
	if(pEngine->pMixerSession)
		free(pEngine->pMixerSession);
#ifndef WIN32
    //disposeNativeWindow();
	dlclose(gOMXHandle);
#endif
}
