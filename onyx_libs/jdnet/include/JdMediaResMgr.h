#ifndef __JD_MEDIA_RES_MGR_H__
#define __JD_MEDIA_RES_MGR_H__

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

#define TRACK_ID_VIDEO         "video"
#define TRACK_ID_AUDIO         "audio"
#define TRACK_ID_STREAM        "stream"

#define CODEC_NAME_MP2T        "MP2T"
#define CODEC_NAME_H264        "H264"
#define CODEC_NAME_AAC         "mpeg4-generic"

#define foreach(type,lst) for (std::vector<type>::iterator it=lst.begin();it != lst.end(); it++)
#define foreach_in_map(type,list) for (std::map<std::string,type>::iterator it=list.begin();it != list.end(); it++)
/**
 * Media parsing utilities
 */
class CMediaUtil
{
public:
	static void ParseResourceName(std::string &nameReosurce, std::string &nameAggregate, std::string &nameTrack)
	{
		int nSlashPos = nameReosurce.find("/");
		if(nSlashPos != std::string::npos) {
			nameAggregate.assign(nameReosurce, 0, nSlashPos);
			nameTrack.assign(nameReosurce.c_str(), nSlashPos  + 1, nameReosurce.size() - nSlashPos - 1);
		} else {
			nameAggregate = nameReosurce;
		}
	}
};

class CAvcCfgRecord 
{
#define MAX_SPS_SIZE		256
#define MAX_PPS_SIZE		256

public:
	CAvcCfgRecord()
	{
		mSpsFound = false;
		memset(mSps, 0x00, MAX_SPS_SIZE);
		memset(mPps, 0x00, MAX_SPS_SIZE);
		mSpsSize = mPpsSize = 0;
	}
	unsigned char mVersion;
	unsigned char mProfile;
	unsigned char mProfileCompat;
	unsigned char mLevel; 

	unsigned char	mSpsSize;
	unsigned char	mSps[MAX_SPS_SIZE];
	unsigned char	mPpsSize;
	unsigned char	mPps[MAX_PPS_SIZE];
	bool             mSpsFound;
int UpdateAvccSps(unsigned char *pSpsData, long lSpsSize)
{
	mVersion = 1;
	mProfile = pSpsData[1]; /* profile */

	mProfileCompat = 0; /* profile compat */
	mLevel = pSpsData[3];

	lSpsSize = lSpsSize;
	memcpy( mSps, pSpsData, lSpsSize);
	mSpsSize = lSpsSize;
	mSpsFound = true;
	return 0;
}

int UpdateAvccPps(char *pPpsData, long lPpsSize)
{
	lPpsSize = lPpsSize;
	memcpy( mPps, pPpsData, lPpsSize);
	mPpsSize = lPpsSize;

	return 0;
}
static int ConvertToBase64(unsigned char *pDest, const unsigned char *pSrc, int nLen)
{
    unsigned char    *pTmp;
    int   i;

    pTmp = pDest;
	static const unsigned char Base64Char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (i = 0; i < nLen - 2; i += 3)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
        *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2) |((pSrc[i + 2] & 0xC0) >> 6)];
        *pTmp++ = Base64Char[pSrc[i + 2] & 0x3F];
    }

    /* Convert last 1 or 2 bytes */
    if (i < nLen)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        if (i == (nLen - 1))  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4)];
            *pTmp++ = '=';
        }  else  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
            *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2)];
        }
        *pTmp++     = '=';
    }
    *pTmp++ = '\0';
    return pTmp - pDest;
}
};

class CAacCfgRecord
{
public:
	unsigned char mConfig[2];
void UpdateConfigFromAdts(char *pAdts)
{
    int profileVal      = pAdts[2] >> 6;
    int sampleRateId    = (pAdts[2] >> 2 ) & 0x0f;
    int channelsVal     = ((pAdts[2] & 0x01) << 2) | ((pAdts[3] >> 6) & 0x03);
    mConfig[0]  = (profileVal + 1) << 3 | (sampleRateId >> 1);
    mConfig[1]  = ((sampleRateId & 0x01) << 7) | (channelsVal <<3);

}
};
/**
 * Interface between codec sbsystem and rtsp stack
 */
class IMediaDelivery
{
public:
	virtual ~IMediaDelivery(){};
	virtual int Deliver(char *pData, int nLen, long long llPts) = 0;
};

/**
 * Callback for notifying media events to streaming server
 */
class IServerSessionCB
{
public:
	enum EVENT_T
	{
		EVENT_MEDIA_REMOVED
	};
	virtual void NotifyEvent(int nEventId, void *pInfo) = 0;
};

/**
 * Media Track Callback supplied by media source to receive event notification
 */

class IMediaTrackCB
{
public:
	enum EVENT_T
	{
		EVENT_DELIVERY_ADDED,
		EVENT_DELIVERY_REMOVED
	};

	virtual void NotifyEvent(int nEventId, void *pInfo) = 0;
};

/**
 * Media Track supplies data for an elementary stream in the session
 */

class CMediaTrack
{
public:
	enum CodecType_T
	{
		CODEC_TYPE_AUDIO,
		CODEC_TYPE_VIDEO,
		CODEC_TYPE_STREAM,
		CODEC_TYPE_GENERIC
	};

	enum CodecID_T
	{
		CODEC_ID_MPT2 = 33,
		/* Dynamic Codec IDs */
		CODEC_ID_H264 = 96,
		CODEC_ID_AAC = 97
	};

	CMediaTrack(const char *szTrackName, const char *szCodecName, int nCodecType, int nCodecId, unsigned short usUdpPortBase);
	void SetCallback(IMediaTrackCB *pCallback)
	{
		mCallback = pCallback;
	}

	bool Deliver(char *pData, int Len, long long llpts);
	bool AddDelivery(IMediaDelivery *pDelivery);
	bool DeleteDelivery(IMediaDelivery *pDelivery);
public:
	int                           mCodecType;
	std::string                   mTrackName;

	bool                          mfLive;
	int                           mCodecId;
	std::string                   mCodecName;
	std::vector<IMediaDelivery *> mDeliveries;
	IMediaTrackCB                 *mCallback;
	unsigned short                mUdpPortBase;
	unsigned short                mfMacast;
	std::string                   mMacastAddr;

	unsigned long                 mSsrc;
	CAvcCfgRecord                 *mpAvcCfgRecord;
	CJdMutex                      mMutex;
};

/**
 * Media Aggregate contains one or more tracks
 */
class CMediaAggregate
{
#define med_it std::vector<CMediaTrack *>::iterator
public:
	CMediaAggregate()
	{
	}

	~CMediaAggregate()
	{
		foreach(IServerSessionCB *, mSessions)
			(*it)->NotifyEvent(IServerSessionCB::EVENT_MEDIA_REMOVED ,NULL);
	}

	/* API used by codec subsystem */
	bool AddTrack(CMediaTrack *pTrack)
	{
		mTracks.push_back(pTrack);
		return true;
	}

	int RemoveTrack(CMediaTrack *pTrack)
	{
		foreach(CMediaTrack *, mTracks)
		if ((*it) == pTrack) 	{ 
			mTracks.erase(it); 
			break; 
		} 
		return true;
	}

	CMediaTrack * GetTrack(std::string nameTrack)
	{
		for (int i=0; i < mTracks.size(); i++) {
			CMediaTrack *pTrack = mTracks[i];
			if (pTrack->mTrackName.compare(nameTrack) == 0)
				return pTrack; 
		}
		return NULL;
	}

	/* API used by RTSP Stack */
	int GetTrackCount() 
	{
		return mTracks.size();
	}
	
	CMediaTrack *GetTrack(int nTrack) 
	{
		return mTracks[nTrack];
	}
	void SetCallback(IServerSessionCB *pCallback)
	{
		mSessions.push_back(pCallback);
	}
	void RemoveCallback(IServerSessionCB *pCallback)
	{
		foreach(IServerSessionCB *, mSessions)
		if ((*it) == pCallback) 	{ 
			mSessions.erase(it); 
			break; 
		} 
	}

public:
	std::string                     mResName;
	std::vector<CMediaTrack *>      mTracks;
	std::vector<IServerSessionCB *> mSessions;
};

/**
 * Media Resource Manager manages Media Aggregates
 */
class CMediaResMgr
{
public:
	#define my_it std::map<std::string, CMediaAggregate *>::iterator
	static CMediaResMgr *CreateInstance()
	{
		return new CMediaResMgr;
	}
	CMediaResMgr()
	{

	}
	~CMediaResMgr()
	{
		mMediaAggregates.clear();
	}
	/* API used by codec subsystem */
	bool AddMediaAggregate(const char *resName, CMediaAggregate *mediaAggregate);
	bool RemoveAggregate(const char *resName);

	/* API used by codec RTSP */
	CMediaAggregate * GetMediaAggregate(const char*mediaName)
	{
		my_it it =  mMediaAggregates.find(mediaName);
		if(it !=  mMediaAggregates.end())
			return it->second;
		else
			return NULL;
	}
public:
	std::map<std::string, CMediaAggregate *> mMediaAggregates;
};
#endif // __JD_MEDIA_RES_MGR_H__