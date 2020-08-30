#include <assert.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>
#include <map>
#define read	_read
#else
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <map>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "JdMutex.h"
#include "JdMediaResMgr.h"

#include "JdDbg.h"
static int modDbgLevel = CJdDbg::LVL_SETUP;

CMediaTrack::CMediaTrack(const char *szTrackName, const char *szCodecName, int nCodecType, int nCodecId, unsigned short usUdpPortBase)
{
	mTrackName = szTrackName;
	mUdpPortBase = usUdpPortBase;
	mCodecType = nCodecType;
	mCodecId = nCodecId;
	mCodecName = szCodecName;
	mCallback = NULL;
	mSsrc = 0;
}


bool CMediaTrack::Deliver(char *pData, int Len, long long llpts)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter %d",  mDeliveries.size()));
	mMutex.lock();
	for (int i=0; i < mDeliveries.size(); i++){
		IMediaDelivery *pDelivery = mDeliveries[i];
		pDelivery->Deliver(pData, Len, llpts);
	}
	mMutex.unlock();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return true;
}

bool CMediaTrack::AddDelivery(IMediaDelivery *pDelivery)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	mMutex.lock();
	mDeliveries.push_back(pDelivery);
	if(mCallback)
		mCallback->NotifyEvent(IMediaTrackCB::EVENT_DELIVERY_ADDED, NULL);
	mMutex.unlock();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return true;
}

bool CMediaTrack::DeleteDelivery(IMediaDelivery *pDelivery)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	mMutex.lock();
	foreach(IMediaDelivery *, mDeliveries)
	if ((*it) == pDelivery) 	{ 
		mDeliveries.erase(it); 
		break; 
	} 

	if(mCallback)
		mCallback->NotifyEvent(IMediaTrackCB::EVENT_DELIVERY_REMOVED, NULL);
	mMutex.unlock();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return true;
}

	/* API used by codec subsystem */
	bool CMediaResMgr::AddMediaAggregate(const char *resName, CMediaAggregate *mediaAggregate)
	{
		JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
		JDBG_LOG(CJdDbg::LVL_SETUP,("Added %s",resName));

		mMediaAggregates[resName] = mediaAggregate;

		JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
		return true;
	}

	bool CMediaResMgr::RemoveAggregate(const char *resName)
	{
		JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
		JDBG_LOG(CJdDbg::LVL_SETUP,("Remove %s",resName));
		my_it it =  mMediaAggregates.find(resName);
		mMediaAggregates.erase(it);
		JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
		return true;
	}
