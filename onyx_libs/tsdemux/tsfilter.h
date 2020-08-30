#ifndef __TS_FILTER_H__
#define __TS_FILTER_H__
#ifdef WIN32
#else
#endif
#include <queue>
using std::queue;
using std::pair;

//#include "StrmClock.h"
#include "TsDemuxIf.h"
#include "TsPsi.h"

#define OUTPUT_TYPE_STREAM	1
#define OUTPUT_TYPE_NALU	2
#define OUTPUT_TYPE_AVC1	3

#define SRC_TYPE_FILE		0
#define SRC_TYPE_RTSP		1
#define SRC_TYPE_HTTP		2
#define SRC_TYPE_HTTPLIVE	3

#define AAC_BUFFERED_FRAME_COUNT	40
#define AAC_MAX_FRAME_SIZE			(8 * 1024)

#define MAX_DEMUX_BUFF				(4 * 1024 * 1024)
#ifdef EN_TS
#define JB_PKT_COUNT		2048
#define JB_PKT_SIZE			2*1024
#else
#define JB_PKT_COUNT		12
#define JB_PKT_SIZE			512*1024
#endif

#define MAX_ELEM_STREAMS	16

class CH264Stream;
class CAacStream;
 
 /**
 * Contains one or more TS packets for audio or video
 */
class CAvSample
{
public:
	CAvSample(long lLen) 
	{
		m_pData = (char *)malloc(lLen);
		m_lMaxLen = lLen;
		m_lPts = 0;
		m_lDts = 0;
		m_lUsedLen = 0;
		m_fDisCont = 0;
	}
	~CAvSample()
	{
		if(m_pData) free(m_pData);
	}
	char	*m_pData;
	long	m_lUsedLen;
	long	m_lMaxLen;
	long long	m_lPts;
	long long	m_lDts;
	int        m_fDisCont;
};

class ITrackAssocInf 
{
public:
	virtual int getTrackId() = 0;
	virtual void setTrackId(int nTrackId) = 0;
	virtual void setTrackEoF() = 0;
	virtual CAvSample *getEmptySample() = 0;
	virtual void putEmptySample(CAvSample *) = 0;
	virtual void putFilledSample(CAvSample *) = 0;
	virtual void resetSampleQueue() = 0;
	virtual bool IsStreaming() = 0;
};

class CSourceStream : public ITrackAssocInf
{
public:
    int Init(void) { return 0; }
    int Exit(void) { return 0; }
    int Run(void) { return 0; }
    int Pause(void) { return 0; }
    int Stop(void) { return 0; }
};

/**
 * Implementation source filter
 * Stream is obtained from file, rtsp or http live source.
 */
class CTsFilter
{
public:

    static  CTsFilter *CreateInstance(long *phr);

	int InitFileSrc(char *lpszFileName);
	int DeinitFileSrc();

    CTsFilter(long *phr);
	virtual ~CTsFilter();
    int Stop();
    int Pause();
    int Run(unsigned long long ullStartMs);

	int InitProgram(int nProgram, CH264Stream **pVidSrc, CAacStream **pAudSrc);

private:
	int CreateH264OutputPin();
	int CreateMP4AOutputPin();

	int OnThreadStartPlay(void);
	int OnThreadDestroy(void);

	static void *DoBufferProcessing(void *pObj);
	long DemuxStream();

	int     m_fRun;
	long     m_lVidWidth;
	long     m_lVidHeight;
	

	int     m_AudSampleRate;
	int     m_AudNumChannels;
	int     m_ProgramNum;
	int     m_fLoop;
	unsigned long long m_ullTotalBytes;
	unsigned long long m_timeStreamStart;
	CTsDemuxIf::CAacCfgRecord	    AacCfgRecord;
private:
	char      m_szFileName[256];

	//CStrmClock			*m_pClock;
	CTsDemuxIf         *m_pDemux;
	CTsbuffer          *m_TsBuffer;
	CTsPsiIf           *m_pTsPsi;
#ifdef WIN32
	HANDLE         m_hThread;
#else
	int				m_hThread;
#endif
    //CCritSec			m_cSharedState;            // Lock on m_rtSampleTime and m_Ball
	int				   m_hFile;
	int				   m_iPins;
	CSourceStream      *m_paStreams[MAX_ELEM_STREAMS];
};

class CH264Stream : public CSourceStream
{

public:

    CH264Stream(long *phr, CTsFilter *pParent);

    virtual ~CH264Stream();

    // plots a ball into the supplied video frame
    int FillBuffer(void *pms);

	unsigned char *m_pQ2nbuff;
	int		m_fRun;

	int getTrackId(){return m_nTrackId;}
	void setTrackId(int nTrackId){m_nTrackId = nTrackId;}
	void setTrackEoF(){m_fEof = true;}
	CAvSample *getEmptySample()
	{
		 CAvSample *pSample = NULL;
		if(!m_EmptyQueue.empty()){
			pSample = m_EmptyQueue.front();
			m_EmptyQueue.pop();
		}
		return pSample;
	}
	void putEmptySample(CAvSample *pAvSample)
	{
		m_EmptyQueue.push(pAvSample);
	}
	void putFilledSample(CAvSample *pAvSample)
	{
		m_FilledQueue.push(pAvSample);
	}
	virtual void resetSampleQueue()
	{
		CAvSample *pSample;
		while (!m_FilledQueue.empty()){
			pSample = m_FilledQueue.front();
			m_FilledQueue.pop();
			m_EmptyQueue.push(pSample);
		}
		m_fEof = false;
	}
	bool IsStreaming()
	{
		// todo
		return 1;
	}
public:
    unsigned long long m_rtSampleTime;            // The time stamp for each sample
	long	m_lOutputType;
	unsigned short m_usNextSeqNum;


	int                m_nTrackId;
    int                m_fEof;
	queue<CAvSample *> m_FilledQueue;
	queue<CAvSample *> m_EmptyQueue;
	unsigned char	m_Sps[MAX_SPS_SIZE];
	int             m_nSpsSize;

}; // CTsStream

/**
 * Implementation of TS output pin 
 */
class CAacStream : public CSourceStream, public ITrackAssocInf
{

public:

    CAacStream(long *phr, CTsFilter *pParent);

    virtual ~CAacStream();

    int FillBuffer(void *pms);


	unsigned char *m_pQ2nbuff;
	int		m_fRun;

	int getTrackId(){return m_nTrackId;}
	void setTrackId(int nTrackId){m_nTrackId = nTrackId;}
	void setTrackEoF(){m_fEof = true;}
	CAvSample *getEmptySample()
	{
		 CAvSample *pSample = NULL;
		if(!m_EmptyQueue.empty()){
			pSample = m_EmptyQueue.front();
			m_EmptyQueue.pop();
		}
		return pSample;
	}
	void putEmptySample(CAvSample *pAvSample)
	{
		m_EmptyQueue.push(pAvSample);
	}
	void putFilledSample(CAvSample *pAvSample)
	{
		m_FilledQueue.push(pAvSample);
	}
	virtual void resetSampleQueue()
	{
		CAvSample *pSample;
		while (!m_FilledQueue.empty()){
			pSample = m_FilledQueue.front();
			m_FilledQueue.pop();
			m_EmptyQueue.push(pSample);
		}
		m_fEof = false;
	}
	bool IsStreaming()
	{
		// todo
		return 1;
	}

public:

    unsigned long long m_rtSampleTime;            // The time stamp for each sample
	long	m_lOutputType;
	unsigned short m_usNextSeqNum;

	int                m_nTrackId;
    int                m_fEof;
	queue<CAvSample *> m_FilledQueue;
	queue<CAvSample *> m_EmptyQueue;
}; // CTsStream
#endif  __TS_FILTER_H__