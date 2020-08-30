#ifndef __STRM_IN_BRIDGE_BASE_H__
#define __STRM_IN_BRIDGE_BASE_H__

#include "OpenMAXAL/OpenMAXAL.h"
#define MAX_NAME_SIZE	256
class CStrmInBridgeBase
{
public:
	CStrmInBridgeBase(int fEnableAud, int fEnableVid);
	~CStrmInBridgeBase();
	XADataSource *GetDataSource1(){return &mDataSrcVideo;}
	XADataSource *GetDataSource2(){return &mDataSrcAudio;}
    virtual int StartStreaming(void) = 0;
    virtual int StopStreaming(void) = 0;

protected:
	
	int CreateH264OutputPin();
	int CreateMP4AOutputPin();
	int CreatePCMUOutputPin();

public:
	int                   m_nUiCmd;
	int                   m_fRun;
	int                   m_fEnableAud;
	int                   m_fEnableVid;

	long                  m_lWidth;
	long                  m_lHeight;


	XADataSource          mDataSrcVideo;
	XADataLocator_Address mDataLocatorVideo;
	XADataFormat_MIME     mFormatVideo;

	XADataSource          mDataSrcAudio;
	XADataLocator_Address mDataLocatorAudio;
	XADataFormat_MIME     mFormatAudio;
};
#endif //__STRM_IN_BRIDGE_BASE_H__