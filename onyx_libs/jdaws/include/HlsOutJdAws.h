#ifndef __HLSOUT_JDAWS_H__
#define __HLSOUT_JDAWS_H__
#include <sstream>
#include "HlsOutBase.h"
#include "JdAwsS3UpnpHttpConnection.h"

class CHlsOutJdS3 : public CHlsOutBase
{
public:
	CHlsOutJdS3(const char *pszCfgFile);
	CHlsOutJdS3( const char *pszBucket, const char *pszHost, const char *pszAccessId, const char *pszSecKey);
	int Start(const char *pszParent, const char *pszFile, int nTotalLen, char *pData, int nLen, const char *pContentType);
	int Continue(char *pData, int nLen);
	int End(char *pData, int nLen);
	int Send(const char *pszParent, const char *pszFile, char *pData, int nLen, const char *pContentType, int nTimeOut);
	int Delete(const char *pszParentUrl, const char *pszFile);

private:
	std::string   m_Bucket;
	CJdAwsContext m_AwsContext;
	CJdAwsS3HttpResponse m_SessionContext;
};

#endif //__HLSOUT_JDAWS_H__