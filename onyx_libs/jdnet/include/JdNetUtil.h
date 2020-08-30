#ifndef __JD_NET_UTIL__
#define __JD_NET_UTIL__
#define DEFAULT_READ_TIMEOUT	30		/* Seconds to wait before giving up */

int ReadHeader(int sock, char *headerPtr, int timeout = DEFAULT_READ_TIMEOUT);
char *GetParentFolderForRtspResource(char *szResource);
char *GetFileNameForDrbResource(const char *szResource);
char *GetFilepathForRtspResource(const char *pszRootFolder, char *szResource);

#endif //__JD_NET_UTIL__