#include <assert.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#define	 OSAL_WAIT(x) Sleep(x)
#define read	_read
#define pthread_t void *
#else

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#define	 OSAL_WAIT(x) usleep(x * 1000)
#include <pthread.h>
#define  O_BINARY     0
#endif
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <map>
#include <list>
#include "JdDbg.h"


#include "JdHttpLiveSgmt.h"
#include "JdHttpSrv.h"
#include "JdHttpClnt.h"
#include "TsParse.h"
#include "HlsOutBase.h"
#include "HlsPublishBase.h"

#define EN_S3_UPLOAD

#ifdef EN_S3_UPLOAD
#include "JdAwsS3.h"
#include "HlsOutJdAws.h"
#include "JdAwsS3UpnpHttpConnection.h"
#include "JdAwsConfig.h"
#endif

//#define TESTING_ROKU
static CJdDbg            mJdDbg("HttpLiveSgmnt", CJdDbg::LVL_WARN);
static int               modDbgLevel = CJdDbg::LVL_WARN;//LVL_TRACE;
/* Delay updating S3 Playlist by S3_CACHE_DELAY segments */
#define S3_CACHE_DELAY				2
#ifdef TESTING_ROKU
#define NUM_SEGS_IN_BCAST_PLAYLIST	3 //0 for no delay. tried with 20 for roku speed loading
#define MAX_FILES					6
#else
#define MAX_FILES					4
#define NUM_SEGS_IN_BCAST_PLAYLIST	3
#endif
#define PLAYLIST_BUFFER_SIZE		(4 * 1024)
#define PLAYLIST_MAX_SIZE			(4 * 1024 * 1024)
#define WAIT_TIME_FOR_FILLED_BUFF	4000 //100000
#define WAIT_TIME_FOR_EMPTY_BUFF	100000

#define MAX_SEGMENT_NAME			128
#define MAX_PL_LINE_SIZE			256

#define	HLS_OUTPUT_S3               1
#define	HLS_OUTPUT_FS               2

#ifndef MAX_PATH
#define MAX_PATH                    256
#endif

class CHlsMemFileServer;
CHlsMemFileServer *GetMemFileServerForResource(const char *pszResource);

#define foreach_in_map(type,list) for (std::map<std::string,type>::iterator it=list.begin();it != list.end(); it++)

#define M3U8_HEADER "Connection: close\r\n" \
                   "Cache-Control: no-cache\r\n" \
				   "Content-Type: audio/x-mpegurl\r\n" \
                   "Accept-Ranges: bytes\r\n" \
                   "Server: OnyxLive/0.2\r\n"

//Content-Type: audio/x-mpegurl
//Content-Type: application/vnd.apple.mpegurl

#define SEGMENT_HEADER "Connection: close\r\n" \
                   "Cache-Control: no-cache\r\n" \
				   "Content-Type: application/octet-stream\r\n" \
                   "Accept-Ranges: bytes\r\n" \
                   "Server: OnyxLive/0.2\r\n" 

//application/octet-stream
//video/MP2T

class CHlsOutFs : public CHlsOutBase
{
public:
	int Start(const char *pszParentFolder, const char *pszFile, int nTotalLen, char *pData, int nLen, const char *pContentType)
	{
		char szFile[256];
		sprintf(szFile, "%s/%s",pszParentFolder, pszFile);
		int fd  =  open(szFile,  O_CREAT|O_RDWR|O_TRUNC|O_BINARY ,
#ifdef WIN32
	_S_IREAD | _S_IWRITE);
#else
	 S_IRWXU | S_IRWXG | S_IRWXO);
#endif

		if(fd > 0) {
			m_pCtx = (void *)fd;
			if(pData && nLen> 0) {
				write(fd, pData, nLen);
			}
		} else {
			m_pCtx = (void *)-1;
			JDBG_LOG(CJdDbg::LVL_ERR,("!!! Failed to open %s!!!\n",pszParentFolder));
		}
		return 0;
	}

	int Continue(char *pData, int nLen)
	{
		int res = 0;
		int fd = (int)m_pCtx;
		if(fd != -1) {
			if(pData && nLen > 0) {
				res = write(fd, pData, nLen);
			}
		}
		return res;
	}

	int End(char *pData, int nLen)
	{
		int res = -1;
		int fd = (int)m_pCtx;
		if(fd != -1)  {
			if(pData && nLen > 0) {
				res = write(fd, pData, nLen);
			}
			close(fd);
		}
		return res;
	}

	int Send(const char *pszParentFolder, const char *pszFile, char *pData, int nLen, const char *pContentType, int nTimeOut)
	{
		int res = 0;
		char szFile[256];
		sprintf(szFile, "%s/%s",pszParentFolder, pszFile);
		int fd  =  open(szFile,  O_CREAT|O_RDWR|O_TRUNC|O_BINARY ,
#ifdef WIN32
	_S_IREAD | _S_IWRITE);
#else
	 S_IRWXU | S_IRWXG | S_IRWXO);
#endif
		if(fd > 0) {
			res = write(fd, pData, nLen);
			close(fd);
		} else {
#ifdef WIN32
			res = GetLastError();
#else
#endif
		}
		return res;
	}

	int Delete(const char *pszParentFolder, const char *pszFile)
	{
		int res = 0;
		char szFile[256];
		sprintf(szFile, "%s/%s",pszParentFolder, pszFile);
		unlink(szFile);
		return 0;
	}
};

/**
 * Generate html file for camera live stream on the fly.
 */
int GetHtmlCamHttpLive(
	char *szbuff,
	char *szM3u8FileName,
	int nMaxLen, 
	int width, 
	int height)
{
	sprintf(szbuff,
		"<html>\n"	\
		"<head>\n"	\
		"<title>Jade Live Cast</title>\n" \
		"<meta name=\"viewport\" content=\"width=%d; initial-scale=1.0; maximum-scale=1.0; user-scalable=0;\"/>\n" \
		"</head>" \
		"<body style=\"background-color:#FFFFFF; \">\n" \
		"<center>\n" \
		"<video src=\"%s.m3u8\" controls autoplay ></video>\n" \
		"</center>\n"	\
		"</body>\n" \
		"</html>\n" \
		,width
		,szM3u8FileName
		);
	return strlen(szbuff);
}



/**
 * Generate M3u8 header
 * if szbuff is NULL, required buffer length is returned.
 */
int m3u8AddHeader(
		char *szbuff,				//< Buffer to return the m3u8 play list
		int nDuration,				//< Max duration of a segment
		int nSeqStart				//< Sequence number of the starting segment in the play list
		)
{
	char szTemp[MAX_PL_LINE_SIZE];
	if(nDuration == 0)	// May be due to rounding off
		nDuration = 1;

	sprintf(szTemp,
		"#EXTM3U\r\n" \
		"#EXT-X-ALLOW-CACHE:NO\r\n"
		"#EXT-X-TARGETDURATION:%d\r\n" \
		"#EXT-X-MEDIA-SEQUENCE:%d\r\n", 
		nDuration, nSeqStart);
	if(szbuff){
		strcpy(szbuff, szTemp);
	}
	return strlen(szTemp);
}

void GetSegmentNameFromIndex(char *pszSegmentFileName, const char *pszBaseName, int nSegmentIdx)
{
	sprintf(pszSegmentFileName,"%s_%d.ts", pszBaseName, nSegmentIdx);
}

/**
 * Generate M3u8 Segment
 * if szbuff is NULL, required buffer length is returned.
 */
int m3u8AddSegment(
		char *szbuff,				//< Buffer to return the m3u8 play list
		char *szSegmentBaseName,		//
		int nDuration,				//< Max duration of a segment
		int nSegment				//< Sequence number of the starting segment in the play list
		)
{
	if(nDuration == 0)	// May be due to rounding off
		nDuration = 1;
	char szTemp[MAX_PL_LINE_SIZE];
	char szSegmentName[MAX_PATH];

	GetSegmentNameFromIndex(szSegmentName,szSegmentBaseName,nSegment);
	sprintf(szTemp, 
		"#EXTINF:%d, no desc\r\n" \
		"%s\r\n",
		nDuration, szSegmentName, nSegment);
	if(szbuff){
		strcpy(szbuff, szTemp);
	}
	return strlen(szTemp);
}
/**
 * Generate M3u8 End of list
 * if szbuff is NULL, required buffer length is returned.
 */
int m3u8AddTail(
		char *szbuff				//< Buffer to return the m3u8 play list
		)
{
	char szTemp[MAX_PL_LINE_SIZE];
	sprintf(szTemp, "#EXT-X-ENDLIST\r\n");
	if(szbuff) {
		strcpy(szbuff, szTemp);
	}
	return strlen(szTemp);
}
/**
 * Generate m3u8 file for camera live stream on the fly.
 * if szbuff is NULL then retruns the size of the required buffer.
 */
int GetM3u8CamHttpLive(
		char *szbuff,				//< Buffer to return the m3u8 play list
		char *szSegmentName,		//
		int nMaxLen,				//< Buffer size
		int nDuration,				//< Max duration of a segment
		int nSeqStart,				//< Sequence number of the starting segment in the play list
		int nReqSegments,			//< Number requested segments in the playlist
		int *pnActualSegments,		//< Number of segments generated 
		int fHead,					//< Add Playlist header
		int fTail					//< Add Playlist End
		)
{
	if(nDuration == 0)	// May be due to rounding off
		nDuration = 1;
	int nBytesTotal = 0;
	int nBytes = 0;
	char szTemp[MAX_PL_LINE_SIZE];
	int i = 0;
	if(fHead) {
		nBytes = m3u8AddHeader(szbuff, nDuration, nSeqStart);
		nBytesTotal += nBytes;
		if(szbuff)
			szbuff += nBytes;
	}
	for (i=0; i < nReqSegments && nBytesTotal < nMaxLen ; i++){
		nBytes = m3u8AddSegment(szbuff, szSegmentName, nDuration, nSeqStart + i);
		nBytesTotal += nBytes;
		if(szbuff)
			szbuff += nBytes;
	}
	
	if(pnActualSegments)
		*pnActualSegments = i;

	if(fTail && nBytesTotal < nMaxLen){
		nBytes = m3u8AddTail(szbuff);
		nBytesTotal += nBytes;
	}
	return (nBytesTotal);	// include NULL char of 
}

/**
 * Stores a segment in the memory
 */
class CMemFile
{
public:
	CMemFile(int nMaxSize)
	{
		m_pData	= (char *)malloc(nMaxSize);	
		if(!m_pData){
			JDBG_LOG(CJdDbg::LVL_ERR,("!!! Can not allocate memory size=%d !!!\n",nMaxSize));
		}
		Reset();
		m_nMaxSize = nMaxSize;
	}
	~CMemFile()
	{
		free(m_pData);
	}
	
	void Reset()
	{
		m_nOffset = 0;
		m_fDisCont = 0;
		m_nFileIdx = 0;
	}

	int WriteData(char *pData, int nLen)
	{
		m_nTotlaBytes += nLen;
		if(m_nOffset + nLen < m_nMaxSize){
			memcpy(m_pData + m_nOffset, pData, nLen);
			m_nOffset += nLen;
			return 0;
		} else {
			JDBG_LOG(CJdDbg::LVL_ERR,("Overflow. at %d len=%d\n", m_nOffset, nLen));
			m_fDisCont = 1;
			return -1;
		}
	}

	int TransferFile(int hSock)
	{
		char *szHeader = STD_HEADER;
		char *szMimeType = "video/MP2T";
		SendHttpOK(hSock, m_nOffset, SEGMENT_HEADER);
		int res = send(hSock, m_pData, m_nOffset,0);
		return res;
	}

	int UploadTsFile(CHlsOutBase *pHlsOut, char *pszHost, char *pszRelUrl, int nTimeOut)
	{
		CHttpPut HttpPut;
		int res = pHlsOut->Send(pszHost, pszRelUrl, m_pData, m_nOffset, CONTENT_STR_MP2T, nTimeOut);
		return m_nOffset;
	}

	int m_nOffset;
	int m_nMaxSize;
	char *m_pData;
	int m_fDisCont;
	int m_nFileIdx;
	static int m_nTotlaBytes;
};

int CMemFile::m_nTotlaBytes = 0;

#define MAX_PREFIX_SIZE 256

static void DumpHex(unsigned char *pData, int nSize)
{
	int i;
	for (i=0; i < nSize; i++)
		printf("%02x ", pData[i]);
	printf("\n");
}

class COsalMutex
{
public:
	COsalMutex()
	{
#ifdef WIN32
		m_Mutex = CreateMutex(NULL, FALSE, NULL);
#else
		pthread_mutex_init(&m_Mutex, NULL);
#endif
	}
	~COsalMutex()
	{
#ifdef WIN32
		CloseHandle(m_Mutex);
#else
		 pthread_mutex_destroy(&m_Mutex);
#endif


	}
	void Acquire()
	{
#ifdef WIN32
		WaitForSingleObject(m_Mutex,INFINITE);
#else
		pthread_mutex_lock(&m_Mutex);
#endif
	}
	void Release()
	{
#ifdef WIN32
	ReleaseMutex(m_Mutex);
#else
	pthread_mutex_unlock(&m_Mutex);
#endif
	}
private:
#ifdef WIN32
	HANDLE          m_Mutex;
#else
	pthread_mutex_t m_Mutex;
#endif
};

class CGop : public COsalMutex
{
public:
	unsigned char *m_pBuff;
	int           m_nLen;
	int           m_nMaxLen;
	int           m_nGopNum;
	int           m_DurationMs;

	CGop( int nMaxLen)
	{
		m_pBuff = (unsigned char *)malloc(nMaxLen);
		m_nMaxLen = nMaxLen;
		m_nLen = 0;
		m_DurationMs = 500;
	}
	~CGop()
	{
		if(m_pBuff) {
			free(m_pBuff);
			m_pBuff = NULL;
		}
	}

	void Clear()
	{
		m_nLen = 0;
	}
	int AddData(unsigned char *pData, int nLen)
	{
		int nRes  = 0;
		if(m_pBuff && m_nLen + nLen < m_nMaxLen) {
			memcpy(m_pBuff + m_nLen, pData, nLen);
			m_nLen += nLen;
			nRes = nLen;
		}
		return nRes;
	}
};


/**
 * CHlsSegmnter: Manages segment list
 */
class CHlsSegmnter
{
public:
	CHlsSegmnter()
	{
		m_nWr = 0;
		m_nSeqStart = 0;
		m_fHasSps = 0;
		m_nGopSize = 2 * 1024 * 1024; // TODO
		m_nCrntGopNum = 0;
		m_pCrntGop = new CGop(m_nGopSize);
	}
	~CHlsSegmnter()
	{
		delete m_pCrntGop;
	}

	int IsFilling( int nFileIdx)
	{
		return (nFileIdx >= m_nSeqStart);
	}


	int WriteFrameData(char *pData, int nLen, int fVideo, int fHasSps, unsigned long ulPtsMs)
	{
		long lTimeout = WAIT_TIME_FOR_EMPTY_BUFF; // milli seconds
		unsigned long ulPtsHi = 0;
		unsigned long ulPtsLo = 0;
		char *pPkt = pData;

		if(fVideo && fHasSps ){
			UpdateOnNewGop();
		}

		while(pPkt < pData + nLen){
			if(fHasSps)
				m_fHasSps = fHasSps;

			if(m_pCrntGop)
				m_pCrntGop->AddData((unsigned char *)pPkt, TS_PKT_LEN);

			pPkt += TS_PKT_LEN;
		}
		return 0;
	}

	void UpdateOnNewGop()
	{
		/* Save the current gop */
		if(m_pCrntGop && m_pCrntGop->m_nLen > 0) {
			PublishGop(m_nCrntGopNum, (const char *)m_pCrntGop->m_pBuff, m_pCrntGop->m_nLen, m_pCrntGop->m_DurationMs);
			m_pCrntGop->Clear();
			m_nCrntGopNum++;
		} 
	}

	// Streaming data unknown au boundaries
	int WriteData(char *pData, int nLen)
	{
		long lTimeout = WAIT_TIME_FOR_EMPTY_BUFF; // milli seconds
		unsigned long ulPtsHi = 0;
		unsigned long ulPtsLo = 0;
		char *pPkt = pData;
		
		while(pPkt < pData + nLen){
			int nHasSps = CTsParse::HasSps((unsigned char *)pPkt, TS_PKT_LEN, &ulPtsHi, &ulPtsLo);
			unsigned long ulPtsMs = ulPtsHi / 90000 + ulPtsLo / 90;
			//DumpHex((unsigned char*)pData, 32);
			
			if(nHasSps){
				JDBG_LOG(CJdDbg::LVL_TRACE,("SPS=:%d time=%d ",nHasSps, ulPtsLo));
				UpdateOnNewGop();
			}
			if(nHasSps)
				m_fHasSps = nHasSps;
			if(m_pCrntGop)
				m_pCrntGop->AddData((unsigned char *)pPkt, TS_PKT_LEN);
			pPkt += TS_PKT_LEN;
		}
		return 0;
	}

	void AddPublisher(CHlsPublishBase *pPublish)
	{
		m_MutexPublish.Acquire();
		m_PublishList.push_back(pPublish);
		m_MutexPublish.Release();
	}

	void RemovePublisher(CHlsPublishBase *pPublish)
	{
		m_MutexPublish.Acquire();
		for (std::list<CHlsPublishBase *>::iterator it = m_PublishList.begin(); it != m_PublishList.end(); ++it) {
			CHlsPublishBase *pTmp = *it;
			if(pTmp == pPublish) {
				m_PublishList.erase(it);
				break;
			}
		}
		m_MutexPublish.Release();
	}

	void PublishGop(int nGopNum, const char *pData, int nLen, int nDurarionMs)
	{
		m_MutexPublish.Acquire();
		for (std::list<CHlsPublishBase *>::iterator it = m_PublishList.begin(); it != m_PublishList.end(); ++it) {
			CHlsPublishBase *pPublish = *it;
			pPublish->ReceiveGop(nGopNum, pData, nLen, nDurarionMs);
		}
		m_MutexPublish.Release();
	}

	std::list <CHlsPublishBase *>  m_PublishList;
	COsalMutex                     m_MutexPublish;

	int m_nWr;
	int m_fHasSps;
	int m_nSeqStart;

	CGop *m_pCrntGop;
	int  m_nCrntGopNum;
	int  m_nGopSize;
};

//===============================================================================================

int CHlsPublishBase::GetStats(HLS_PUBLISH_STATS *pStats)
{
	pStats->nState = m_nError;
	pStats->nLostBufferTime = m_nLostBufferTime / 1000;
	pStats->nSegmentTime = m_nSegmentTime / 1000;
	pStats->nStreamInTime = m_nInStreamTime / 1000;
	pStats->nStreamOutTime = m_nOutStreamTime / 1000;
	return 0;
}

CHlsPublishBase::CHlsPublishBase()
{
	m_nInStreamTime = 0;
	m_nOutStreamTime = 0;
	m_nError = 0;

	m_nLostBufferTime = 0;
	m_nSegmentTime = 0;

}

/**
 * CHlsSegmnter: Manages segment list
 */
class CHlsMemFileServer : public CHlsPublishBase
{
public:
	CHlsMemFileServer(const char *pszFilePrefix, int nSegmentDuration, int nSegmentSize)
	{
		m_nWr = 0;
		m_nSeqStart = 0;

		m_nSegmentDuration = nSegmentDuration;
		m_nSegmentCrntTimeMs = 0;
		m_nSegmentStartTimeMs = 0;
		for(int i=0; i <MAX_FILES; i++){
			m_pMemFiles[i] = new CMemFile(nSegmentSize); 
		}
		strncpy(m_szFilePrefix, pszFilePrefix, MAX_PREFIX_SIZE - 1);
	}
	~CHlsMemFileServer()
	{
		for(int i=0; i <MAX_FILES; i++){
			delete m_pMemFiles[i];
		}
	}

	int IsFilling( int nFileIdx)
	{
		return (nFileIdx >= m_nSeqStart);
	}

	int TransferFile(int hSock, char *pszFile)
	{
		long lTimeout = WAIT_TIME_FOR_FILLED_BUFF;
		int nRd = -1;

		int nFileIdx = CM3u8Helper::GetStrmAndIndexForSegment(pszFile,m_szFilePrefix);
		JDBG_LOG(CJdDbg::LVL_WARN,("Req Index %d. m_nWr=%d m_nSeqStart=%d\n",nFileIdx, m_nWr, m_nSeqStart));
		if(nFileIdx >= 0) {
#if 1
			while(IsFilling(nFileIdx) && lTimeout > 0){
				lTimeout -= 100;
				OSAL_WAIT(100);
			}
#endif
			nRd = GegRdIdxForFileIdx(nFileIdx);
			if(nRd >= 0){
				return m_pMemFiles[nRd]->TransferFile(hSock);
			}
		}

		JDBG_LOG(CJdDbg::LVL_WARN,("File Index %d not found. m_nWr=%d m_nSeqStart=%d\n",nFileIdx, m_nWr, m_nSeqStart));
		SendError(hSock, 404, "Could not open file");
		return -1;
	}

	int UploadTsFile(CHlsOutBase *pHlsOut, int nFileIdx, char *pszParentFolder, char *pszSegmentName)
	{
		long lTimeout = WAIT_TIME_FOR_FILLED_BUFF;
		int nRd = -1;

		while(IsFilling(nFileIdx) && lTimeout > 0){
			lTimeout -= 100;
			OSAL_WAIT(100);
		}
		nRd = GegRdIdxForFileIdx(nFileIdx);
		if(nRd >= 0){
			return m_pMemFiles[nRd]->UploadTsFile(pHlsOut, pszParentFolder, pszSegmentName, m_nSegmentDuration / 1000);
		}
		JDBG_LOG(CJdDbg::LVL_WARN,("File Index %d not found. m_nWr=%d m_nSeqStart=%d\n",nFileIdx, m_nWr, m_nSeqStart));
		return -1;
	}

	int GegRdIdxForFileIdx(int nFileIdx)
	{
		for(int i=0; i < MAX_FILES; i++){
			if(m_pMemFiles[i]->m_nFileIdx == nFileIdx)
				return i;
		}
		return -1;
	}
	int ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs) {return 0;};
	void Run()
	{
		m_nError = HLS_UPLOAD_STATE_RUN;
	};
	void Stop()
	{
		m_nError = HLS_UPLOAD_STATE_STOP;
	};
	int GetStats(HLS_PUBLISH_STATS *pStats)
	{
		return 0;
	}

	CMemFile *m_pMemFiles[MAX_FILES];
	int m_nSeqStart;

	int m_nWr;
	int m_nSegmentDuration;
	int m_nSegmentCrntTimeMs;
	int m_nSegmentStartTimeMs;
	char m_szFilePrefix[MAX_PREFIX_SIZE];
};

//
// Circualr buffer for storing H.264 Gops
// 
class CGopCircBuff
{
public:
	CGopCircBuff(int nBufSize) 
	{
		m_pData = (unsigned char *)malloc(nBufSize);
		m_nSize  = nBufSize; /* include empty elem */
		m_nRd = 0;
		m_nWr   = 0;
	}
 
	~CGopCircBuff() 
	{
		free(m_pData);
	}
 
	/*
	** Allocate memory for Gop data and advance the write ptr
	*/
	unsigned char *Alloc(int nSize) 
	{
		unsigned char *pData = 0;
		int nTailSize = 0;
		int nHeadSize = 0;
		
		if(m_nWr < m_nRd) {
			if(m_nRd - m_nWr > nSize) {
				pData = m_pData + m_nWr;
				m_nWr += nSize;
			}
		} else {
			int nTailSize = m_nSize - m_nWr - 1;
			if(nTailSize >= nSize) {
				pData = m_pData + m_nWr;
				m_nWr = m_nWr + nSize;
			} else if (m_nRd > nSize) {
				pData = m_pData;
				m_nWr = nSize;
			}
		}
		return pData;
	}

	/*
	** Free the Gop data AND advancE the read ptr
	*/
	void Free(unsigned char *pData, int nSize) 
	{
		int nTailSize = 0;
		int nHeadSize = 0;
		if(m_nRd  < m_nWr) {
			assert(pData ==  m_pData + m_nRd);
			m_nRd += nSize;
		} else {
			int nTailSize = m_nSize - m_nRd - 1;
			if(nTailSize >= nSize) {
				assert(pData ==  m_pData + m_nRd);
				m_nRd = m_nRd + nSize;
			} else {
				assert(pData == m_pData);
				m_nRd = nSize;
			}
		}
	}

	int         m_nSize;
	int         m_nRd;
	int         m_nWr;

	unsigned char *m_pData;
};

class CGopCb
{
public:
	unsigned char *m_pBuff;
	int           m_nLen;
	int           m_nGopNum;
	int           m_DurationMs;

	CGopCb(unsigned char *pData, int nLen)
	{
		m_pBuff = pData;
		m_nLen = nLen;
		m_DurationMs = 500;
	}
	~CGopCb()
	{
	}

	int AddData(unsigned char *pData, int nLen)
	{
		int nRes  = 0;
		memcpy(m_pBuff, pData, nLen);
		nRes = nLen;
		return nRes;
	}
};

/******************************************************************************
 * Uploads data from segmenter to http site or local file system
 */

class CHlsPublishS3 : public CHlsPublishBase
{
public:
	CHlsPublishS3(
		int			nTotalTimeMs,
		void		*pSegmenter, 
		const char	*pszM3u8File,
		const char	*pszParentFolder,
		const char	*pszBucket, 
		const char	*pszHost,
		const char	*szAccessId,
		const char	*szSecKey,
		int         fLiveOnly,
		int         nSegmentDuration,
		int         nDestType
		):
			m_nTotalTimeMs(nTotalTimeMs),
			m_pszM3u8File(strdup(pszM3u8File)),
			m_fLiveOnly(fLiveOnly)
	{
		// TODO: Pass as parameter
		m_nSegmentDurationMs = nSegmentDuration;
		if (nDestType == UPLOADER_TYPE_S3) {
			m_pHlsOut = new CHlsOutJdS3(pszBucket, pszHost, szAccessId, szSecKey);
			
		} else if (nDestType == UPLOADER_TYPE_DISC) {
			m_pHlsOut = new CHlsOutFs;
		}
		m_pszParentFolder = strdup(pszParentFolder);

		m_nGopDuration = 500;
		m_nGopSize = 1 * 1024 * 1024;
		m_pCb = new CGopCircBuff(10 * 1024 * 1024);
		
		m_fRunState = 0;

		m_pSegmenter = (CHlsSegmnter *)pSegmenter;
		m_nSegmentStartIndex = time(NULL);//1;
		m_nSegmentIndex = m_nSegmentStartIndex;
	}
	
	~CHlsPublishS3()
	{
		if(m_pszM3u8File) free(m_pszM3u8File);
		if(m_pszParentFolder) free(m_pszParentFolder);
	}

	int GetAvailBuffDuration()
	{
		int nDuration = 0;
		std::list<CGopCb *>::iterator it;
		m_Mutex.Acquire();
		for (it = m_pGopFilledList.begin(); it != m_pGopFilledList.end(); it++){
			nDuration += (*it)->m_DurationMs;
		}
		m_Mutex.Release();
		return nDuration;
	}
	int GetSegmentLen()
	{
		int nSegmentLen = 0;
		int nDuration = 0;
		std::list<CGopCb *>::iterator it;
		m_Mutex.Acquire();
		for (it = m_pGopFilledList.begin(); it != m_pGopFilledList.end(); it++){
			nSegmentLen += (*it)->m_nLen;
			nDuration += (*it)->m_DurationMs;
			if(nDuration >= m_nSegmentDurationMs)
				break;
		}
		m_Mutex.Release();
		return nSegmentLen;
	}
	int SegmentCount() { return m_nSegmentIndex - m_nSegmentStartIndex;}
	int ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs);
	DWORD Process();
	void Run();
	void Stop();


#ifdef WIN32
static DWORD WINAPI thrdStreamHttpLiveUpload(	void *pArg);
#else
static void *thrdStreamHttpLiveUpload(void *pArg);
#endif

	void UpLoadBroadcastPlayList(
		char			*pSegmentBaseName,
		int				nSeqStart,
		int             nSegments);

	void UpLoadFullPlayList(
		char			*pSegmentBaseName,
		int				nStartSegment,
		int				nCountSegments);

	int					m_nTotalTimeMs;

	char				*m_pszParentFolder;
	char				*m_pszM3u8File;

	CHlsOutBase         *m_pHlsOut;
	pthread_t			thrdHandle;
	int                 m_fLiveOnly;
	int                 m_nSegmentDurationMs;

	std::list <CGopCb *>  m_pGopFilledList;

	CGopCircBuff        *m_pCb;
	COsalMutex          m_Mutex;

	int                 m_nGopSize;
	int                 m_nGopDuration;
	int                 m_fRunState;

    int                 m_nSegmentStartIndex;
	int                 m_nSegmentIndex;
	CHlsSegmnter		*m_pSegmenter;
};

int CHlsPublishS3::ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs)
{
	unsigned char *pCbData;
	CGopCb *pGop;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	m_nInStreamTime += nDurarionMs;
	m_Mutex.Acquire();
	JDBG_LOG(CJdDbg::LVL_MSG,("Enter:Receiving Gop %d:  nLen=%d nDurarionMs=%d", nGopNum, nLen, nDurarionMs));
	pCbData = m_pCb->Alloc(nLen);
	if(pCbData == NULL) {
		JDBG_LOG(CJdDbg::LVL_ERR,("No Buffer"));
		m_nLostBufferTime += nDurarionMs;
		goto Exit;
	}

	pGop = new  CGopCb(pCbData, nLen);
	memcpy(pGop->m_pBuff, pData, nLen);
	m_pGopFilledList.push_back(pGop);
Exit:
	JDBG_LOG(CJdDbg::LVL_MSG,(":Leave"));
	m_Mutex.Release();
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}
//=================================================================================
// CMemSegmentFile is helper class used by CHlsPublishMemFile to write a local file
class CMemSegmentFile
{
public:
	int Open(const char *pszParentFolder, const char *pszFile)
	{
		char szFile[256];
		sprintf(szFile, "%s/%s",pszParentFolder, pszFile);
		m_fd  =  open(szFile,  O_CREAT|O_RDWR|O_TRUNC|O_BINARY ,
#ifdef WIN32
	_S_IREAD | _S_IWRITE);
#else
	 S_IRWXU | S_IRWXG | S_IROTH);
#endif
		if(m_fd < 0) {
			JDBG_LOG(CJdDbg::LVL_ERR,("!!! Failed to open %s!!!\n",pszParentFolder));
			return -1;
		}
		return 0;
	}

	int Write(const char *pData, int nLen)
	{
		int res =  0;
		if(m_fd > 0) {
			res = write(m_fd, pData, nLen);
		}
		return res;
	}

	void Close()
	{
		close(m_fd);
	}

	int Send(const char *pszParentFolder, const char *pszFile, char *pData, int nLen)
	{
		int res = 0;
		if(Open(pszParentFolder, pszFile) ==0) {
			res = write(m_fd, pData, nLen);
			close(m_fd);
		} else {
#ifdef WIN32
			res = GetLastError();
#else
			res = -1;
#endif
		}
		return res;
	}

	static int Delete(const char *pszParentFolder, const char *pszFile)
	{
		int res = 0;
		char szFile[256];
		sprintf(szFile, "%s/%s",pszParentFolder, pszFile);
		unlink(szFile);
		return 0;
	}

public:
	int m_fd;
};

/******************************************************************************
 * Uploads data from segmenter to http site or local file system
 */
class CHlsPublishMemFile : public CHlsPublishBase
{
public:
	CHlsPublishMemFile(
		void		*pSegmenter, 
		const char	*pszM3u8File,
		const char	*pszServerRoot,
		const char	*pszFolder,
		int          nSegmentDuration
		)
	{
		char szHlsFolderPath[256];
		// TODO: Pass as parameter
		m_nSegmentDurationMs = nSegmentDuration;
		m_pSegmenter = (CHlsSegmnter *)pSegmenter;
		sprintf(szHlsFolderPath, "%s/%s", pszServerRoot, pszFolder);
		m_pszParentFolder = strdup(szHlsFolderPath);
		m_pszM3u8File = strdup(pszM3u8File);
		m_pszM3u8BaseName = CM3u8Helper::GetPlayListBaseName(pszM3u8File);
		m_nSegmentStartIndex = time(NULL);//1;
		m_nSegmentIndex = m_nSegmentStartIndex;
		m_nSegmentCount = 0;
		m_CrntSegmentDuration = 0;
		m_nError = 0;
	}
	
	~CHlsPublishMemFile()
	{
		if(m_pszM3u8File) free(m_pszM3u8File);
		if(m_pszParentFolder) free(m_pszParentFolder);
		if(m_pszM3u8BaseName)
			free(m_pszM3u8BaseName);

	}
	int ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs);
	void Run()
	{ 
		m_pSegmenter->AddPublisher(this);
		m_fRunState = 1;
		m_nError = HLS_UPLOAD_STATE_RUN;
	}
	void Stop() 
	{ 
		m_pSegmenter->RemovePublisher(this);
		m_fRunState = 0;
		m_nError = HLS_UPLOAD_STATE_STOP;
	}
	int GetStats(HLS_PUBLISH_STATS *pStats)
	{
		return 0;
	}
	void UpLoadBroadcastPlayList(
		char			*pSegmentBaseName,
		int				nSeqStart,
		int             nSegments);
	void UpdatePlayList();

	CMemSegmentFile     m_MemFile;
	CHlsSegmnter		*m_pSegmenter;
	char				*m_pszParentFolder;
	char				*m_pszM3u8File;
	char                *m_pszM3u8BaseName;
	int                 m_nSegmentDurationMs;
	int                 m_nSegmentIndex;
	int                 m_nSegmentStartIndex;
	int                 m_nSegmentCount;
	int                 m_CrntSegmentDuration;
	int                 m_fRunState;
	int                 m_nError;
};

void CHlsPublishMemFile::UpLoadBroadcastPlayList(
		char			*pSegmentBaseName,
		int				nSeqStart,
		int             nSegments)
{
	int nLen = 0;

	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));

	CMemSegmentFile MemFile;
	char *buffer = (char *)malloc(PLAYLIST_BUFFER_SIZE);
	nLen = GetM3u8CamHttpLive(buffer, pSegmentBaseName, PLAYLIST_BUFFER_SIZE, m_nSegmentDurationMs / 1000, nSeqStart, nSegments, NULL, 1, 0);
	MemFile.Send(m_pszParentFolder, m_pszM3u8File, buffer,nLen);
	if(buffer)
		free(buffer);
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

#define  MEM_FILE_CACHE_DELAY 3
void CHlsPublishMemFile::UpdatePlayList()
{
	int nSegmentCount;
	int nSegmentStart;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(m_nSegmentCount > NUM_SEGS_IN_BCAST_PLAYLIST) {
		nSegmentCount = NUM_SEGS_IN_BCAST_PLAYLIST;
		nSegmentStart = m_nSegmentIndex - (NUM_SEGS_IN_BCAST_PLAYLIST - 1);
	} else {
		nSegmentCount = m_nSegmentCount;
		nSegmentStart = m_nSegmentIndex - (nSegmentCount - 1);
	}
	UpLoadBroadcastPlayList(m_pszM3u8BaseName, nSegmentStart, nSegmentCount);

	// delete unused segment
	if(m_nSegmentCount > (NUM_SEGS_IN_BCAST_PLAYLIST + MEM_FILE_CACHE_DELAY)) {
		char szTsFile[256];
		char szTsFilePath[256];
		int nDelSegment = (m_nSegmentIndex - (NUM_SEGS_IN_BCAST_PLAYLIST +  MEM_FILE_CACHE_DELAY));
		GetSegmentNameFromIndex(szTsFile, m_pszM3u8BaseName, nDelSegment);
		sprintf(szTsFilePath, "%s/%s",m_pszParentFolder, szTsFile);
		unlink(szTsFilePath);
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

int CHlsPublishMemFile::ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs)
{
	int res = 0;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	m_nInStreamTime += nDurarionMs;
	if(m_CrntSegmentDuration ==  0) {
		char szTsFile[256];
		m_nSegmentIndex++;
		GetSegmentNameFromIndex(szTsFile, m_pszM3u8BaseName, m_nSegmentIndex);
		if(m_MemFile.Open(m_pszParentFolder, szTsFile) != 0){
			m_nError = HLS_UPLOAD_ERROR_XFR_FAIL;
			JDBG_LOG(CJdDbg::LVL_ERR,("Failed to open %s", szTsFile));
			goto Exit;
		}
	}

	if(m_MemFile.Write(pData, nLen) == nLen) {
		m_CrntSegmentDuration += nDurarionMs;
		m_nOutStreamTime += nDurarionMs;
	} else {
		m_nError = HLS_UPLOAD_ERROR_XFR_FAIL;
		JDBG_LOG(CJdDbg::LVL_ERR,("Failed to write"));
		m_nLostBufferTime += nDurarionMs;
		goto Exit;
	}

	if(m_CrntSegmentDuration >=  m_nSegmentDurationMs){
		m_MemFile.Close();
		m_nSegmentTime += m_CrntSegmentDuration;
		m_CrntSegmentDuration = 0;
		m_nSegmentCount++;
		UpdatePlayList();
	}
Exit:
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}

//===========================================================================
std::map<std::string, CHlsMemFileServer *> gpHlsMemFileServers;

CHlsMemFileServer *GetMemFileServerForResource(const char *pszResource)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	CHlsMemFileServer *pRes = NULL;
	std::map<std::string,CHlsMemFileServer *>::iterator it = gpHlsMemFileServers.find(pszResource);
	if(gpHlsMemFileServers.end() != it){
		pRes = (*it).second;
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return pRes;
}


/**
 * httpliveWriteData is expored to encoder to supply the data
 */
int httpliveWriteData(void *pCtx, char *pData, int nLen)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	int res = 0;
	CHlsSegmnter *pHttpLiveSgmnt = (CHlsSegmnter *)pCtx;
	if(pHttpLiveSgmnt) {
		res = pHttpLiveSgmnt->WriteData(pData, nLen);
	} else {
		JDBG_LOG(CJdDbg::LVL_ERR,("httpliveWriteData: Invalid Segmenter"));
		res =  -1;
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return res;
}

/**
 * httpliveWriteData is expored to encoder to supply the data
 */
int hlsWriteFrameData(void *pCtx, char *pData, int nLen, int fVideo, int fHasSps,  unsigned long ulPtsMs)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	int res = 0;
	CHlsSegmnter *pHttpLiveSgmnt = (CHlsSegmnter *)pCtx;
	if(pHttpLiveSgmnt) {
		res =  pHttpLiveSgmnt->WriteFrameData(pData, nLen,  fVideo, fHasSps,  ulPtsMs);
	} else {
		JDBG_LOG(CJdDbg::LVL_ERR,("httpliveWriteData: Invalid Segmenter"));
		res = -1;
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return res;
}

/**
 * hlsServerHandler : Processes requests from http live playback client.
 */
int hlsServerHandler(
		int				hSock, 
		char			*pszUri)
{
	char szResFolder[256];
	char szFilePath[256];
	char *buffer = NULL;
	/* TODO Get Based on pszUri */
	CHlsMemFileServer *pHlsMemFileServer =  NULL;

	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if (CM3u8Helper::ParseResourcePath(pszUri, szResFolder, szFilePath) != 0) {
		return 0;
	}

	pHlsMemFileServer = GetMemFileServerForResource(szResFolder);

	if(pHlsMemFileServer == NULL){
		JDBG_LOG(CJdDbg::LVL_ERR,("Segmenter not initialized\n"));
		goto Exit;
	}

	buffer = (char *)malloc(PLAYLIST_BUFFER_SIZE);
	if(!buffer) {
		JDBG_LOG(CJdDbg::LVL_ERR,("Can not allocated buffer\n"));
		goto Exit;
	}

	if(CM3u8Helper::IsHtmlFile(szFilePath)){
		char *pszHtmlaBaseName = CM3u8Helper::GetHtmlBaseName(szFilePath);
		int nLen = GetHtmlCamHttpLive(buffer, pszHtmlaBaseName, PLAYLIST_BUFFER_SIZE, 640, 480);
		SendHttpOK(hSock, nLen, STD_HEADER);
		if(pszHtmlaBaseName) free(pszHtmlaBaseName);
		if ( send(hSock, buffer, nLen, 0) < 0 ) {
			JDBG_LOG(CJdDbg::LVL_ERR,("%d Failed to write to socket. Exiting..\n", hSock));
			goto Exit;
		}
	} else if(CM3u8Helper::IsPlayListFile(szFilePath)){
		int nSeqStart = pHlsMemFileServer->m_nSeqStart - NUM_SEGS_IN_BCAST_PLAYLIST;
		if(nSeqStart < 0)
			nSeqStart = 0;
		char *pszPlaylistBaseName = CM3u8Helper::GetPlayListBaseName(szFilePath);
		int nLen = GetM3u8CamHttpLive(buffer, pszPlaylistBaseName, PLAYLIST_BUFFER_SIZE, 
					pHlsMemFileServer->m_nSegmentDuration / 1000, nSeqStart, NUM_SEGS_IN_BCAST_PLAYLIST, NULL,1, 0);
		SendHttpOK(hSock, nLen, M3U8_HEADER);
		if(pszPlaylistBaseName) free(pszPlaylistBaseName);
		if ( send(hSock, buffer, nLen, 0) < 0 ) {
			JDBG_LOG(CJdDbg::LVL_ERR,("%d Failed to write to socket. Exiting..\n", hSock));
			goto Exit;
		}
	} else if(CM3u8Helper::IsTsFile(szFilePath)){
		/* TODO: Send stream parameter derived from pszUri*/
		pHlsMemFileServer->TransferFile(hSock, szFilePath);
	} else {
		JDBG_LOG(CJdDbg::LVL_ERR,("Unknown resource..\n", pszUri));
	}
Exit:
	if(buffer)
		free(buffer);
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}

#define	MAX_FILE_NAME	 256

void CHlsPublishS3::UpLoadFullPlayList(
		char			*pSegmentBaseName,
		int				nStartSegment,
		int				nCountSegments)
{
	int nTotalLen = 0;
	int bufLen = 0;
	char *buffer = NULL;
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));

	buffer = (char *)malloc(PLAYLIST_BUFFER_SIZE);
	if(!buffer) {
		JDBG_LOG(CJdDbg::LVL_ERR,("Can not allocate buffer\n"));
		goto Exit;
	}
	{
		int nNextSegment = nStartSegment;
		int nEndSegment = nStartSegment + nCountSegments;
		int nSegmentsCreated = 0;
		int nDuration =m_nSegmentDurationMs / 1000;

		nTotalLen = GetM3u8CamHttpLive(NULL, pSegmentBaseName, PLAYLIST_MAX_SIZE, 
						nDuration, nNextSegment, nCountSegments, NULL, 1, 1);
		int nLen = m3u8AddHeader(buffer, nDuration, nStartSegment);
		m_pHlsOut->Start(m_pszParentFolder, m_pszM3u8File, nTotalLen, buffer,nLen, CONTENT_STR_M3U8);

		while(nNextSegment < nEndSegment) {
			JDBG_LOG(CJdDbg::LVL_ERR,("Playlist. nNextSegment=%d nCountSegments=%d\n",nNextSegment,nCountSegments));
			nLen = GetM3u8CamHttpLive(buffer, pSegmentBaseName, PLAYLIST_BUFFER_SIZE, 
						nDuration, nNextSegment, nCountSegments, &nSegmentsCreated, 0, 0);
			m_pHlsOut->Continue ( buffer,nLen);
			nNextSegment += nSegmentsCreated;
			nCountSegments -= nSegmentsCreated;
		}
		nLen = m3u8AddTail(buffer);
		m_pHlsOut->End(buffer,nLen);
	}
Exit:
	if(buffer)
		free(buffer);
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

void CHlsPublishS3::UpLoadBroadcastPlayList(
		char			*pSegmentBaseName,
		int				nSeqStart,
		int             nSegments)
{
	int nLen = 0;
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));

	char *buffer = (char *)malloc(PLAYLIST_BUFFER_SIZE);

	nLen = GetM3u8CamHttpLive(buffer, pSegmentBaseName, PLAYLIST_BUFFER_SIZE, 
		m_nSegmentDurationMs / 1000, nSeqStart, nSegments, NULL, 1, 0);
	m_pHlsOut->Send(m_pszParentFolder, m_pszM3u8File, buffer,nLen, CONTENT_STR_M3U8, m_nSegmentDurationMs / 1000);

Exit:
	if(buffer)
		free(buffer);
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

/**
 * HttpLiveUploadCameraHandler : Stores the data in the given folder with http put request for each segment
 * Saves the html file and playlist at the end of streaming.
 * TODO: implement termination
 */
#ifdef WIN32
DWORD WINAPI CHlsPublishS3::thrdStreamHttpLiveUpload(	void *pArg)
#else
void *CHlsPublishS3::thrdStreamHttpLiveUpload(	void *pArg)
#endif
{
	CHlsPublishS3 *pUpload = (CHlsPublishS3 *)pArg;
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	
	pUpload->Process();

	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
#ifdef WIN32
	return 0;
#else
	return (void *)0;
#endif
}

void CHlsPublishS3::Run()
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	m_pSegmenter->AddPublisher(this);
	m_fRunState = 1;
	m_nError = HLS_UPLOAD_STATE_RUN;
#ifdef WIN32
	DWORD dwThreadId;
    thrdHandle = CreateThread(NULL, 0, CHlsPublishS3::thrdStreamHttpLiveUpload, this, 0, &dwThreadId);
#else
	pthread_create(&thrdHandle, NULL, CHlsPublishS3::thrdStreamHttpLiveUpload, this);
#endif
	thrdHandle = thrdHandle;
}

void CHlsPublishS3::Stop()
{
	m_pSegmenter->RemovePublisher(this);
	m_fRunState = 0;
	m_nError = HLS_UPLOAD_STATE_STOP;
#ifdef WIN32
	WaitForSingleObject(thrdHandle, 3000);
#endif
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

DWORD CHlsPublishS3::Process()
{
	int len, lfd;
	char *ChunkStart;
	char *buffer;
	int nContentLen = 0;
	int fChunked = 0;
	int fDone = 0;
	CHlsOutBase *pHlsOut = NULL;
	pHlsOut = m_pHlsOut;
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));

	JDBG_LOG(CJdDbg::LVL_MSG,("HttpLive Request. parent url=%s m3u8=%s\n", m_pszParentFolder, m_pszM3u8File));

	char szTsFileName[MAX_FILE_NAME];
	char *pszM3u8BaseName = CM3u8Helper::GetPlayListBaseName(m_pszM3u8File);

	if(0) {
		char szHtmlFile[MAX_FILE_NAME];
		buffer = (char *)malloc(PLAYLIST_BUFFER_SIZE);
		if(buffer) {
			sprintf(szHtmlFile,"%s.html", pszM3u8BaseName);
			int nLen = GetHtmlCamHttpLive(buffer, pszM3u8BaseName, PLAYLIST_BUFFER_SIZE, 640, 480);
			int res = pHlsOut->Send(m_pszParentFolder, szHtmlFile, buffer,nLen, CONTENT_STR_HTML, 5);
			if(res != JD_OK) {
				m_nError = HLS_UPLOAD_ERROR_XFR_FAIL;
				goto Exit;
			}
			free(buffer);
		}
	}

	while(!fDone){
		int lTimeout = m_nSegmentDurationMs * 2;
		int nCrntDuration = 0;
		while(GetAvailBuffDuration() < m_nSegmentDurationMs && lTimeout > 0 && m_fRunState) {
			lTimeout -= 100;
			OSAL_WAIT(100);
		}

		if(lTimeout <= 0 || !m_fRunState){
			JDBG_LOG(CJdDbg::LVL_MSG,("Timeout(%d) while waiting for filled buffer", m_nSegmentDurationMs * 2));
			fDone = true;
			continue;
		}
		
		/* Segment Available */
		GetSegmentNameFromIndex(szTsFileName, pszM3u8BaseName, m_nSegmentIndex);
		int nTotlaLen = GetSegmentLen();
		int nBytesSent = 0;
		JDBG_LOG(CJdDbg::LVL_MSG,("m_nSegmentIndex=%d nTotlaLen=%d: numGops=%d SegDurationMs=%d nCrntDuration=%d",m_nSegmentIndex, nTotlaLen, m_pGopFilledList.size()));
		if( m_pHlsOut->Start(m_pszParentFolder,szTsFileName,nTotlaLen, NULL,0,CONTENT_STR_MP2T) == JD_ERROR){
			JDBG_LOG(CJdDbg::LVL_MSG,(":Start: Exiting due to error writing: %s", szTsFileName));
			m_nError = HLS_UPLOAD_ERROR_CONN_FAIL;
			goto Exit;
		}

		while(nCrntDuration < m_nSegmentDurationMs) {
			m_Mutex.Acquire();
			JDBG_LOG(CJdDbg::LVL_MSG,(":Uploading Segment %d: numGops=%d SegDurationMs=%d nCrntDuration=%d",  m_nSegmentIndex, m_pGopFilledList.size(), m_nSegmentDurationMs, nCrntDuration));
			
			if(m_pGopFilledList.size() <= 0) {
				JDBG_LOG(CJdDbg::LVL_MSG,("Error:Unexpected Size %d!!!",m_pGopFilledList.size()));
			}
			CGopCb *pGop = m_pGopFilledList.front();
			nBytesSent += pGop->m_nLen;
			m_pGopFilledList.pop_front();
			m_Mutex.Release();
			
			if(m_pHlsOut->Continue((char *)pGop->m_pBuff, pGop->m_nLen) == JD_ERROR) {
				JDBG_LOG(CJdDbg::LVL_MSG,(":Continue: Exiting due to error writing: %s", szTsFileName));
				m_nError = HLS_UPLOAD_ERROR_XFR_FAIL;
				goto Exit;
			}
			nCrntDuration += pGop->m_DurationMs;
			m_nOutStreamTime += pGop->m_DurationMs;
			m_Mutex.Acquire();
			m_pCb->Free(pGop->m_pBuff, pGop->m_nLen);
			m_Mutex.Release();
			delete pGop;;
		}
		m_nSegmentTime += m_nSegmentDurationMs;
		if(m_pHlsOut->End(NULL, 0) == JD_ERROR){
			JDBG_LOG(CJdDbg::LVL_MSG,(":End: Exiting due to error writing: %s", szTsFileName));
			m_nError = HLS_UPLOAD_ERROR_XFR_FAIL;
			goto Exit;
		}
		
		if(nBytesSent) {
			JDBG_LOG(CJdDbg::LVL_MSG,("Upload complete: url=%s segment=%s Bytes=%d\n", m_pszParentFolder, szTsFileName, nBytesSent));
			// Upload updated m3u8
			if(SegmentCount()  >=  NUM_SEGS_IN_BCAST_PLAYLIST) {
				/* Delay the playlist update to accommmodate s3 caching ?*/
				JDBG_LOG(CJdDbg::LVL_MSG,("Upload playlist: url=%s playlist=%s\n", m_pszParentFolder, pszM3u8BaseName));
				UpLoadBroadcastPlayList(pszM3u8BaseName, m_nSegmentIndex - NUM_SEGS_IN_BCAST_PLAYLIST + 1, NUM_SEGS_IN_BCAST_PLAYLIST);

				// Delete prev head segment for LiveOnly upload
				if(m_fLiveOnly && SegmentCount()  >  NUM_SEGS_IN_BCAST_PLAYLIST + 1/*TODO: Replace with start segment */) {
					char szOldSegmentFileName[256];
					GetSegmentNameFromIndex(szOldSegmentFileName, pszM3u8BaseName, m_nSegmentIndex - NUM_SEGS_IN_BCAST_PLAYLIST - 1);
					pHlsOut->Delete(m_pszParentFolder, szOldSegmentFileName);
				}
			}
		}
		m_nSegmentIndex++;
	}
	if(SegmentCount() > 0 && !m_fLiveOnly)
		UpLoadFullPlayList(pszM3u8BaseName, m_nSegmentStartIndex, SegmentCount());
Exit:
	if(pszM3u8BaseName)
		free(pszM3u8BaseName);
	m_fRunState = 0;
	JDBG_LOG(CJdDbg::LVL_MSG,("Exiting "));
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}

void *hlsPublishStart(
			int		    nTotalTimeMs, 
			void	    *pSegmenter, 
			const char	*pszSrcM3u8File,				///< Play list fine name
			const char	*pszParentFolder,
 			const char	*pszBucket, 
			const char	*pszHost,
 			const char	*szAccessId,
			const char	*szSecKey,
			int         fLiveOnly,
			int         nSegmentDurationMs,
			int         nDestType
			)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	CHlsPublishBase *pHlsUpload = NULL;

	if(nDestType == UPLOADER_TYPE_S3 || nDestType == UPLOADER_TYPE_DISC) {
		if(nTotalTimeMs == -1)
			nTotalTimeMs = MAX_UPLOAD_TIME;
		pHlsUpload = new CHlsPublishS3(nTotalTimeMs, pSegmenter, pszSrcM3u8File, pszParentFolder, 
			pszBucket, pszHost, szAccessId, szSecKey, fLiveOnly, nSegmentDurationMs, nDestType);
	} else {
		pHlsUpload = new CHlsPublishMemFile(pSegmenter, pszSrcM3u8File, pszBucket/*ServerRoot*/, pszParentFolder, nSegmentDurationMs);
	}

	pHlsUpload->Run();
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return pHlsUpload;
}

void hlsPublishStop(void *pUploadCtx)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	CHlsPublishBase *pHlsUpload = (CHlsPublishBase *)pUploadCtx;
	pHlsUpload->Stop();
}

int hlsPublishGetStats(void *pUploadCtx, int *pnState, int *pnStreamInTime, int *pnLostBufferTime,  int *pnStreamOutTime, int *pnSegmentTime)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	HLS_PUBLISH_STATS Stats;
	CHlsPublishBase *pHlsUpload = (CHlsPublishBase *)pUploadCtx;
	pHlsUpload->GetStats(&Stats);
	*pnState = Stats.nState;
	*pnSegmentTime = Stats.nSegmentTime;
	*pnLostBufferTime = Stats.nLostBufferTime;
	*pnStreamInTime = Stats.nStreamInTime;
	*pnStreamOutTime = Stats.nStreamOutTime;

	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	return 0;
}


void *hlsCreateMemFileServer(const char *pszResourceFolder, const char *pszFilePrefix, int nSegmentDuration, int nBitrate)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	if(nSegmentDuration < 1000)
		nSegmentDuration = 1000;
	int nSegmentSize = (int)(((float)nSegmentDuration / 1000) * (nBitrate / 8) * 2); // Bytes
	gpHlsMemFileServers[pszResourceFolder] = new CHlsMemFileServer(pszFilePrefix, nSegmentDuration, nSegmentSize);
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
	
	return gpHlsMemFileServers[pszResourceFolder];
}

void hlsDeleteMemFileServer(const char *pszResource)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	std::map<std::string,CHlsMemFileServer *>::iterator it = gpHlsMemFileServers.find(pszResource);
	if(gpHlsMemFileServers.end() != it){
		delete (*it).second;
		gpHlsMemFileServers.erase(it);
	}
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

void *hlsCreateSegmenter()
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	CHlsSegmnter *pHlsSegmenter = new CHlsSegmnter();
	return pHlsSegmenter;
}

void hlsDeleteSegmenter(void *pSegmenter)
{
	JDBG_LOG(CJdDbg::LVL_TRACE,("Enter"));
	
	CHlsSegmnter *pHlsSegmenter = (CHlsSegmnter *)pSegmenter;
	delete pHlsSegmenter;
	
	JDBG_LOG(CJdDbg::LVL_TRACE,("Leave"));
}

void hlsSetDebugLevel(int nLevel)
{
	modDbgLevel = nLevel;
}
