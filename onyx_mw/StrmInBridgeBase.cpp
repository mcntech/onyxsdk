#ifdef WIN32
//#include <Windows.h>
#include <winsock2.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "string.h"

#include "JdDbg.h"
#include "strmconn.h"
#include "h264parser.h"
#include "StrmInBridgeBase.h"


#define MAX_VID_FRAME_SIZE      (1920 * 1080)
#define MAX_AUD_FRAME_SIZE      (8 * 1024)



CStrmInBridgeBase::CStrmInBridgeBase(int fEnableAud, int fEnableVid)
{
	mDataSrcAudio.pLocator = NULL;
	mDataSrcAudio.pFormat = NULL;
	mDataSrcVideo.pLocator = NULL;
	mDataSrcVideo.pFormat = NULL;
	m_fEnableAud = fEnableAud;
	m_fEnableVid = fEnableVid;
}


CStrmInBridgeBase::~CStrmInBridgeBase()
{

}

int CStrmInBridgeBase::CreateH264OutputPin()
{
	int hr = 0;
//    CSourceStream *pStreamV = new CH264Stream(&hr, this, L"H264 AnnexB Source!");
	// TODO: Prepare mDataSrcVideo
	mDataLocatorVideo.locatorType = XA_DATALOCATOR_ADDRESS;
	mDataLocatorVideo.pAddress = CreateStrmConn(MAX_VID_FRAME_SIZE, 4);
	mDataSrcVideo.pLocator = &mDataLocatorVideo;
	mDataSrcVideo.pFormat = NULL;
	return hr;
}

int CStrmInBridgeBase::CreateMP4AOutputPin()
{
	int hr = 0;
//    CSourceStream *pStreamA = new CTsStream(&hr, this, L"AAC Source!");
//	if(*phr == S_OK) {
//		if(*phr == S_OK) {
//			m_pQ2nbuff = (unsigned char *)CoTaskMemAlloc(MAX_DEMUX_BUFF);
//			m_pDemux = new CQbox2Nalu(m_pQ2nbuff, MAX_DEMUX_BUFF);
//		}
//	}
	// TODO: Prepare mDataSrcAudio
	mDataLocatorAudio.locatorType = XA_DATALOCATOR_ADDRESS;
	mDataLocatorAudio.pAddress = CreateStrmConn(MAX_AUD_FRAME_SIZE, 4);
	mDataSrcAudio.pLocator = &mDataLocatorAudio;
	mDataSrcAudio.pFormat = NULL;
	return hr;
}
int CStrmInBridgeBase::CreatePCMUOutputPin()
{
	int hr = 0;
	mDataLocatorAudio.locatorType = XA_DATALOCATOR_ADDRESS;
	mDataLocatorAudio.pAddress = CreateStrmConn(MAX_AUD_FRAME_SIZE, 4);
	mDataSrcAudio.pLocator = &mDataLocatorAudio;
	mDataSrcAudio.pFormat = &mFormatAudio;

	return hr;
}


