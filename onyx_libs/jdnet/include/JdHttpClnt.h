#ifndef __JD_HTTP_CLNT_H__
#define __JD_HTTP_CLNT_H__

typedef enum _CONTENT_TYPE_T
{
	CONTENT_TYPE_NONE,
	CONTENT_TYPE_HTML,
	CONTENT_TYPE_M3U8,
	CONTENT_TYPE_MP2T
}CONTENT_TYPE_T;

#define CONTENT_STR_TYPE			"Content-Type"
#define CONTENT_STR_HTML			"text/html"
#define CONTENT_STR_M3U8			"application/vnd.apple.mpegurl"
#define CONTENT_STR_MP2T			"application/octet-stream"
#define CONTENT_STR_DEF				"application/octet-stream"

typedef enum _AMZ_ACL_T
{
	AMZ_ACL_NONE,
	AMZ_ACL_PRIVATE,
	AMZ_ACL_PUBLIC_READ,
	AMZ_ACL_PUBLIC_READ_WRITE,
	AMZ_ACL_PUBLIC_AUTH_READ,
	AMZ_ACL_PUBLIC_OWNER_READ,
	AMZ_ACL_PUBLIC_OWNER_FULL_CTRL
}AMZ_ACL_T;

#define X_AMZ_ACL					"x-amz-acl"

#define ACL_STR_PRIVATE				"private" 
#define ACL_STR_PUBLIC_READ			"public-read"
#define ACL_STR_PUBLIC_READ_WRITE	"public-read-write"
#define ACL_STR_AUTH_READ			"authenticated-read" 
#define ACL_STR_OWNER_READ			"bucket-owner-read"
#define ACL_STR_OWNER_FULL_CTRL		"bucket-owner-full-control"

class CHttpGet
{
public:
	CHttpGet()
	{
		m_hSock = -1;
		m_contentLength = 0;
	}
	int Open(const char *pszUrl);
	int Read(char *pData, int nMaxLen);
	void Close();
	
	int	m_hSock;
	int m_contentLength;
};

class CHttpPut
{
public:
	CHttpPut()
	{
		m_hSock = -1;
		m_contentLength = 0;
		m_nAmzAcl = AMZ_ACL_NONE;
		m_nContentType = CONTENT_TYPE_NONE;
	}
	CHttpPut(int nContentType, int nAmzAcl)
	{
		m_hSock = -1;
		m_contentLength = 0;
	}

	virtual int Open(const char *pszUrl, int nContentLen);
	virtual int Write(char *pData, int nLen);
	virtual int Write(char *pData, int nLen, int nTimeOut);
	virtual int GetStatus();
	virtual void Close();

private:
	int GetS3Headers(char *pData, int nMaxLen);

public:
	int	m_hSock;
	int m_contentLength;
	int m_nAmzAcl;
	int m_nContentType;
};

void *httpputStart(char *pszHost, char *pszResource, int nTotalLen, char *pData, int nLen);
int httpputAdd(void *pCtx, char *pData, int nLen);
int httpputEnd(void *pCtx, char *pData, int nLen);

int HttpPutResource(
			char *pszHost, 
			char *pszResource, 
			char *pData, 
			int nLen, 
			int nTimeOut,
			int nContentType=CONTENT_TYPE_NONE, 
			int nAmzAcl=AMZ_ACL_NONE);
int HttpGetResource(char *pszHost, char *pszResource, char *pData, int nMaxLen);
#endif //__JD_HTTP_CLNT_H__