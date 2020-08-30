#ifndef __JD_HTTP_LIVE_SGMT_H__
#define __JD_HTTP_LIVE_SGMT_H__

#define PROTOCOL_FILE            "file:///"
#define PROTOCOL_HTTP            "http://"

#define AWS_DOMAIN                "amazonaws.com"
#define S3_AWS_SUB_DOMAIN         "s3.amazonaws.com"

#define MAX_UPLOAD_TIME			(3600 * 1000)	// 1 hour
#define DEF_SEGMENT_DURATION	4000

#define UPLOADER_TYPE_S3			1
#define UPLOADER_TYPE_DISC			2
#define UPLOADER_TYPE_MEM			3

#define HLS_UPLOAD_UNKNOWN_STATE		0
#define HLS_UPLOAD_STATE_STOP			1
#define HLS_UPLOAD_STATE_RUN			2
#define HLS_UPLOAD_ERROR_CONN_FAIL		0x80000001
#define HLS_UPLOAD_ERROR_XFR_FAIL		0x80000001
#define HLS_UPLOAD_ERROR_INPUT_TIMEOUT  0x80000002

/**
 * httpliveWriteData is used to supply data to the segmenter
 */
int httpliveWriteData(void *pCtx, char *pData, int nLen);

int hlsWriteFrameData(void *pCtx, char *pData, int nLen, int fVideo, int fHasSps,  unsigned long ulPtsMs);

/**
 * hlsServerHandler called for any resource request from the cleint.
 */
int hlsServerHandler(int hSock, char *pszUri);

void *hlsPublishStart(
		int		    nTotalTimeMs,				///< Total time of stream
		void	    *pSegmenter,				///< Segmenter to be used
		const char	*pszSrcM3u8Url,             ///< Play list file name
		const char	*pszDestParentUrl,			///< Network folder name
		const char	*pszBucket, 
		const char	*pszHost,
		const char	*szAccessId,
		const char	*szSecKey,
		int         fLiveOnly,
		int         nSegmentDurationMs,
		int         nDestType
		);		

void hlsPublishStop(void *pUploadCtx);
int hlsPublishGetStats(void *pUploadCtx, int *pnState, int *pnStreamInTime, int *nLostBufferTime,  int *pnStreamOutTime, int *nSegmentsTime);

void hlsSetDebugLevel(int nLevel);

void *hlsCreateSegmenter();

void hlsDeleteSegmenter(void *pSegmenter);

class CM3u8Helper
{
public:
	static int IsPlayListFile(const char *pszFile)
	{
		return (strstr(pszFile,".m3u8") != NULL);
	}

	static int IsHtmlFile(const char *pszFile)
	{
		return (strstr(pszFile,".html") != NULL);
	}

	static int IsTsFile(const char *pszFile)
	{
		return (strstr(pszFile,".ts") != NULL);
	}

	static char *GetPlayListBaseName(const char *pszFile)
	{
		const char *pIdx  = strstr(pszFile,".m3u8");
		if(pIdx) {
			int nLen = pIdx - pszFile;
			if (nLen > 0) {
				char *pszBase = (char *)malloc(nLen + 1);
				memcpy(pszBase, pszFile, nLen);
				pszBase[nLen] = 0x00;
				return pszBase;
			}
		}
		return NULL;
	}

	static char *GetHtmlBaseName(const char *pszFile)
	{
		const char *pIdx  = strstr(pszFile,".html");
		if(pIdx) {
			int nLen = pIdx - pszFile;
			if (nLen > 0) {
				char *pszBase = (char *)malloc(nLen + 1);
				memcpy(pszBase, pszFile, nLen);
				pszBase[nLen] = 0x00;
				return pszBase;
			}
		}
		return NULL;
	}

	/** 
	 * Returns file index on succes else returns -1 
	 */
	static int GetStrmAndIndexForSegment(char *pszFile, char *pszPrefix)
	{
		int nFileIdx = -1;
		int nStrmId = 0;
		char *pIdx = strstr(pszFile, pszPrefix);
		if(pIdx) {
			pIdx += strlen(pszPrefix);
			return(atoi(pIdx));
		}
		return -1;
	}
	static int ParseResourcePath(char *pszResPath, char *pszFolder, char *pszFile)
	{
		int res = -1;
		char *pTmp = pszResPath;
		char *pResFolder;
		if(pszResPath[0] == '/') {
			pTmp++;
		}
		pResFolder = pTmp;
		if(pResFolder) {
			int nResFolderSize = 0;
			char *pTmpPrev = NULL;
			// Find start of resource	
			while(pTmp = strstr(pTmp, "/")){
				pTmp++;
				pTmpPrev = pTmp;
			}
				
			if(pTmpPrev) {
				int nResFolderSize = pTmpPrev - pResFolder;	
				strcpy(pszFile, pTmpPrev);
				if(nResFolderSize > 1) {
					pszFolder[0] = '/';
					memcpy(pszFolder + 1, pResFolder, nResFolderSize);
					pszFolder[nResFolderSize] = 0;
					res = 0;
				}
			}
		}
		return res;
	}
};

#endif //__JD_HTTP_LIVE_SGMT_H__