#include "RtmpPublishBridge.h"
#include "JdDbg.h"

#include <assert.h>

static int  modDbgLevel = CJdDbg::LVL_TRACE;

CRtmpPublishBridge::CRtmpPublishBridge()
{
	m_pVidRtmpBuffer = (char *)malloc(1024*1024);
	m_pAudRtmpBuffer = (char *)malloc(256*1024);;

	m_pOutputStream = NULL;
	mRtmpPort = 59400;

	m_fEnableAud = 1;
	m_fEnableVid = 1;
	for (int i = 0; i <MAX_RTMP_SERVERS; i++) {
		m_fRecordServer[i] = false;
	}

}

CRtmpPublishBridge::~CRtmpPublishBridge()
{

}
void CRtmpPublishBridge::SetServerConfig(CRtmpPublishConfig *pRtmpPublishConfig)
{
	m_pRtmpPublishCfg = pRtmpPublishConfig;
}
int CRtmpPublishBridge::PrepareMediaDelivery(COutputStream *pOutputStream)
{
    int rc = -1;
	int nId = 0;
	m_urlRtmpServers[0] = 0;
	char szUrl[256];
	// Clear previous instance, if any
	for (int i = 0; i < m_pRtmpServers.size(); i++) {
		RTMP *pRtmp = m_pRtmpServers[i];
		free(pRtmp);
	}
	m_pRtmpServers.clear();

	// Open Primary server
	if(m_pRtmpPublishCfg->fEnablePrimarySrv){
		RTMP *pRtmp  = (RTMP *)malloc(sizeof(RTMP));
		RTMP_Init(pRtmp);
		sprintf(szUrl, "rtmp://%s:%d/%s/%s", m_pRtmpPublishCfg->szPrimarySrvAddr, 
											m_pRtmpPublishCfg->usPrimarySrvPort, 
											m_pRtmpPublishCfg->szPrimarySrvAppName, 
											m_pRtmpPublishCfg->szPrimarySrvStrmName);
		if (!RTMP_SetupURL(pRtmp, szUrl)) {
			// Error
		} else {
			if(m_fRecordServer[nId]){
				AVal av_OptionRecord = AVC("record");
				AVal av_OptionVal = AVC("true");
				RTMP_SetOpt(pRtmp, &av_OptionRecord, &av_OptionVal);
			}
			RTMP_EnableWrite(pRtmp);

			if (RTMP_Connect(pRtmp, NULL)){
					if(RTMP_ConnectStream(pRtmp, 0)) {
						//RTMP_SetChunkSize(pRtmp, 1400);
						rc = 0;
					}
			}
			m_pRtmpServers.push_back(pRtmp);
			nId++;
		}
	}
	// Open Primary server
	if(m_pRtmpPublishCfg->fEnableSecondarySrv){
		RTMP *pRtmp  = (RTMP *)malloc(sizeof(RTMP));
		RTMP_Init(pRtmp);
		sprintf(szUrl, "rtmp://%s:%d/%s/%s", m_pRtmpPublishCfg->szSecondarySrvAddr, 
											m_pRtmpPublishCfg->usSecondarySrvPort, 
											m_pRtmpPublishCfg->szSecondarySrvAppName, 
											m_pRtmpPublishCfg->szPrimarySrvStrmName);

		if (!RTMP_SetupURL(pRtmp, szUrl)) {
			// Error
		} else {
			if(m_fRecordServer[nId]){
				AVal av_OptionRecord = AVC("record");
				AVal av_OptionVal = AVC("true");
				RTMP_SetOpt(pRtmp, &av_OptionRecord, &av_OptionVal);
			}
			RTMP_EnableWrite(pRtmp);

			if (RTMP_Connect(pRtmp, NULL)){
					if(RTMP_ConnectStream(pRtmp, 0)) {
						//RTMP_SetChunkSize(pRtmp, 1400);
						rc = 0;
					}
			}
			m_pRtmpServers.push_back(pRtmp);
			nId++;
		}
	}
	return rc;
}

unsigned char *CRtmpPublishBridge::FindStartCode(unsigned char *p, unsigned char *end)
{
    while( p < end - 3) {
        if( p[0] == 0 && p[1] == 0 && ((p[2] == 1) || (p[2] == 0 && p[3] == 1)) )
            return p;
		p++;
    }
    return end;
}

int CRtmpPublishBridge::ConnectToPublishServer(CRtspPublishConfig *pRtspPublishCfg)
{
	int res = -1;
	return res;
}


/* offsets for packed values */
#define FLV_AUDIO_SAMPLESSIZE_OFFSET 1
#define FLV_AUDIO_SAMPLERATE_OFFSET  2
#define FLV_AUDIO_CODECID_OFFSET     4
/* bitmasks to isolate specific values */
#define FLV_AUDIO_CHANNEL_MASK    0x01
#define FLV_AUDIO_SAMPLESIZE_MASK 0x02
#define FLV_AUDIO_SAMPLERATE_MASK 0x0c
#define FLV_AUDIO_CODECID_MASK    0xf0

#define FLV_VIDEO_FRAMETYPE_OFFSET   4

enum {
    FLV_CODECID_AAC                  = 10<< FLV_AUDIO_CODECID_OFFSET,
};

enum {
    FLV_SAMPLERATE_SPECIAL = 0, /**< signifies 5512Hz and 8000Hz in the case of NELLYMOSER */
    FLV_SAMPLERATE_11025HZ = 1 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_22050HZ = 2 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_44100HZ = 3 << FLV_AUDIO_SAMPLERATE_OFFSET,
};

enum {
    FLV_MONO   = 0,
    FLV_STEREO = 1,
};

enum {
    FLV_SAMPLESSIZE_8BIT  = 0,
    FLV_SAMPLESSIZE_16BIT = 1 << FLV_AUDIO_SAMPLESSIZE_OFFSET,
};

#define FLV_VIDEO_FRAMETYPE_OFFSET   4
enum {
    FLV_FRAME_KEY        = 1 << FLV_VIDEO_FRAMETYPE_OFFSET,
    FLV_FRAME_INTER      = 2 << FLV_VIDEO_FRAMETYPE_OFFSET,
    FLV_FRAME_DISP_INTER = 3 << FLV_VIDEO_FRAMETYPE_OFFSET,
};

enum {
    FLV_CODECID_H264    = 7
};



#define MAX_TAG_SIZE	256
int CRtmpPublishBridge::SendVideo(
	  unsigned char	*pData, 
	  int			size,
	  unsigned long lPts
	  )
{
    unsigned char *p = pData;
    unsigned char *end = p + size;
    unsigned char *pNalStart, *pNalEnd;
	int           lNalSize = 0;
	long          lBytesUsed = 0;
	int           sizeTag = 0;
	char          Tag[MAX_TAG_SIZE] = {0};
	
	unsigned char flags = FLV_FRAME_INTER; // defualt
	pNalStart = FindStartCode(p, end);

    while (pNalStart < end) {
		/* Skip Start code */
		while(!*(pNalStart++))
			lBytesUsed++;
        lBytesUsed++;

		pNalEnd = FindStartCode(pNalStart, end);
		lNalSize = pNalEnd - pNalStart;
		assert(lNalSize > 0);

		/* Update SPS and PPS */
		if((pNalStart[0] & 0x1F) == 7 /*SPS*/) {
			mAvcCfgRecord.UpdateAvccSps(pNalStart, lNalSize);
			flags = FLV_FRAME_KEY;
		} else if((pNalStart[0] & 0x1F) == 8 /*PPS*/) {
			mAvcCfgRecord.UpdateAvccPps((char *)pNalStart, lNalSize);
		}
		if((pNalStart[0] & 0x1F) >= 20) {
			int InvalidNalType  = 1;
		}
		if(mAvcCfgRecord.mSpsPpsFound) {
			//if((pNalStart[0] & 0x1f) == 5){
			//	mVideoTrack->Deliver((char *)mAvcCfgRecord.mSps, mAvcCfgRecord.mSpsSize, lPts);
			//	mVideoTrack->Deliver((char *)mAvcCfgRecord.mPps, mAvcCfgRecord.mPpsSize, lPts);
			//}
			if(!mAvcCfgRecord.mSentConfig && mAvcCfgRecord.mSpsSize > 0 && mAvcCfgRecord.mPpsSize > 0){
				if(m_ulVidPtsStart == -1)
					m_ulVidPtsStart = lPts;

				Tag[0] = FLV_FRAME_KEY | FLV_CODECID_H264;
				Tag[1] = 0; // Sequence Header
				sizeTag = 5;
				char *pTagBuf =  Tag + sizeTag;
				//ff_isom_write_avcc(pb, enc->extradata, enc->extradata_size);
				*pTagBuf++ = 1; /* version */
				*pTagBuf++ =  mAvcCfgRecord.mProfile; /* profile */
				*pTagBuf++ = mAvcCfgRecord.mProfileCompat; /* profile compat */
				*pTagBuf++ = mAvcCfgRecord.mLevel; /* level */
				*pTagBuf++ = 0xff; /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
				
				*pTagBuf++ = 0xe1; /* 3 bits reserved (111) + 5 bits number of sps (00001) */

				AMF_EncodeInt16(pTagBuf, pTagBuf + 2, mAvcCfgRecord.mSpsSize);
				pTagBuf += 2;
				memcpy(pTagBuf, mAvcCfgRecord.mSps, mAvcCfgRecord.mSpsSize);
				
				pTagBuf += mAvcCfgRecord.mSpsSize;
				*pTagBuf++ = 1; /* number of pps */
				
				AMF_EncodeInt16(pTagBuf ,pTagBuf + 2, mAvcCfgRecord.mPpsSize);
				pTagBuf += 2;
				
				memcpy(pTagBuf, mAvcCfgRecord.mPps, mAvcCfgRecord.mPpsSize);
				pTagBuf += mAvcCfgRecord.mPpsSize;
				
				sizeTag = pTagBuf - Tag;
				
				WriteRtmpPacket(SAMPLE_TYPE_H264, /*lPts -*/ m_ulVidPtsStart, NULL, 0, (const unsigned char *)Tag, sizeTag);
				lBytesUsed += lNalSize;
				mAvcCfgRecord.mSentConfig = true;
			} else {
				if(pNalStart[0] == 0x09) {
					// Skip Access unit
					int skip = 1;
				} else {
					Tag[0] = flags | FLV_CODECID_H264;
					Tag[1] = 1; // Nalu
					Tag[2] = Tag[3] = Tag[4] = 0;	// Composition time offset
					AMF_EncodeInt32(Tag + 5, Tag + 9, lNalSize);
					sizeTag = 9;
					
					JDBG_LOG(CJdDbg::LVL_MSG, ("H264 %d", lNalSize + sizeTag));
					WriteRtmpPacket(SAMPLE_TYPE_H264, lPts /*- m_ulVidPtsStart*/, (unsigned char *)pNalStart, lNalSize, (const unsigned char *)Tag, sizeTag);

					lBytesUsed += lNalSize;
					JDBG_LOG(CJdDbg::LVL_MSG, ("H264 PTS=%d", lPts));
				}
			}
		} else {
			// Discard data
			int Discard = 1;
		}
        pNalStart = pNalEnd;
    }
    return lBytesUsed;
}

/*
** if B[0..2] == "FLV" 3byte header
** 1 Byte : m_packetType
** 3 Bytes body size
** 4 Bytes Timestamp
** 3 ??
*/
int CRtmpPublishBridge::WriteRtmpPacket(
		unsigned char       nStrmType, 
		unsigned long ulTimeStamp, 
		const unsigned char *pData, 
		int           sizeData,
		const unsigned char *pCodecTag,
		int           sizeTag
		)
{
	char *buff = NULL;
	char *buffStart = NULL;


	/* !!!Header required by librtmp. librtmp translates it to flv tag !!!*/
	if(nStrmType == SAMPLE_TYPE_H264){
		buffStart = buff = m_pVidRtmpBuffer;
		*buff = 0x09;
		buff++;
	} else {
		buffStart = buff = m_pAudRtmpBuffer;
		*buff = 0x08; 
		buff++;
	}
	AMF_EncodeInt24(buff, buff + 3, sizeData + sizeTag);
	buff += 3;
	AMF_EncodeInt24(buff, buff + 3, ulTimeStamp & 0x00FFFFFF);
	buff += 3;
	*buff = (ulTimeStamp >> 24) & 0x000000FF;
	buff++;
	//??
	buff += 3;

	/* Payload tag header and payload. Not translated by librtmp*/
	if(pCodecTag && sizeTag > 0) {
		memcpy(buff, pCodecTag, sizeTag);
		buff += sizeTag;
	}
	if(pData &&  sizeData > 0) {
		memcpy(buff, pData, sizeData);
	}
	for (int i = 0; i < m_pRtmpServers.size(); i++) {
		RTMP  *pRtmp = m_pRtmpServers[i];
		if(pRtmp && pRtmp->m_bPlaying){
			RTMP_Write(pRtmp, buffStart, sizeData + sizeTag + 11);
		} 
	}
	return 0;
}


unsigned short getAdtsFrameLen(unsigned char *pAdts)
{
	unsigned short usLen = (((unsigned short)pAdts[3] << 11) & 0x1800) |
							(((unsigned short)pAdts[4] << 3) & 0x07F8) |
							(((unsigned short)pAdts[5] >> 5) & 0x0007);
	return usLen;
}

int CRtmpPublishBridge::SendAudio(	  
		unsigned char	*pData, 
		int			    sizeData,
		unsigned long   lPts
		)
{
	// TODO AAC Config
	int           sizeTag = 0;
	char          Tag[MAX_TAG_SIZE] = {0};

	if(!mAacConfig.mSentConfig) {
		mAacConfig.UpdateConfigFromAdts((char *)pData);
		if(mAacConfig.mInit) {
			Tag[0] = FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
			Tag[1] = 0; // AAC AudioSpecificConfig
			memcpy(Tag + 2, mAacConfig.mConfig, 2);
			sizeTag = 4;
			WriteRtmpPacket(SAMPLE_TYPE_AAC, lPts, NULL, 0, (unsigned char *)Tag, sizeTag);
			mAacConfig.mSentConfig = true;
		}
	};
	if(mAacConfig.mInit) {

		unsigned char *pFrameStart =  pData;
		unsigned char *pDataEnd = pData + sizeData;
		int nFrameSize = 0;

		unsigned long lFramePts = lPts;

		Tag[0] = FLV_CODECID_AAC | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
		Tag[1] = 1; // AAC Raw
		sizeTag = 2;
		while ((pFrameStart + 7) < pDataEnd) {
			nFrameSize = getAdtsFrameLen(pFrameStart);
			int nAdtsHdrSize = 7 + (( pFrameStart[1] & 0x01 ) ? 0 : 2 );
			unsigned char *pRawData = pFrameStart + nAdtsHdrSize;
			int nRawDataSize = nFrameSize - nAdtsHdrSize;

			WriteRtmpPacket(SAMPLE_TYPE_AAC, lFramePts, (unsigned char *)pRawData, nRawDataSize, (unsigned char *)Tag, sizeTag);

			lFramePts += (1000 * 1024 / 44100);	// AAC frame duration in ms
			pFrameStart += nFrameSize;
			JDBG_LOG(CJdDbg::LVL_MSG, ("AAC PTS=%d", lFramePts));
		}
	}
	return 0;
}
