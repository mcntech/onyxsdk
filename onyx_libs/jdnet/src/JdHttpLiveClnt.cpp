#include <assert.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#define read	_read
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
#endif
#include <stdlib.h>
#include <stdio.h>

#include "JdHttpLiveClnt.h"
#include "JdOsal.h"

#ifndef _WIN32
	#define strnicmp strncasecmp
#endif

//"#EXT-X-DISCONTINUITY\r\n"
											// where x is file nummber

#define TAG_EXTINF				"#EXTINF"
#define TAG_EXTM3U				"#EXTM3U"
#define TAG_ALLOW_CACHE			"#EXT-X-ALLOW-CACHE"
#define TAG_TARGETDURATION		"#EXT-X-TARGETDURATION"
#define TAG_MEDIA_SEQUENCE		"#EXT-X-MEDIA-SEQUENCE"
#define TAG_KEY					"#EXT-X-KEY"
#define TAG_PROGRAM_DATE_TIME	"#EXT-X-PROGRAM-DATE-TIME"
#define TAG_STREAM_INF			"#EXT-X-STREAM-INF"
#define TAG_ENDLIST				"#EXT-X-ENDLIST"

static int msglevel = 1;
#define PREFIX "HlsClnt"
#define MAX_M3U8_BUFF	(16* 1024)

#ifdef WIN32
extern "C" void DbgLog(int Level, char *szError,...)
{
	if(Level > msglevel)
		return;
	char	szBuff[256];
    va_list vl;
    va_start(vl, szError);
	sprintf(szBuff, PREFIX);
    vsprintf(szBuff + strlen(PREFIX), szError, vl);
    strcat(szBuff, "\r\n");
    OutputDebugString(szBuff);
	va_end(vl);
}
#else
//TODO
extern "C" void DbgLog(int Level, char *szError,...)
{

}
#endif

/*
 * Move the char pointer to next field 
 * in comma separated format.
 * Skip CR,LF, tab and space
 */
const char *NextField(const char *pData )
{
	/* Looks for comma */
	const char *pIn = strchr( pIn, ',' );
	if( pData == NULL )
		return NULL; /* No more transport options */

	pIn++; /* skips comma */
	while( strchr( "\r\n\t ", *pIn ) )
		pIn++;

	return (*pIn) ? pIn : NULL;
}

/*
 * Move the char pointer to next field 
 * in comma separated format.
 * Skip CR,LF, tab and space
 */
int GetNextField(const char *pData, char **ppField, int nDelimiter )
{
	/* Looks for comma */
	int nLen = 0;
	char *pszTmp = NULL;
	const char *pFiledStart;
	*ppField = NULL;
	const char *pIn = pData;

	if(pIn){
		pFiledStart = strchr(pIn, nDelimiter);
		if(pFiledStart ==  NULL)
			return 0;
		pFiledStart++;	// Skip delimiter

		/* Skip Any preceeding spaces */
		while(*pFiledStart != 0 && *pFiledStart == ' ')
			pFiledStart++;

		pIn = pFiledStart;
		while(*pIn != 0 && !strchr( "\r\n\t,", *pIn))
			pIn++;
		nLen = pIn - pFiledStart;
		if(nLen){
			pszTmp = (char *) malloc(nLen+1);
			memcpy(pszTmp, pFiledStart, nLen);
			pszTmp[nLen] = 0;
		}
	}
	*ppField = pszTmp;
	return pIn - pData;
}

/*
 * Move char pointerto the next line 
 */
static const char *NextLine(const char *pData )
{
	int nLen = strlen(pData);
	if(!nLen)
		return NULL;

	// Look for CR or LF
	while((*pData != '\r' && *pData != '\n') && nLen ){
		pData++;
		nLen--;
	}
	// Skip CR. LF
	while((*pData == '\r' || *pData == '\n') && nLen ){
		pData++;
		nLen--;
	}

	return (*pData) ? pData : NULL;
}

/*
 * Move char pointerto the next line 
 */
static char *GetLine(const char *pData )
{
	char *pszTmp = NULL;
	const char *pIn = pData;
	if(*pIn == 0)	// End of line
		return NULL;

	// Look for CR or LF
	while(*pIn && *pIn != '\r' && *pIn != '\n'){
		pIn++;
	}
	if(*pIn != 0) {
		int nLen = pIn - pData;
		if(nLen > 0){
			pszTmp = (char *) malloc(nLen+1);
			memcpy(pszTmp, pData, nLen);
			pszTmp[nLen] = 0;
		}
	}
	return pszTmp;
}

int CM3u8Segment::Parse(const char *pData)
{
	const char *pIn = pData;
	if(pIn) {
		char *pszTmp;
		int nLen = GetNextField(pIn, &pszTmp, ':');
		if(nLen) {
			m_nDuration = atoi(pszTmp);
			free(pszTmp);
		}
		pIn += nLen;
		nLen = GetNextField(pIn, &m_pszDescript, ',');
		pIn = NextLine(pIn);
		if(pIn){
			m_pszUrl = GetLine(pIn);
		}
	}
	return 0;
}
CHttpLiveGet::CHttpLiveGet()
{
	m_pSegmentList = NULL;
	m_nNumSegments = 0;
	m_nCrntSegment = 0;
	m_pHttpGet = NULL;
	m_nMediaSequence = 0;
	m_nTargetDuration = 0;
	m_nSeqNum = 0;
	m_nPrevSeqNum = -1;
	m_fAllowCaching = 0;
	m_fSlidingWindow = 0;
	m_pszDateAndTime = NULL;
	m_pszKeyFile = NULL;
	m_pszUrl = NULL;
	m_pDataM3u8 = (char *)malloc(MAX_M3U8_BUFF);;
}
CHttpLiveGet::~CHttpLiveGet()
{
	FreeResources();
	if(m_pDataM3u8)
		free(m_pDataM3u8);
	if(m_pszUrl)
		free(m_pszUrl);
}

void CHttpLiveGet::AddSegment(CM3u8Segment *pSegment)
{
	m_pSegmentList = (CM3u8Segment **)
			realloc(m_pSegmentList, (m_nNumSegments + 1) * sizeof(CM3u8Segment *));
	m_pSegmentList[m_nNumSegments] = pSegment;
	m_nNumSegments++;
}

int CHttpLiveGet::Open(const char *pszUrl)
{
	m_pszUrl = strdup(pszUrl);
	return UpdatePlayList();
}

int CHttpLiveGet::Read(char *pData, int nMaxLen)
{
	int nBytesRead = 0;
	CM3u8Segment *pCrntSegment = GetSegment(m_nCrntSegment);
	if(!pCrntSegment){
		DbgLog(0, "No Segment available");
		return -1;
	}
	if(!m_pHttpGet) {
		m_pHttpGet = new CHttpGet;
		char *pszUrl = GetSegmentAbsUrl(pCrntSegment->m_pszUrl);
		DbgLog(1, "Start New Segment seq=%d %s", m_nSeqNum, pCrntSegment->m_pszUrl);
		if(pszUrl){
			m_pHttpGet->Open(pszUrl);
			free(pszUrl);
		}
	}
	if(m_pHttpGet){
		nBytesRead =  m_pHttpGet->Read(pData, nMaxLen);
		while(nBytesRead <= 0 && m_pHttpGet){
			/* End of segment */
			DbgLog(1, "End of segment seq=%d %s",m_nSeqNum, pCrntSegment->m_pszUrl);
			delete m_pHttpGet;
			m_pHttpGet = NULL;
			if(m_fSlidingWindow){
				UpdatePlayList();
				m_nCrntSegment = 0;
			} else {
				m_nCrntSegment++;
			}
			pCrntSegment = GetSegment(m_nCrntSegment);
			if(pCrntSegment){
				m_pHttpGet = new CHttpGet;
				char *pszUrl = GetSegmentAbsUrl(pCrntSegment->m_pszUrl);
				DbgLog(1, "Start New Segment seq=%d  %s",m_nSeqNum, pCrntSegment->m_pszUrl);
				if(pszUrl){
					m_pHttpGet->Open(pszUrl);
					free(pszUrl);
				}
			}
			if(m_pHttpGet) {
				nBytesRead = m_pHttpGet->Read(pData, nMaxLen);
				if(nBytesRead <= 0) {
					DbgLog(0, "Read failed from %s",pCrntSegment->m_pszUrl);
				}
			}
		}
	}
Exit:
	return nBytesRead;
}

void CHttpLiveGet::Close()
{

}

int CHttpLiveGet::UpdatePlayList()
{
	int res = 0;
	CHttpGet  HttpGet;
	FreeResources();
	int fDone = 0;
	char *pData = m_pDataM3u8;
	while(!fDone){
		if(HttpGet.Open(m_pszUrl) < 0) {
			res = -1;
			DbgLog(1, "Failed to open %s.",m_pszUrl);
			goto Exit;
		}
		HttpGet.Read(pData, MAX_M3U8_BUFF);
		//TODO: free current play list
		Parse(pData);
		if(m_nSeqNum == m_nPrevSeqNum){
			DbgLog(1, "wiating for m_nSeqNum(%d) to increment",m_nSeqNum);
			JD_OAL_SLEEP(100)
			continue;
		}
		fDone = 1;
	}
	m_nPrevSeqNum = m_nSeqNum;
Exit:
	return res;
}


int CHttpLiveGet::Parse(const char *pData)
{
	const char *pIn = pData;
	
	m_fSlidingWindow = 1;	// Default

	while(pIn = NextLine(pIn)){
		if (strnicmp(pIn,TAG_EXTINF, strlen(TAG_EXTINF)) == 0){
			CM3u8Segment *pM3u8Segment = new CM3u8Segment;
			int nbytesConsumed  = pM3u8Segment->Parse(pIn);
			AddSegment(pM3u8Segment);
			pIn += nbytesConsumed;
		} else if(strnicmp(pIn,TAG_EXTM3U, strlen(TAG_EXTM3U)) == 0){
			// Do nothing
		} else if(strnicmp(pIn,TAG_MEDIA_SEQUENCE, strlen(TAG_MEDIA_SEQUENCE)) == 0){
			sscanf(pIn,TAG_MEDIA_SEQUENCE ":%d", &m_nSeqNum);
			if(m_nSeqNum == m_nPrevSeqNum)
				return 0;
		} else if(strnicmp(pIn,TAG_ALLOW_CACHE, strlen(TAG_ALLOW_CACHE)) == 0){
			
		} else if(strnicmp(pIn,TAG_TARGETDURATION, strlen(TAG_TARGETDURATION)) == 0){
			sscanf(pIn,TAG_TARGETDURATION ":%d", &m_nTargetDuration);
		} else if(strnicmp(pIn,TAG_KEY, strlen(TAG_KEY)) == 0){
			// TODO
		} else if(strnicmp(pIn,TAG_PROGRAM_DATE_TIME, strlen(TAG_PROGRAM_DATE_TIME)) == 0){
			// TODO
		} else if(strnicmp(pIn,TAG_STREAM_INF, strlen(TAG_STREAM_INF)) == 0){
			// TODO
		} else if(strnicmp(pIn,TAG_ENDLIST, strlen(TAG_ENDLIST)) == 0){
			m_fSlidingWindow = 0;
		} else {
			// Uknown Tag or empty line
		}
	}
	return 0;
}

CM3u8Segment *CHttpLiveGet::GetSegment(int nSegment)
{
	if(nSegment < m_nNumSegments - 1)
		return m_pSegmentList[nSegment];
	return NULL;
}

char *CHttpLiveGet::GetSegmentAbsUrl(char *pszSegmentRelUrl)
{
	if(strncmp(pszSegmentRelUrl,"http://",strlen("http://")) == 0){
		return strdup(pszSegmentRelUrl);
	} else if(m_pszUrl != NULL) {
		char *pszAbsUrl = NULL;
		const char *pIdx = m_pszUrl + strlen(m_pszUrl);
		while(pIdx > m_pszUrl && *(pIdx - 1) != '/')
			pIdx--;

		int nLenParenUrl = pIdx - m_pszUrl;
		if(nLenParenUrl > 0) {
			int nLenAbsUrl = nLenParenUrl + strlen(pszSegmentRelUrl);
			pszAbsUrl = (char *)malloc(nLenAbsUrl + 1);
			memcpy(pszAbsUrl,m_pszUrl,nLenParenUrl);
			memcpy(pszAbsUrl + nLenParenUrl, pszSegmentRelUrl, strlen(pszSegmentRelUrl));
			pszAbsUrl[nLenAbsUrl] = 0x00;
			return pszAbsUrl;
		}
	}
	return NULL;
}
void CHttpLiveGet::FreeResources()
{
	if(m_pSegmentList) {
		for (int i=0; i < m_nNumSegments; i++){
			delete(m_pSegmentList[i]);
		}
		free(m_pSegmentList);
		m_pSegmentList = NULL;
		m_nNumSegments = 0;
	}
	if(m_pHttpGet){
		delete m_pHttpGet;
		m_pHttpGet = NULL;
	}
	if(m_pszDateAndTime) {
		free(m_pszDateAndTime);
		m_pszDateAndTime = NULL;
	}
	if(m_pszKeyFile) {
		free(m_pszKeyFile);
		m_pszKeyFile = NULL;
	}
}