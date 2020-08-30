#include <stdlib.h>

#ifdef WIN32
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <list>
#include <map>
#include "TsDemuxIf.h"
//#include "Dbg.h"

#ifdef DBG_LOG
#define DbgOut(DbgLevel, x) do { if(DbgLevel <= gDbgLevel) { \
									fprintf(stderr,"%s:%s:%d:", __FILE__, __FUNCTION__, __LINE__); \
									dbg_printf x;                                                  \
									fprintf(stderr,"\n");                                          \
								}                                                                  \
							} while(0);
#else
#define DbgOut(DbgLevel, x)
#endif


#define BYTE_OFFSET(pb,i)           (& BYTE_VALUE((pb),i))
#define BYTE_VALUE(pb,i)            (pb[i])
#define WORD_VALUE(pb,i)            (((unsigned short)(pb[i] << 8) & 0xFF00)  | ((unsigned short)pb[i+1] & 0x00FF))
#define DWORD_VALUE(pb,i)           (((unsigned long)(pb[i] << 24) & 0xFF000000) | ((unsigned long)(pb[i+1] << 16) & 0x00FF0000) | ((unsigned long)(pb[i+2] << 8) & 0x0000FF00) | ((unsigned long)(pb[i+3]) & 0x000000FF))
#define ULONGLONG_VALUE(pb,i)       (*(unsigned long long *) BYTE_OFFSET((pb),i))

#define START_CODE_PREFIX_VALUE(pb)  (DWORD_VALUE(pb,0) >> 8)
#define PACKET_LENGTH_VALUE(pb)      (WORD_VALUE(pb,4))
#define PES_PTS_DTS_FLAGS_VALUE(pb)  ((BYTE_VALUE(pb,1) & 0xc0) >> 6)
#define PES_HEADER_LENGTH_VALUE(pb)  BYTE_VALUE(pb,2)

#define PES_PTS_VALUE_LO(pb)         (                                         \
									 ((((unsigned long)pb[0] & 0x00000006) >> 1) << 30) |   \
									 ((( unsigned long)pb[1] & 0x000000FF      ) << 22) |   \
									 ((((unsigned long)pb[2] & 0x000000FE) >> 1) << 15) |   \
									 ((( unsigned long)pb[3] & 0x000000FF      ) << 7 ) |   \
									 ((( unsigned long)pb[4] &  0x000000FE  >> 1))          \
 									 ) 

#define PES_PTS_VALUE_HI(pb)         ((unsigned long)(pb[0] >> 3) & 0x00000001)

#define PES_DTS_VALUE_LO(pb)         PES_PTS_VALUE_LO(pb)
#define PES_DTS_VALUE_HI(pb)         PES_PTS_VALUE_HI(pb)

#ifndef BIG_ENDIAN
#define NTOH_LL(ll)                                             \
                    ( (((ll) & 0xFF00000000000000) >> 56)  |    \
                      (((ll) & 0x00FF000000000000) >> 40)  |    \
                      (((ll) & 0x0000FF0000000000) >> 24)  |    \
                      (((ll) & 0x000000FF00000000) >> 8)   |    \
                      (((ll) & 0x00000000FF000000) << 8)   |    \
                      (((ll) & 0x0000000000FF0000) << 24)  |    \
                      (((ll) & 0x000000000000FF00) << 40)  |    \
                      (((ll) & 0x00000000000000FF) << 56) )
#else   //  BIG_ENDIAN
#define NTOH_LL(ll)     (ll)
#endif
#define ADAPTATION_FIELD_CONTROL_VALUE(pb)  ((BYTE_VALUE((pb),3) & 0x00000030) >> 4)
#define PCR_FLAG_BIT(pb)                    ((BYTE_VALUE((pb),1) & 0x00000010) >> 4)
#define PCR_BASE_VALUE(pb)                  (NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 31)
#define PCR_EXT_VALUE(pb)                   ((unsigned long) (NTOH_LL(ULONGLONG_VALUE(pb,0)) & 0x00000000000001ff))

CTsbuffer::CTsbuffer(int nMaxSize)
{
	mData= (char *)malloc(nMaxSize);
	mMaxSize = nMaxSize;
	mRdOffset = 0;
	mUsedSize = 0;
}


CTsbuffer::~CTsbuffer()
{
	if(mData)
		free(mData);
}


CAvFrame::CAvFrame(int nMaxDataSize, int nPid)
{
	mFrmData = (char *)malloc(nMaxDataSize);
	mMaxSize = nMaxDataSize;
	mUsedSize = 0;
	mTsPid = nPid;
	mFrameState = FRAME_STATE_INIT;
	mPts = 0;
}
CAvFrame::~CAvFrame()
{
	free(mFrmData);
}

class CVPktParser : public CPktParser
{
public:
	CVPktParser():CPktParser(){};
	~CVPktParser()
	{
		if(mAvFrame)
			delete mAvFrame;
	};
	int Process(unsigned char *pTsData);
};


class CAPktParser : public CPktParser
{
public:
	CAPktParser():CPktParser()
	{
	};
	~CAPktParser(){};
	int Process(unsigned char *pTsData);
};

class CPcrParser : public CPktParser
{
public:
	CPcrParser():CPktParser()
	{
		mllPcr = 0;
		mlPcrExt = 0;
	};
	~CPcrParser(){};
	int Process(unsigned char *pTsData);
	long long  mllPcr;
	long  mlPcrExt;
};

int CVPktParser::Process(unsigned char *_pData)
{
	int nAdaptLen = 0;
	int lDataLen = TS_PKT_LEN;
	unsigned char *pData = _pData;
	CAvFrame  *pFrame = mAvFrame;

	// Skip Adaptation pkt
	if ((0x20 & pData[3]) == 0x20) {
		// Length Byte + Length
		nAdaptLen += pData[4] + 1;
	}
	
	pData += (nAdaptLen + 4);
	/* packet start code prefix and stream id */
	unsigned long ulPrefix = START_CODE_PREFIX_VALUE(pData);
	if(START_CODE_PREFIX_VALUE(pData) == 0x000001 && pData[3] == 0xE0) {
		/* Cpmplete the previous frame */
		if(pFrame->mUsedSize) {
			pFrame->mFrameState = FRAME_STATE_COMPLETE;
			return 0;
		}
		pFrame->mFrameState = FRAME_STATE_FILLING;
		m_lPesLen = PACKET_LENGTH_VALUE(pData);

		/* Move the Data pointer to start of PES HEADER */
		pData += 6;

		if((PES_PTS_DTS_FLAGS_VALUE(pData)& 0x02)){
			unsigned char *pb = pData+3;
			pFrame->mPts = (long long)PES_PTS_VALUE_HI(pb) << 32 | PES_PTS_VALUE_LO(pb);

		}
		if((PES_PTS_DTS_FLAGS_VALUE(pData)& 0x01)){
			unsigned char *pb = pData+8;
			pFrame->mDts = PES_PTS_VALUE_HI(pb) | PES_PTS_VALUE_LO(pb);
		}
		long lPesHdrLen = PES_HEADER_LENGTH_VALUE(pData);
		pData += lPesHdrLen + 3;
	}
	if(pFrame->mFrameState == FRAME_STATE_FILLING){
		lDataLen = 	lDataLen - (pData - _pData);
		if(pFrame->mUsedSize < pFrame->mMaxSize - lDataLen){
			memcpy(pFrame->mFrmData + pFrame->mUsedSize, pData, lDataLen);
			pFrame->mUsedSize += lDataLen;
		} else {
			//TODO: reallocate buffer and copy
		}
	}
	return 0;
}

int CAPktParser::Process(unsigned char *_pData)
{
	int nAdaptLen = 0;
	int lDataLen = TS_PKT_LEN;
	unsigned char *pData = _pData;
	CAvFrame *pFrame = mAvFrame;

	// Skip Adaptation pkt
	if ((0x20 & pData[3]) == 0x20) {
		// Length Byte + Length
		nAdaptLen += pData[4] + 1;
	}
	
	pData += (nAdaptLen + 4);
	/* packet start code prefix and stream id */
	if(START_CODE_PREFIX_VALUE(pData) == 0x000001 && pData[3] == 0xC0) {
		/* Cpmplete the previous frame */
		if(pFrame->mUsedSize) {
			pFrame->mFrameState = FRAME_STATE_COMPLETE;
			return 0;
		}

		m_lPesLen = PACKET_LENGTH_VALUE(pData);
		/* Move the Data pointer to start of PES HEADER */
		pData += 6;

		if((PES_PTS_DTS_FLAGS_VALUE(pData)& 0x02)){
			unsigned char *pb = pData+3;
			pFrame->mPts = PES_PTS_VALUE_HI(pb) | PES_PTS_VALUE_LO(pb);
		}
		if((PES_PTS_DTS_FLAGS_VALUE(pData)& 0x01)){
			unsigned char *pb = pData+8;
			pFrame->mDts = PES_PTS_VALUE_HI(pb) | PES_PTS_VALUE_LO(pb);
		}
		long lPesHdrLen = PES_HEADER_LENGTH_VALUE(pData);
		pData += lPesHdrLen + 3;
		if(m_lPesLen)
			m_lPesLen -= (lPesHdrLen + 3);
	}

	lDataLen = 	lDataLen - (pData - _pData);

	//DbgOut(DBG_LVL_TRACE,("peslen=%d datalen=%d\n",m_lPesLen, lDataLen));

	if(m_lPesLen > 0) {
		if (lDataLen > m_lPesLen){
			lDataLen = m_lPesLen;
			m_lPesLen = 0;
		} else{
			m_lPesLen -= lDataLen;
		}
	}
	if(pFrame->mUsedSize < pFrame->mMaxSize - lDataLen){
		memcpy(pFrame->mFrmData + pFrame->mUsedSize, pData, lDataLen);
		pFrame->mUsedSize += lDataLen;
	} else {
		//TODO: reallocate buffer and copy
	}
	//DbgOut(DBG_LVL_TRACE,("Leave\n"));

	return 0;
}

int CPcrParser::Process(unsigned char *pTsPkt)
{
	unsigned char *pAdapt = pTsPkt + 4;
	if((ADAPTATION_FIELD_CONTROL_VALUE(pTsPkt) == 0x02)&& PCR_FLAG_BIT(pAdapt)){
		mllPcr = PCR_BASE_VALUE(pAdapt+2);
		mlPcrExt = PCR_EXT_VALUE(pAdapt+2);
		DbgOut(DBG_LVL_MSG,("mllPcr=%lld mlPcrExt=%ld", mllPcr, mlPcrExt));
	}
	return 0;
}

/*
** It holds list of elementary stream and pcr pkt parsers
** and dispatches the pkt parsing to these parsers
** A pkt parser is enabled explicitly using the EnableElemStream API
** TODO: Enhance for multiple programs
*/
class CTsDemux : public CTsDemuxIf
{
public:
	CTsDemux()
	{
		mPcrParser = NULL;
	}

	~CTsDemux()
	{
		for(std::map<int, CPktParser *>::iterator it = mlistParsers.begin(); it != mlistParsers.end(); it++){
			CPktParser *pParser = (*it).second;
			delete pParser;
		}
		mlistParsers.clear();
		if(mPcrParser)
			delete mPcrParser;
	};

	// Enables an elementary stream pkt parser for a PID
	int EnableElemStream(int nPid, int nStrmType);

	int EnablePcrProcessing(int nPid);
	CTsDemuxIf::CAvcCfgRecord *GetAvcCfg(int nPid);

	void Reset();
	
	// Parse multiple TS packets. Returns NULL or completed AV Frame
	CAvFrame   *Parse(CTsbuffer *pTsData);
	
	// Parse data from a TS Packet. Returns NULL or completed AV Frame
	CAvFrame   *ParsePkt(unsigned char *pPktData);

	int RegisterPktParser(int nPid, CPktParser *pPktProcess);

	CPktParser *mPcrParser;

	int        mPcrPid;
	std::map<int, CPktParser *> mlistParsers;
};

int CTsDemux::EnableElemStream(int nPid, int nCodecId)
{
	DbgOut(DBG_LVL_STRM,("Enter\n"));
	CPktParser *pPktParser = NULL;

	if(CTsDemuxIf::CODEC_ID_H264 == nCodecId)
		pPktParser = new CVPktParser();
	else if (CTsDemuxIf::CODEC_ID_AAC == nCodecId)
		pPktParser = new CAPktParser();
	if(pPktParser) {
		mlistParsers[nPid] = pPktParser;
		CAvFrame *pFrame = new CAvFrame(2 * 1024 * 1024, nPid);
		pFrame->mTsPid = nPid;
		pPktParser->mAvFrame = pFrame;
	}
	DbgOut(DBG_LVL_STRM,("Leave\n"));
	return 0;
}

int CTsDemux::EnablePcrProcessing(int nPid)
{
	mPcrParser = new CPcrParser();
	mPcrPid = nPid;
	return 0;
}

int CTsDemux::RegisterPktParser(int nPid, CPktParser *pPktParser)
{
	mlistParsers[nPid] = pPktParser;
	return 0;
}

CTsDemuxIf::CAvcCfgRecord *CTsDemux::GetAvcCfg(int nPid)
{
	return NULL;
}

void CTsDemux::Reset()
{
	for(std::map<int, CPktParser *>::iterator it = mlistParsers.begin(); it != mlistParsers.end(); it++){
		CPktParser *pParser = (*it).second;
		pParser->Reset();
	}
}

CAvFrame *CTsDemux::Parse(
		CTsbuffer *pTsBuffer
	)
{
	DbgOut(DBG_LVL_STRM,("Enter pTsBuffer=%p pTsData=%p mRdOffset=%d mUsedSize=%d\n", pTsBuffer, pTsBuffer->mData, pTsBuffer->mRdOffset, pTsBuffer->mUsedSize));

	while(pTsBuffer->mRdOffset < pTsBuffer->mUsedSize) {
		CPktParser *pPktParser = NULL;
		unsigned char *pb = (unsigned char *)pTsBuffer->mData + pTsBuffer->mRdOffset;
		int nPid = (WORD_VALUE(pb,1)) & 0x00001fff;
		//pPktParser = mlistParsers[nPid];
		std::map<int, CPktParser *>::iterator it = mlistParsers.find(nPid);
		if(it != mlistParsers.end()){
			pPktParser = it->second;
			if(pPktParser) {
				pPktParser->Process(pb);
				if(pPktParser->mAvFrame) {
					if(pPktParser->mAvFrame->mFrameState == FRAME_STATE_COMPLETE)
						return pPktParser->mAvFrame;
				}
			}
		}

		if(mPcrParser && nPid == mPcrPid){
			mPcrParser->Process(pb);
		}
		pTsBuffer->mRdOffset += TS_PKT_LEN;
	}
	DbgOut(DBG_LVL_STRM,("Leave\n"));
	return NULL;
}

CAvFrame *CTsDemux::ParsePkt(
	unsigned char *pPktData
	)
{
	CPktParser *pPktParser = NULL;
	unsigned char *pb = pPktData;
	int nPid = (WORD_VALUE(pb,1)) & 0x00001fff;
	std::map<int, CPktParser *>::iterator it = mlistParsers.find(nPid);
	if(it != mlistParsers.end()){
		pPktParser = it->second;
		if(pPktParser) {
			pPktParser->Process(pb);
			if(pPktParser->mAvFrame->mFrameState == FRAME_STATE_COMPLETE)
				return pPktParser->mAvFrame;
		}
	}
	DbgOut(DBG_LVL_STRM,("Leave\n"));
	return NULL;
}

CTsDemuxIf *CTsDemuxIf::CreateInstance()
{
	CTsDemuxIf	*pDemux = new CTsDemux();
	return pDemux;
}

