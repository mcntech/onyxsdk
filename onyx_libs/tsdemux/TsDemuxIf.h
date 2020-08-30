#ifndef __TS_DEMUX_IF__
#define __TS_DEMUX_IF__

#define int64_t __int64

#define PSI_STREM_TYPE_H264			0x1B
#define PSI_STREM_TYPE_AAC_ADTS		0x0F
#define TS_PKT_LEN           188
//#define CODEC_TYPE_UNKNOWN	0
//#define CODEC_TYPE_AUDIO	1
//#define CODEC_TYPE_VIDEO	2

//#define CODEC_ID_UNKNOWN	0
//#define CODEC_ID_H264		1
//#define CODEC_ID_AAC		2

#define MAX_SPS_SIZE		256
#define MAX_PPS_SIZE		256
#define MAX_AAC_CFG_LEN		8

#include <list>

/**
 * Intermediate buffer to cache the file data
 */
class CTsbuffer
{
public:
	CTsbuffer(int nMaxSize);
	~CTsbuffer();
	char               *mData;     // ts buffer pointer
	int                mRdOffset;    // consumed data offset
	int                mUsedSize; // actaul data size
	int                mMaxSize;     // Max buffer size
};

typedef enum
{
	FRAME_STATE_INIT,
	FRAME_STATE_FILLING,
	FRAME_STATE_COMPLETE
} FRAME_STATE;
/**
 * Frame parsed by the Demux routine
 */
class CAvFrame
{
public:
	CAvFrame(int nMaxDataSize, int nPid);
	~CAvFrame();
	void Reset()
	{
		mFrameState = FRAME_STATE_INIT;
		mUsedSize = 0;
	}
	char               *mFrmData;    // frame data pointer
	int                mUsedSize;    // actual data size
	int                mMaxSize;     // Max buffer size
	long long          mPts;       // Presentation time
	long long          mDts;       // Presentation time

	FRAME_STATE       mFrameState;    // Frame complete indication

	int                mCodecId;     // Codec ID used 
	int                mTsPid;       // TS Program ID
	
	int                mSynchPoint;
};

class CPktParser
{
public:
	CPktParser()
	{	
		mAvFrame = NULL;
		m_nPktLen = TS_PKT_LEN; 
	};
	virtual ~CPktParser(){};
	virtual int Process(unsigned char *pTsData) = 0;
	void Reset()
	{
		m_lPesLen = 0;
		if(mAvFrame)
			mAvFrame->Reset();
	}
public:
	int           m_nPktLen;
	int           m_nPid;
	long          m_lPesLen;
	CAvFrame      *mAvFrame;
};


/**
 * Demux interface class
 */
class CTsDemuxIf
{
public:
	enum CodecID_T
	{
		CODEC_ID_MPT2 = 33,
		/* Dynamic Codec IDs */
		CODEC_ID_H264 = 96,
		CODEC_ID_AAC = 97
	};

class CAvcCfgRecord {
public:
	unsigned char bVersion;
	unsigned char Profile;
	unsigned char ProfileCompat;
	unsigned char Level; 
	unsigned char PictureLengthSize; 
	unsigned short nonStoredDegredationPriorityLownonReferenceDegredationPriorityLow;
	unsigned short nonReferenceStoredDegredationPriorityHigh;
	//bit(6) reserved = ‘111111’b;
	//unsigned int(2) lengthSizeMinusOne; 
	unsigned char lengthSizeMinusOne;
	unsigned char numOfSequenceParameterSets;
	//for (i=0; i< numOfSequenceParameterSets;  i++) {
	//	unsigned int(16) sequenceParameterSetLength ;
	//	bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
	//}
	unsigned int	lSpsSize;
	unsigned char	Sps[MAX_SPS_SIZE];
	unsigned char numOfPictureParameterSets;

	//for (i=0; i< numOfPictureParameterSets;  i++) {
	//		unsigned int(16) pictureParameterSetLength;
	//		bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
	//	}
	unsigned int	lPpsSize;
	unsigned char	Pps[MAX_PPS_SIZE];
};

class CAacCfgRecord
{
public:
	CAacCfgRecord() {cfgLen = 0;}
	int cfgLen;
	char cfgRec[MAX_AAC_CFG_LEN];
};

public:
	CTsDemuxIf(){}
	virtual ~CTsDemuxIf(){}
	static CTsDemuxIf *CreateInstance();
	virtual int RegisterPktParser(int nPid, CPktParser *pPktProcess) = 0;
	virtual int EnableElemStream(int nPid, int nStrmType) = 0;
	virtual int EnablePcrProcessing(int nPid) = 0;
	virtual CAvFrame *Parse(CTsbuffer *pTsData) = 0;
	virtual CAvFrame *ParsePkt(unsigned char *pPktData) = 0;

	virtual CAvcCfgRecord *GetAvcCfg(int TrackId) = 0;
	virtual void Reset() = 0;
protected:
	static CTsDemuxIf *m_Instance;
};

#endif //__TS_DEMUX_IF__