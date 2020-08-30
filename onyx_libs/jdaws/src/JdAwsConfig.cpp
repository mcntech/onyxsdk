#ifdef WIN32
#include <winsock2.h>
#include <io.h>
#define close	   closesocket
#define snprintf	_snprintf
#define write	   _write
#define open	   _open
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#define O_BINARY	0
#endif

#include <fcntl.h>


#include <stdio.h>
#include "JdAwsConfig.h"

int CJdAwsConfig::GetField(char *pszData, const char *pszField, std::string &strVal)
{
	const char *p= strstr(pszData, pszField);
	if(p){
		p += strlen(pszField) + 1;
		const char *q = p;
		while (*q != '\r' && *q != '\n' && *q != '\t' && *q != ' ')
			q++;
		if(q > p )
		strVal.assign(p, q-p);
		return 0;
	}
	return -1;
}

CJdAwsConfig::CJdAwsConfig(const char *pszCfgFile) 
{
	char Buff[1024]={0};
	int fd = open(pszCfgFile, O_RDONLY);
	if(fd > 0) {
		int nLen = read(fd, Buff, 1024);
		if(nLen > 0){
			GetField(Buff, "M3u8File", m_M3u8File);
			GetField(Buff, "Bucket", m_Bucket);
			GetField(Buff, "SecKey", m_SecKey);
			GetField(Buff, "AccessId", m_AccessId);
			GetField(Buff, "Host", m_Host);
			GetField(Buff, "Folder", m_Folder);
		}
	}
}
