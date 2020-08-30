#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
#define close		closesocket
#define snprintf	_snprintf
#define write		_write
#define socklen_t	int
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "JdAuthRfc2617.h"

#define BASE64_SIZE(x)  (((x)+2) / 3 * 4 + 1)

static unsigned long bswap32(unsigned long x)
{
    x= ((x<<8)&0xFF00FF00) | ((x>>8)&0x00FF00FF);
    x= (x>>16) | (x<<16);
    return x;
}

static unsigned long random_seed()
{
	return 0;
}

static int Strlcatf(char *dst, size_t size, const char *fmt, ...)
{
    int len = strlen(dst);
    va_list vl;

    va_start(vl, fmt);
    len += vsnprintf(dst + len, size > len ? size - len : 0, fmt, vl);
    va_end(vl);

    return len;
}

char *DataToHex(char *buff, const unsigned char *src, int s, int lowercase)
{
    int i;
    static const char hex_table_uc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'A', 'B',
                                           'C', 'D', 'E', 'F' };
    static const char hex_table_lc[16] = { '0', '1', '2', '3',
                                           '4', '5', '6', '7',
                                           '8', '9', 'a', 'b',
                                           'c', 'd', 'e', 'f' };
    const char *hex_table = lowercase ? hex_table_lc : hex_table_uc;

    for(i = 0; i < s; i++) {
        buff[i * 2]     = hex_table[src[i] >> 4];
        buff[i * 2 + 1] = hex_table[src[i] & 0xF];
    }
    return buff;
}

static int ConvertToBase64(unsigned char *pDest, const unsigned char *pSrc, int nLen)
{
    unsigned char    *pTmp;
    int   i;

    pTmp = pDest;
	static const unsigned char Base64Char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (i = 0; i < nLen - 2; i += 3)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
        *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2) |((pSrc[i + 2] & 0xC0) >> 6)];
        *pTmp++ = Base64Char[pSrc[i + 2] & 0x3F];
    }

    /* Convert last 1 or 2 bytes */
    if (i < nLen)   {
        *pTmp++ = Base64Char[(pSrc[i] >> 2) & 0x3F];
        if (i == (nLen - 1))  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4)];
            *pTmp++ = '=';
        }  else  {
            *pTmp++ = Base64Char[((pSrc[i] & 0x3) << 4) | ((pSrc[i + 1] & 0xF0) >> 4)];
            *pTmp++ = Base64Char[((pSrc[i + 1] & 0xF) << 2)];
        }
        *pTmp++     = '=';
    }
    *pTmp++ = '\0';
    return pTmp - pDest;
}

static const unsigned char S[4][4] = {
    { 7, 12, 17, 22 },  /* round 1 */
    { 5,  9, 14, 20 },  /* round 2 */
    { 4, 11, 16, 23 },  /* round 3 */
    { 6, 10, 15, 21 }   /* round 4 */
};

static const unsigned long T[64] = { // T[i]= fabs(sin(i+1)<<32)
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,   /* round 1 */
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,   /* round 2 */
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,   /* round 3 */
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,   /* round 4 */
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

void CORE(unsigned long i, unsigned long &t, unsigned long &a, unsigned long &b, unsigned long &c, unsigned long &d,  unsigned long *X) 
{
	t = S[i >> 4][i & 3];                                           
	a += T[i];                                                      
	                                                            
	if (i < 32) {                                                   
		if (i < 16) 
			a += (d ^ (b & (c ^ d))) + X[       i  & 15];   
		else        
			a += (c ^ (d & (c ^ b))) + X[(1 + 5*i) & 15];   
	} else {                                                        
		if (i < 48) 
			a += (b ^ c ^ d)         + X[(5 + 3*i) & 15];   
		else        
			a += (c ^ (b | ~d))      + X[(    7*i) & 15];   
	}                                                               
	a = b + (a << t | a >> (32 - t));                               
}

void CMd5::Body(unsigned long ABCD[4], unsigned long X[16])
{
    unsigned long t;
    int i;
    unsigned long a = ABCD[3];
    unsigned long b = ABCD[2];
    unsigned long c = ABCD[1];
    unsigned long d = ABCD[0];

    for (i = 0; i < 64; i++) {
        CORE(i, t, a, b, c, d, X);
        t = d;
        d = c;
        c = b;
        b = a;
        a = t;
    }
    ABCD[0] += d;
    ABCD[1] += c;
    ABCD[2] += b;
    ABCD[3] += a;
}

void CMd5::Init()
{
	mLen     = 0;

	mAbcd[0] = 0x10325476;
	mAbcd[1] = 0x98badcfe;
	mAbcd[2] = 0xefcdab89;
	mAbcd[3] = 0x67452301;
}

void CMd5::Update(const unsigned char *src, const int len)
{
    int i, j;

    j = mLen & 63;
    mLen += len;

    for (i = 0; i < len; i++) {
        mBlock[j++] = src[i];
        if (j == 64) {
            Body(mAbcd, (unsigned long *) mBlock);
            j = 0;
        }
    }
}

void CMd5::Final( unsigned char *dst)
{
    int i;
    unsigned long long finalcount = mLen << 3;

    Update((const unsigned char *)"\200", 1);
    while ((mLen & 63) != 56)
        Update((const unsigned char *)"", 1);

    Update((unsigned char *)&finalcount, 8);

    for (i = 0; i < 4; i++)
        *(unsigned long *)(dst + 4*i) =  bswap32(mAbcd[3 - i]);
}

void CMd5::Sum(unsigned char *dst, const unsigned char *src, const int len)
{
    Init();
    Update(src, len);
    Final(dst);
}

void CJdAuthRfc2167::HandleHeader(const char *key,  const char *value)
{

}


void UpdateMd5Strings(CMd5 *pMd5, ...)
{
    va_list vl;

    va_start(vl, pMd5);
    while (1) {
        const char* str = va_arg(vl, const char*);
        if (!str)
            break;
		pMd5->Update((const unsigned char *)str, strlen(str));
    }
    va_end(vl);
}

int CJdAuthRfc2167::ParseDigestAuth(const char *pszKey)
{
	int res = 0;
	const char *p = pszKey;

    for (;;) {
		const char *key;
		char *dest = NULL, *dest_end;
		int key_len, dest_len = 0;
		while (*p && (isspace(*p) || *p == ','))
			p++;

		if (!*p)
			goto Exit;

		key = p;

		if (!(p = strchr(key, '=')))
			goto Exit;
		p++;
		key_len = p - key;

		if (!strncmp(key, "realm=", key_len)) {
			dest = m_Realm;
			dest_len = MAX_REALM_LEN;
		} else if (!strncmp(key, "nonce=", key_len)) {
			dest     = m_Nonce;
			dest_len = MAX_KEY_LEN;
		} else if (!strncmp(key, "opaque=", key_len)) {
			dest = m_Qop;
			dest_len = MAX_OPAQUE_LEN;
		} else if (!strncmp(key, "algorithm=", key_len)) {
			dest     = m_Algorithm;
			dest_len = MAX_ALG_LEN;
		} else if (!strncmp(key, "qop=", key_len)) {
			dest     =  m_Qop;
			dest_len = MAX_OPAQUE_LEN;
		}


		if (*p == '\"') {
			p++;
			while (*p && *p != '\"') {
				if (*p == '\\') {
					if (!p[1])
						break;
					if (dest && dest < dest_end)
						*dest++ = p[1];
					p += 2;
				} else {
					if (dest && dest < dest_end)
						*dest++ = *p;
					p++;
				}
			}
			if (*p == '\"')
				p++;
		} else {
			for (; *p && !(isspace(*p) || *p == ','); p++)
				if (dest && dest < dest_end)
					*dest++ = *p;
		}
		if (dest)
			*dest = 0;
	}

Exit:
	return res;
}

int CJdAuthRfc2167::ParseAuth(const char *pszAuth)
{
	int res  = 0;
	const char *p = pszAuth;
	if(strncmp(p, "Basic ", strlen("Basic ")) == 0) {
		m_AuthType = AUTH_BASIC;
	} else if(strncmp(p, "Digest ", strlen("Digest ")) == 0) {
		m_AuthType = AUTH_DIGEST;
		ParseDigestAuth(pszAuth + strlen("Digest "));
	} else {
		res = 1;
	}
	return res;
}

/* Generate a digest reply, according to RFC 2617. */
char *CJdAuthRfc2167::MakeDigestAuth(
			const char *username,
			const char *password, 
			const char *uri,
			const char *method)
{
	CMd5 Md5;
	int len;
	unsigned long cnonce_buf[2];
	char cnonce[17];
	char nc[9];
	int  i;
	char A1hash[33], A2hash[33], response[33];
	unsigned char hash[16];
	char *authstr;

	m_Nc++;
	snprintf(nc, sizeof(nc), "%08x", m_Nc);

    /* Generate a client nonce. */
    for (i = 0; i < 2; i++)
        cnonce_buf[i] = random_seed();
    DataToHex(cnonce, (const unsigned char*) cnonce_buf, sizeof(cnonce_buf), 1);
    cnonce[2*sizeof(cnonce_buf)] = 0;


	// request-digest = <"> < KD ( H(A1), unq(nonce-value) ":" H(A2) ) >  <">


	// 3.2.2.2 A1
	//  A1       = unq(username-value) ":" unq(realm-value) ":" passwd
    Md5.Init();
    UpdateMd5Strings(&Md5, username, ":", m_Realm, ":", password, NULL);
    Md5.Final(hash);
    DataToHex(A1hash, hash, 16, 1);
    A1hash[32] = 0;

	
    if (!strcmp(m_Algorithm, "") || !strcmp(m_Algorithm, "MD5")) {
		// Default or MD5: Do nothing

    } else if (!strcmp(m_Algorithm, "MD5-sess")) {

		//  A1 = H( unq(username-value) ":" unq(realm-value)  ":" passwd )
		//           ":" unq(nonce-value) ":" unq(cnonce-value)

        Md5.Init();
        UpdateMd5Strings(&Md5, A1hash, ":", m_Nonce, ":", cnonce, NULL);
        Md5.Final(hash);
        DataToHex(A1hash, hash, 16, 1);
        A1hash[32] = 0;
    } else {
        /* Unsupported algorithm */
        return NULL;
    }

	// 3.2.2.3 A2
	// A2       = Method ":" digest-uri-value
    Md5.Init();
    UpdateMd5Strings(&Md5, method, ":", uri, NULL);
    Md5.Final(hash);
    DataToHex(A2hash, hash, 16, 1);
    A2hash[32] = 0;

    Md5.Init();
    UpdateMd5Strings(&Md5, A1hash, ":", m_Nonce, NULL);

	if (!strcmp(m_Qop, "auth") || !strcmp(m_Qop, "auth-int")) {
		// A2       = Method ":" digest-uri-value ":" H(entity-body)
        UpdateMd5Strings(&Md5, ":", nc, ":", cnonce, ":", m_Qop, NULL);
    }
    UpdateMd5Strings(&Md5, ":", A2hash, NULL);
    Md5.Final(hash);
    DataToHex(response, hash, 16, 1);
    response[32] = 0;


    if (!strcmp(m_Qop, "") || !strcmp(m_Qop, "auth")) {
    } else if (!strcmp(m_Qop, "auth-int")) {
        /* qop=auth-int not supported */
        return NULL;
    } else {
        /* Unsupported qop value. */
        return NULL;
    }

    len = strlen(username) + strlen(m_Realm) + strlen(m_Nonce) +
              strlen(uri) + strlen(response) + strlen(m_Algorithm) +
              strlen(m_Opaque) + strlen(m_Qop) + strlen(cnonce) +
              strlen(nc) + 150;

    authstr = (char *)malloc(len);
    if (!authstr)
        return NULL;

	//3.2.2 The Authorization Request Header
    snprintf(authstr, len, "Authorization: Digest ");

    /* TODO: Escape the quoted strings properly. */
    Strlcatf(authstr, len, "username=\"%s\"",   username);
    Strlcatf(authstr, len, ",realm=\"%s\"",     m_Realm);
    Strlcatf(authstr, len, ",nonce=\"%s\"",     m_Nonce);
    Strlcatf(authstr, len, ",uri=\"%s\"",       uri);
    Strlcatf(authstr, len, ",response=\"%s\"",  response);
    if (m_Algorithm[0])
        Strlcatf(authstr, len, ",algorithm=%s",  m_Algorithm);
    if (m_Opaque[0])
        Strlcatf(authstr, len, ",opaque=\"%s\"", m_Opaque);
    if (m_Qop[0]) {
        Strlcatf(authstr, len, ",qop=\"%s\"",    m_Qop);
        Strlcatf(authstr, len, ",cnonce=\"%s\"", cnonce);
        Strlcatf(authstr, len, ",nc=%s",         nc);
    }

    Strlcatf(authstr, len, "\r\n");

    return authstr;
}

char *CJdAuthRfc2167::CreateResponse(
		const char *auth,			// user:[passwd]
		const char *path, 
		const char *method
		)
{
	char *authstr = NULL;
	if (!auth || !strchr(auth, ':'))
		return NULL;

	if (m_AuthType == AUTH_BASIC) {
		int auth_b64_len = BASE64_SIZE(strlen(auth));
		int len = auth_b64_len + 30;
		char *ptr;
		authstr = (char *)malloc(len);
		if (!authstr)
			return NULL;

		snprintf(authstr, len, "Authorization: Basic ");
		ptr = authstr + strlen(authstr);
		int tmpLen = ConvertToBase64((unsigned char *)ptr, (const unsigned char *)auth, strlen(auth));
		strcat(ptr, "\r\n");
	} else if (m_AuthType == AUTH_DIGEST) {
		char *username = strdup(auth), *password;

		if (!username)
			return NULL;

		if ((password = strchr(username, ':'))) {
			*password++ = 0;
			authstr = MakeDigestAuth(username, password, path, method);
		}
		free(username);
    }
	return authstr;
}


static void print_md5(unsigned char *md5)
{
	int i;
	for (i = 0; i < 16; i++)
		printf("%02x", md5[i]);
	printf("\n");
}

int TestMd5(void)
{
    unsigned char md5val[16];
    int i;
    unsigned char in[1000];
	CMd5 Md5;
    for (i = 0; i < 1000; i++)
        in[i] = i * i;
	Md5.Sum(md5val, in, 1000); 
	print_md5(md5val);

    Md5.Sum(md5val, in,   63); print_md5(md5val);
    Md5.Sum(md5val, in,   64); print_md5(md5val);
    Md5.Sum(md5val, in,   65); print_md5(md5val);
    for (i = 0; i < 1000; i++)
        in[i] = i % 127;
    Md5.Sum(md5val, in,  999); print_md5(md5val);

    return 0;
}
