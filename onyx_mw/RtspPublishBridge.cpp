#include "RtspPublishBridge.h"

static int  modDbgLevel = CJdDbg::LVL_TRACE;

CRtspPublishBridge::CRtspPublishBridge()
{
	mMediaResMgr = new CMediaResMgr();
	mRtspSrv = new CJdRtspSrv(mMediaResMgr);
	mRtspClntRec = new CJdRtspClntRecSession(mMediaResMgr);
	m_pOutputStream = NULL;
	mAggregate = NULL;
	mVideoTrack = NULL;
	mAudioTrack = NULL;
	mRtspPort = 59400;
	m_fEnableRtspSrv = 0;
	m_fEnableAud = 1;
	m_fEnableVid = 1;
}

CRtspPublishBridge::~CRtspPublishBridge()
{
	if(mRtspClntRec) delete mRtspClntRec;
	if(mRtspSrv) delete mRtspSrv;
	if(mMediaResMgr) delete mMediaResMgr;
}

void CRtspPublishBridge::PrepareMediaDelivery(COutputStream *pOutputStream)
{
	//m_pRtspSrv;
	mAggregate = new CMediaAggregate;
	m_pOutputStream = pOutputStream;
	if(m_fEnableVid) {
		mVideoTrack = new CMediaTrack(TRACK_ID_VIDEO, CODEC_NAME_H264, CMediaTrack::CODEC_TYPE_VIDEO, CMediaTrack::CODEC_ID_H264, 59426);
		mVideoTrack->mpAvcCfgRecord = &mAvcCfgRecord;
		mAggregate->AddTrack(mVideoTrack);
	}
	if(m_fEnableAud) {
		mAudioTrack = new CMediaTrack(TRACK_ID_AUDIO, CODEC_NAME_AAC, CMediaTrack::CODEC_TYPE_AUDIO, CMediaTrack::CODEC_ID_AAC, 59428);
		mAggregate->AddTrack(mAudioTrack);
	}
	mMediaResMgr->AddMediaAggregate(pOutputStream->m_szStreamName, mAggregate);
}

int CRtspPublishBridge::StartRtspServer(CRtspSrvConfig *pRtspSvrCfg)
{
	m_fEnableRtspSrv = 1;
	mpRtspSrvCfg = pRtspSvrCfg;
	mRtspSrv->Run(1, mpRtspSrvCfg->usServerRtspPort);
	return 0;
}

void CRtspPublishBridge::RemoveMediaDelivery(COutputStream *pOutputStream)
{
	if(mRtspClntRec) {
		mRtspClntRec->Close();
	}
	if(mRtspSrv) {
		mRtspSrv->Stop(1);
	}
	if(mMediaResMgr) {
		if(mAggregate) {
			if(mVideoTrack) {
				mAggregate->RemoveTrack(mVideoTrack);
				delete mVideoTrack;
			}
			mMediaResMgr->RemoveAggregate(pOutputStream->m_szStreamName);
			delete mAggregate;
		}
	}
}

unsigned char *CRtspPublishBridge::FindStartCode(unsigned char *p, unsigned char *end)
{
    while( p < end - 3) {
        if( p[0] == 0 && p[1] == 0 && ((p[2] == 1) || (p[2] == 0 && p[3] == 1)) )
            return p;
		p++;
    }
    return end;
}

int CRtspPublishBridge::SendVideo(unsigned char *pData, int size, unsigned long lPts)
{
	unsigned char *p = pData;
	unsigned char *end = p + size;
	unsigned char *pNalStart, *pNalEnd;
	int           lNalSize = 0;
	long          lBytesUsed = 0;

	JDBG_LOG(CJdDbg::LVL_STRM,("size=%d", size));
	pNalStart = FindStartCode(p, end);

	while (pNalStart < end) {
		/* Skip Start code */
		while(!*(pNalStart++)) lBytesUsed++;
        lBytesUsed++;

		pNalEnd = FindStartCode(pNalStart, end);
		lNalSize = pNalEnd - pNalStart;
		assert(lNalSize > 0);
		/* Update SPS and PPS */
		if((pNalStart[0] & 0x1F) == 7 /*SPS*/) {
			mAvcCfgRecord.UpdateAvccSps(pNalStart, lNalSize);
		} else if((pNalStart[0] & 0x1F) == 8 /*PPS*/) {
			mAvcCfgRecord.UpdateAvccPps((char *)pNalStart, lNalSize);
		}
		if((pNalStart[0] & 0x1F) >= 20) {
			int InvalidNalType  = 1;
		}
		if(mAvcCfgRecord.mSpsFound) {
			//if((pNalStart[0] & 0x1f) == 5){
			//	mVideoTrack->Deliver((char *)mAvcCfgRecord.mSps, mAvcCfgRecord.mSpsSize, lPts);
			//	mVideoTrack->Deliver((char *)mAvcCfgRecord.mPps, mAvcCfgRecord.mPpsSize, lPts);
			//}
			mVideoTrack->Deliver((char *)pNalStart, lNalSize, lPts);
			lBytesUsed += lNalSize;
		} else {
			// Discard data
			int Discard = 1;
		}
        pNalStart = pNalEnd;
    }
    return lBytesUsed;
}

int CRtspPublishBridge::SendAudio(unsigned char *pData, int sizeData, unsigned long lPts)
{
	int res = 0;
	unsigned char *pFrameStart =  pData;
	unsigned char *pDataEnd = pData + sizeData;
	int nFrameSize = 0;

	if(mAudioTrack == NULL){
		goto Exit;
	}

	mAudioTrack->Deliver((char *)pData, sizeData, lPts);
	res = sizeData;
Exit:
    return res;
}


int CRtspPublishBridge::ConnectToPublishServer(CRtspPublishConfig *pRtspPublishCfg)
{
	int res = -1;
	if(strlen(pRtspPublishCfg->szRtspServerAddr)) {
		if(mRtspClntRec->Open(pRtspPublishCfg->szRtspServerAddr, pRtspPublishCfg->szApplicationName, pRtspPublishCfg->usServerRtspPort) == 0) {
			CMediaAggregate *pMediaAggregate = mRtspClntRec->mpMediaResMgr->GetMediaAggregate(m_pOutputStream->m_szStreamName);
			if(mRtspClntRec->SendAnnounce(m_pOutputStream->m_szStreamName) == 0) {
				if(mRtspClntRec->SendSetup("video",pRtspPublishCfg->usRtpLocalPort,pRtspPublishCfg->usRtpLocalPort+1) == 0){
					mRtspClntRec->SendRecord(m_pOutputStream->m_szStreamName);
					mRtspClntRec->StartPublish(m_pOutputStream->m_szStreamName, TRACK_ID_VIDEO);
					res = 0;
				} else {
					fprintf(stderr, "Failed to setup video port=%d\n",pRtspPublishCfg->usRtpLocalPort );
				}
			} else {
				fprintf(stderr, "Failed to Announce application=%s stream=%s\n", pRtspPublishCfg->szApplicationName,m_pOutputStream->m_szStreamName);
			}
		} else {
			fprintf(stderr, "Failed to connect to %s:%d\n", pRtspPublishCfg->szRtspServerAddr,pRtspPublishCfg->usServerRtspPort);
		}
	}
	return res;
}
