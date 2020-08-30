#ifndef __JD_SDP_H__
#define __JD_SDP_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#ifdef WIN32
#include <winsock2.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#define read	_read

#pragma warning(disable : 4996)

#else
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#endif
#ifndef _WIN32
	#define strnicmp strncasecmp
//	#define _stricmp strcasecmp
#endif

/**
 * Standard SDP Attribes
 */
#define ATRRIB_FRAME_RATE	"framerate"		//a=framerate:<frame rate>
#define ATRRIB_QUALITY		"quality"		//a=quality:<quality>
#define ATRRIB_FMTP			"fmtp"			//a=fmtp:<format> <format specific parameters>
#define ATRRIB_RTPMAP		"rtpmap"		// a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]

/**
 * RFC3065 Extension SDP Attribes
 */
#define RTCP_PORT			"rtcp"			//a=rtcp:<rtcp port>

/**
 * MCN Priprietary SDP Attribes
 * Used only with DRB protocol
 */
#define LAN_RTP_PORT		"lanrtp"		//a=lanrtp:<lan rtp port>
#define LAN_RTCP_PORT		"lanrtcp"		//a=lanrtcp:<lan rtp port>


#define CHK_FREE(x) if(x) free(x);

/**
 * Look for CR and LF and skip
 * Allow a condition where only CR or LF may present
 */
class CSdpParse
{
public:
	static const char *NextSdpField(const char *pData )
	{
		int nLen = strlen(pData);
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

	static int GetFieldLen(const char *pData )
	{
		int nLen = strlen(pData);
		const char *pCrLf = pData;
		while( strchr( "\r\n", *pCrLf)==0 && nLen){
			pCrLf++;
			nLen--;
		}
		return (pCrLf - pData);
	}
};
class COrigin
{
public:
	COrigin(
		const char *pszUserName, 
		unsigned long ulSessId,
		unsigned long ulSessVersion,
		char *pszaddr) 
		: m_pszUserName(strdup(pszUserName)), 
			m_ulSessId(ulSessId),
			m_ulSessVersion(ulSessVersion),
			m_pszAddr(strdup(pszaddr))
	{
	}
	COrigin(const char *pData):
			m_pszUserName(NULL), 
			m_ulSessId(0),
			m_ulSessVersion(0),
			m_pszAddr(NULL)
	{
		Parse(pData);
	}
	int Parse(const char *pData)
	{
		pData;
		return 0;
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszUserName) {
			sprintf(pOut,"o=%s %u %u IN IP4 %s\r\n",m_pszUserName, m_ulSessId, m_ulSessVersion,  m_pszAddr);
			len = strlen(pOut);
			pOut += len;
		}
		return pOut - pData;
	}
	char *m_pszUserName;
	unsigned long m_ulSessId;
	unsigned long m_ulSessVersion;
	char *m_pszAddr;
};

class CConnection
{
public:
	CConnection(	
		char *pszNetType,
		char *pszAddrType,
		char *pszAddress): 
			m_pszNetType(strdup(pszNetType)),
			m_pszAddrType(strdup(pszAddrType)), 
			m_pszAddress(strdup(pszAddress))

	{

	}
	CConnection(const char *pData)
	{
		m_pszNetType = NULL;
		m_pszAddrType = NULL;
		m_pszAddress = NULL;

		Parse(pData);
	}

	~CConnection()
	{
		if(m_pszNetType)
			free(m_pszNetType);
		if(m_pszAddrType)
			free(m_pszAddrType);
		if(m_pszAddress)
			free(m_pszAddress);

	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszAddress) {
			sprintf(pOut,"c=%s %s %s\r\n",m_pszNetType, m_pszAddrType, m_pszAddress);
			len = strlen(pOut);
			pOut += len;
		}
		return pOut - pData;
	}
	int Parse(const char *pData)
	{
		const char *pIn = pData;
		int nMaxLen = CSdpParse::GetFieldLen(pIn);
		m_pszAddrType = (char *) malloc(nMaxLen);
		m_pszAddress = (char *)malloc(nMaxLen);
		m_pszNetType = (char *)malloc(nMaxLen);
		sscanf(pIn,"c=%s %s %s\r\n",m_pszNetType, m_pszAddrType, m_pszAddress);
		return 0;
	}

	char *m_pszNetType;
	char *m_pszAddrType;
	char *m_pszAddress;
};

class CBandWitdh
{
public:
	CBandWitdh(
		char *pszType,
		long lVlaue):
		m_pszType(strdup(pszType)),
		m_lValue(lVlaue)
	{

	}

	CBandWitdh(const char *pData)
	{
		m_pszType = NULL;
		m_lValue = 0;
		Parse(pData);
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszType) {
			sprintf(pOut,"b=%s:%d\r\n",m_pszType, m_lValue);
			len = strlen(pOut);
			pOut += len;
		}
		return pOut - pData;
	}
	int Parse(const char *pData)
	{
		const char *pIn = pData;
		pIn += strlen("b=");
		const char *Idx = strchr(pIn, ':');
		int nLen = Idx - pIn;
		if(nLen > 0) {
			m_pszType = (char *)malloc(nLen + 1);
			strncpy(m_pszType, pIn, nLen);
			m_pszType[nLen] = 0;
			pIn += nLen;
			sscanf(pIn,":%d",&m_lValue);
		}
		return 0;
	}

	char *m_pszType;
	long m_lValue;
};


typedef struct _MEDIA_T
{
	char *pMedia;
	int nPort;
	char **nProtoList;
} MEDIA_T;


class CAttribute
{
public:
	CAttribute(	
		const char *pszAttrib,
		const char *pszValue):
		m_pszAttrib(strdup(pszAttrib)),
		m_pszValue(strdup(pszValue))
	{
	}
	CAttribute(const char *pData)
	{
		m_pszAttrib = NULL;
		m_pszValue = NULL;
		Parse(pData);
	}

	~CAttribute()
	{
		CHK_FREE(m_pszAttrib)
		CHK_FREE(m_pszValue)
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszAttrib) {
			sprintf(pOut,"a=%s:%s\r\n",m_pszAttrib, m_pszValue);
			len = strlen(pOut);
			pOut += len;
		}
		return pOut - pData;
	}

	int Parse(const char *pData)
	{
		const char *pIn = pData;
		pIn += strlen("a=");
		const char *Idx = strchr(pIn, ':');
		int nLen = Idx - pIn;
		if(nLen > 0) {
			m_pszAttrib = (char *)malloc(nLen + 1);
			strncpy(m_pszAttrib, pIn, nLen);
			m_pszAttrib[nLen] = 0;
			pIn += (nLen + 1);
			Idx = strstr(pIn, "\r\n");
			nLen = Idx - pIn;
			if(nLen) {
				m_pszValue = (char *)malloc(nLen + 1);
				strncpy(m_pszValue, pIn, nLen);
				m_pszValue[nLen] = 0;
			}
		}
		return 0;
	}

	char *m_pszAttrib;
	char *m_pszValue;
};


class CMediaDescription
{
public:
	CMediaDescription(
		char				*pszMedia,
		unsigned short		usPort,
		unsigned char		ucPl,
		char				*pszInformation
		)
	{
		Reset();
		m_pszMedia = strdup(pszMedia);
		m_usPort = usPort;
		m_ucPl = ucPl;
		m_pszInformation = strdup(pszInformation);;
	}
	CMediaDescription(const char *pData)
	{
		Reset();
		Parse(pData);
	}
	void Reset()
	{
		m_usPort = 0;
		m_ucPl = 0;
		m_pszInformation = NULL;

		m_listFormats = NULL;
		m_NumFormats = 0;

		m_listConnections = NULL;
		m_NumConnections = 0;
		m_listAttributes = NULL;
		m_NumAttributes = 0;
		m_listBandwidths = NULL;
		m_NumBandwidths = 0;
	}

	~CMediaDescription()
	{
		CHK_FREE(m_pszMedia)
		CHK_FREE(m_pszInformation)
		for(int i=0; i < m_NumConnections;i++)
			delete m_listConnections[i];
		CHK_FREE(m_listConnections)

		for(int i=0; i < m_NumAttributes;i++)
			delete m_listAttributes[i];
		CHK_FREE(m_listAttributes)

	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		int i;
		if(m_pszMedia) {
			sprintf(pOut,"m=%s %u RTP/AVP %u",m_pszMedia, m_usPort, m_ucPl);
			len = strlen(pOut);
			pOut += len;
		} else {
			return 0;
		}

		for (i=0; i < m_NumFormats; i++){
			sprintf(pOut,"%s ",m_listFormats[i]);
			len = strlen(pOut);
			pOut += len;
		}
		
		sprintf(pOut,"\r\n");
		len = strlen(pOut);
		pOut += len;
		if(m_pszInformation) {
			sprintf(pOut,"i=%s\r\n",m_pszInformation);
			len = strlen(pOut);
			pOut += len;
		}
		for (i=0; i < m_NumConnections; i++){
			m_listConnections[i]->Encode(pOut);
			len = strlen(pOut);
			pOut += len;
		}
		for (i=0; i < m_NumBandwidths; i++){
			m_listBandwidths[i]->Encode(pOut);
			len = strlen(pOut);
			pOut += len;
		}

		for (i=0; i < m_NumAttributes; i++){
			m_listAttributes[i]->Encode(pOut);
			len = strlen(pOut);
			pOut += len;
		}

		return pOut - pData;
	}
	int Parse(const char *pData)
	{
		int nElems = 0;
		const char *pIn = pData;
		int nLen = CSdpParse::GetFieldLen(pIn);
		m_pszMedia = (char *)malloc(nLen);
		nElems = sscanf(pIn,"m=%s %u RTP/AVP %u",m_pszMedia, &m_usPort, &m_ucPl);
		if(nElems != 3)
			goto Exit;

		// TODO: Parse formats		
		while(pIn = CSdpParse::NextSdpField(pIn)){
			if(strncmp(pIn,"c=", strlen("c=")) == 0){
				CConnection *pConnection= new CConnection(pIn);
				AddConnection(pConnection);

			} else if(strncmp(pIn,"i=",strlen("i=")) == 0){
				const char *pStart = pIn+ strlen("i=");
				const char *pCrLf = strchr(pIn,'\r');
				int nLen = pCrLf - pStart;
				if(nLen > 0 && nLen < 1024){
					m_pszInformation = (char *)malloc(nLen + 1);
					strncpy(m_pszInformation, pStart, nLen);
					m_pszInformation[nLen] = 0;
				}
			} else if(strncmp(pIn,"a=",strlen("a=")) == 0){
				CAttribute *pAttribute = new CAttribute(pIn);
				AddAttribute(pAttribute);
			} else if(strncmp(pIn,"b=",strlen("b=")) == 0){
				CBandWitdh *pBandwith = new CBandWitdh(pIn);
				AddBandwidth(pBandwith);
			} else if(strncmp(pIn,"m=",strlen("m=")) == 0){
				// Next Media descriptor
				break;
			}
		}

		Exit:
			return 0;
	}

	void AddConnection(CConnection *pConnect)
	{
		m_listConnections = (CConnection **)
				realloc(m_listConnections, (m_NumConnections + 1) * sizeof(CConnection *));
		m_listConnections[m_NumConnections] = pConnect;
		m_NumConnections++;
	}

	void AddAttribute(CAttribute *pAttribute)
	{
		m_listAttributes = (CAttribute **)
				realloc(m_listAttributes, (m_NumAttributes + 1) * sizeof(CAttribute *));
		m_listAttributes[m_NumAttributes] = pAttribute;
		m_NumAttributes++;
	}
	void AddBandwidth(CBandWitdh *pBandwidth)
	{
		m_listBandwidths = (CBandWitdh **)
				realloc(m_listBandwidths, (m_NumBandwidths + 1) * sizeof(CBandWitdh *));
		m_listBandwidths[m_NumBandwidths] = pBandwidth;
		m_NumBandwidths++;
	}
	long GetBandwidthValue(const char *pszType)
	{
		for (int i = 0; i < m_NumBandwidths; i++){
			CBandWitdh *pBandWidth = m_listBandwidths[i];
			if(strcmp(pBandWidth->m_pszType, pszType) == 0)
				return pBandWidth->m_lValue;
		}
		return 0;;

	}


	const char *GetAttributeValue(const char *pszType, int nPos)
	{
		int tmpPos = 0;
		for (int i = 0; i < m_NumAttributes; i++){
			CAttribute *pAttribute = m_listAttributes[i];
			if(strcmp(pAttribute->m_pszAttrib, pszType) == 0 ){
				if( nPos == tmpPos) 
					return pAttribute->m_pszValue;
				else
					tmpPos++;
			}
		}
		return NULL;;
	}
	
	// Utility functions
	int GetControl(std::string &url)
	{
		for (int i=0; i < m_NumAttributes; i++){
			if (strnicmp(m_listAttributes[i]->m_pszAttrib, "Control", strlen("Control")) == 0) {
				url = m_listAttributes[i]->m_pszValue;
				return 1;
			}
		}
		return 0;
	}

	char				*m_pszMedia;
	unsigned short		m_usPort;
	unsigned char		m_ucPl;
	char				*m_pszInformation;
	char				**m_listFormats;
	int					m_NumFormats;
	CConnection			**m_listConnections;
	int					m_NumConnections;
	CBandWitdh			**m_listBandwidths;
	int					m_NumBandwidths;
	CAttribute			**m_listAttributes;
	int					m_NumAttributes;

};


class CKey
{
public:
	CKey(	
		char *pszMethod,
		char *pszValue)
	{
		if(pszMethod)
			m_pszMethod= strdup(pszMethod);
		else
			m_pszMethod= strdup("");
		if(pszValue)
			m_pszValue= strdup(pszValue);
		else
			m_pszValue= strdup("");
	}
	CKey(const char *pData)
	{
		pData;
		m_pszMethod = NULL;
		m_pszValue = NULL;
	}
	~CKey()
	{
		CHK_FREE(m_pszMethod)
		CHK_FREE(m_pszValue)
	}
	int Parse(const char *pData)
	{
		pData;
		return 0;
	}

	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszValue) {
			sprintf(pOut,"t=%s:%s ",m_pszMethod, m_pszValue);
			len = strlen(pOut);
			pOut += len;
		} else {
			sprintf(pOut,"t=%s ",m_pszMethod);
			len = strlen(pOut);
			pOut += len;
		}

		return pOut - pData;
	}

	char *m_pszMethod;
	char *m_pszValue;
};

class CTime
{
public:
	CTime(	
		char *pszStartTime,
		char *pszEndTime):
		m_pszStartTime(strdup(pszStartTime)),
		m_pszEndTime(strdup(pszEndTime))
	{

	}
	CTime(const char *pData)
	{
		m_pszStartTime = NULL;
		m_pszEndTime = NULL;
		Parse(pData);
	}
	~CTime()
	{
		CHK_FREE(m_pszStartTime)
		CHK_FREE(m_pszEndTime)
	}
	int Parse(const char *pData)
	{
		pData;
		return 0;
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		int len = 0;
		if(m_pszStartTime) {
			sprintf(pOut,"t=%s %s",m_pszStartTime, m_pszEndTime);
			len = strlen(pOut);
			pOut += len;
		}
		return pOut - pData;
	}

	char *m_pszStartTime;
	char *m_pszEndTime;
};

class CSdp
{
public:
	CSdp()
	{
		m_nVersion = 0;
		m_pszSessionName = NULL;
		m_pszInformation = NULL;
		m_pszUri =NULL;
		m_pszEmail = NULL;
		m_pszPphone = NULL;
		m_pOrigin = NULL;
		m_pConnection = NULL;

		m_NumBandWidths = 0;
		m_listBandwidths = NULL;

		m_NumTimes = 0;
		m_listTime = NULL;

		m_NumKeys = 0;
		m_listKeys = NULL;

		m_NumAttributes = 0;
		m_listAttributes = NULL;

		m_NumeMediaDescriptions = 0;
		m_listMediaDescription = NULL;

	}
	~CSdp()
	{
		int i = 0;
		fprintf(stderr, "%s : Enter\n",__FUNCTION__);
		CHK_FREE(m_pszSessionName)
		CHK_FREE(m_pszInformation)
		CHK_FREE(m_pszUri)

		CHK_FREE(m_pszEmail)
		CHK_FREE(m_pszPphone)
		CHK_FREE(m_pOrigin)
		CHK_FREE(m_pConnection)
		CHK_FREE(m_listBandwidths)

		CHK_FREE(m_listTime)

		CHK_FREE(m_listKeys)

		CHK_FREE(m_listAttributes)

		for(i= 0; i < m_NumeMediaDescriptions; i++)
				delete (m_listMediaDescription[i]);
		CHK_FREE(m_listMediaDescription);
		fprintf(stderr, "%s : Leave\n",__FUNCTION__);
	}

	int Parse(const char *pData)
	{
		const char *pIn = pData;
		
		fprintf(stderr, "%s : Enter\n",__FUNCTION__);
		if(sscanf(pIn,"v=%d",&m_nVersion) != 1){
			return -1;
		}
		while(pIn = CSdpParse::NextSdpField(pIn)){
			if(strncmp(pIn,"o=", strlen("o=")) == 0){
				m_pOrigin = new COrigin(pIn);
			} else if(strncmp(pIn,"s=", strlen("s=")) == 0){
				int nLen = CSdpParse::GetFieldLen(pIn);
				m_pszSessionName = (char *)malloc(nLen+1);
				sscanf(pIn,"s=%s",m_pszSessionName);
			} else if(strncmp(pIn,"u=", strlen("u=")) == 0){
				int nLen = CSdpParse::GetFieldLen(pIn);
				m_pszUri = (char *)malloc(nLen+1);
				sscanf(pIn,"s=%s",m_pszUri);
			} else if(strncmp(pIn,"p=", strlen("p=")) == 0){
				int nLen = CSdpParse::GetFieldLen(pIn);
				m_pszPphone = (char *)malloc(nLen+1);
				sscanf(pIn,"p=%s",m_pszPphone);
			} else if(strncmp(pIn,"c=", strlen("c=")) == 0){
				m_pConnection = new CConnection(pIn);
			} else if(strncmp(pIn,"b=", strlen("b=")) == 0){
				CBandWitdh *pBandwidth = new CBandWitdh(pIn);
				Add(pBandwidth);
			} else if(strncmp(pIn,"t=", strlen("t=")) == 0){
				CTime *pTime = new CTime(pIn);
				Add(pTime);
			} else if(strncmp(pIn,"k=", strlen("k=")) == 0){
				CKey *pKey = new CKey(pIn);
				Add(pKey);
			} else if(strncmp(pIn,"a=", strlen("a=")) == 0){
				CAttribute *pAttribute = new CAttribute(pIn);
				Add(pAttribute);
			} else if(strncmp(pIn,"m=", strlen("m=")) == 0){
				CMediaDescription *pMediaDescription = new CMediaDescription(pIn);
				Add(pMediaDescription);
			}
		}
		fprintf(stderr, "%s : Leave\n",__FUNCTION__,__LINE__);
		return 0;
	}

	int Encode(char *pData, int nMaxLen)
	{
		char *pOut = pData;
		int len;
		int i;
		
		fprintf(stderr, "%s : Enter\n",__FUNCTION__);
		sprintf(pOut,"v=%d\r\n",m_nVersion);
		len = strlen(pOut);
		pOut += len;

		if(m_pOrigin) {
			len = m_pOrigin->Encode(pOut);
			pOut += len;
		}

		if(m_pszSessionName) {
			sprintf(pOut,"s=%s\r\n",m_pszSessionName);
			len = strlen(pOut);
			pOut += len;
		}

		if(m_pszInformation) {
			sprintf(pOut,"s=%s\r\n",m_pszInformation);
			len = strlen(pOut);
			pOut += len;
		}

		if(m_pszUri) {
			sprintf(pOut,"u=%s\r\n",m_pszUri);
			len = strlen(pOut);
			pOut += len;
		}

		if(m_pszEmail) {
			sprintf(pOut,"u=%s\r\n",m_pszEmail);
			len = strlen(pOut);
			pOut += len;
		}

		if(m_pszPphone) {
			sprintf(pOut,"u=%s\r\n",m_pszPphone);
			len = strlen(pOut);
			pOut += len;
		}

		if(m_pConnection) {
			len = m_pConnection->Encode(pOut);
			pOut += len;
		}

		for (i=0; i < m_NumBandWidths; i++) {
			len = m_listBandwidths[i]->Encode(pOut);
			pOut += len;
		}
		for (i=0; i < m_NumTimes; i++) {
			len = m_listTime[i]->Encode(pOut);
			pOut += len;
		}
		for (i=0; i < m_NumKeys; i++) {
			len = m_listKeys[i]->Encode(pOut);
			pOut += len;
		}
		for (i=0; i < m_NumAttributes; i++) {
			len = m_listAttributes[i]->Encode(pOut);
			pOut += len;
		}

		for (i=0; i < m_NumeMediaDescriptions; i++) {
			len = m_listMediaDescription[i]->Encode(pOut);
			pOut += len;
		}
		
		/* Trailing CRLF */
		sprintf(pOut,"\r\n");
		len = strlen(pOut);
		pOut += len;

		fprintf(stderr, "%s : Leave\n",__FUNCTION__,__LINE__);
		return pOut - pData;
	}
	void Add(CMediaDescription *pDescript)
	{
		m_listMediaDescription = (CMediaDescription **)
				realloc(m_listMediaDescription, (m_NumeMediaDescriptions + 1) * sizeof(CMediaDescription *));
		m_listMediaDescription[m_NumeMediaDescriptions] = pDescript;
		m_NumeMediaDescriptions++;
	}

	void Add(CAttribute *pAttribute)
	{
		m_listAttributes = (CAttribute **)
				realloc(m_listAttributes, (m_NumAttributes + 1) * sizeof(CAttribute *));
		m_listAttributes[m_NumAttributes] = pAttribute;
		m_NumAttributes++;
	}
	void Add(CKey *pKey)
	{
		m_listKeys = (CKey **)
				realloc(m_listKeys, (m_NumKeys + 1) * sizeof(CKey *));
		m_listKeys[m_NumKeys] = pKey;
		m_NumKeys++;
	}

	void Add(CTime *pTime)
	{
		m_listTime = (CTime **)
				realloc(m_listTime, (m_NumTimes + 1) * sizeof(CTime *));
		m_listTime[m_NumTimes] = pTime;
		m_NumTimes++;
	}

	void Add(CBandWitdh *pBandwidth)
	{
		m_listBandwidths = (CBandWitdh **)
				realloc(m_listBandwidths, (m_NumBandWidths + 1) * sizeof(CBandWitdh *));
		m_listBandwidths[m_NumBandWidths] = pBandwidth;
		m_NumBandWidths++;
	}

	// Utility functions
	int GetControl(std::string &url, char *szTrackType)
	{
		for (int i=0; i < m_NumeMediaDescriptions; i++){
			if (strnicmp(m_listMediaDescription[i]->m_pszMedia, szTrackType, strlen(szTrackType)) == 0) {
				return m_listMediaDescription[i]->GetControl(url);
			}
		}
		return 0;
	}

public:
	int				m_nVersion;
	COrigin			*m_pOrigin;
	char			*m_pszSessionName;
	char			*m_pszInformation;
	char			*m_pszUri;

	char			*m_pszEmail;
	char			*m_pszPphone;
	CConnection		*m_pConnection;
	CBandWitdh		**m_listBandwidths;
	int				m_NumBandWidths;
	
	CTime			**m_listTime;
	int				m_NumTimes;

	CKey			**m_listKeys;
	int				m_NumKeys;

	CAttribute		**m_listAttributes;
	int				m_NumAttributes;

	CMediaDescription **m_listMediaDescription;
	int				m_NumeMediaDescriptions;

};

#endif // __JD_SDP_H__