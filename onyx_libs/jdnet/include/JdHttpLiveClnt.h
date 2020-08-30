#ifndef __JD_HTTP_LIVE_CLNT__
#define __JD_HTTP_LIVE_CLNT__
#include "JdHttpClnt.h"

class CM3u8Segment
{
public:
	int Parse(const char *pData);
	char	*m_pszUrl;
	char	*m_pszDescript;
	int		m_nDuration;
};

class CHttpLiveGet
{
public:
	CHttpLiveGet();
	~CHttpLiveGet();
	int Open(const char *pszUrl);
	int Read(char *pData, int nMaxLen);
	void Close();

	int UpdatePlayList();
	int Parse(const char *pData);
	CM3u8Segment *GetSegment(int nSegment);

	void AddSegment(CM3u8Segment *pSegment);
	void FreeResources();
	char *GetSegmentAbsUrl(char *pszSegmentRelUrl);

	CM3u8Segment	**m_pSegmentList;
	int				m_nNumSegments;
	int				m_nCrntSegment;
	CHttpGet		*m_pHttpGet;
	int				m_nMediaSequence;
	int				m_nTargetDuration;
	int				m_fAllowCaching;
	int				m_fSlidingWindow;
	int				m_nSeqNum;
	int				m_nPrevSeqNum;

	char			*m_pszDateAndTime;
	unsigned char	*m_pszKeyFile;
	char		    *m_pszUrl;
	char            *m_pDataM3u8;
};

#endif //__JD_HTTP_LIVE_CLNT__