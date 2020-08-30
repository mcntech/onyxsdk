#ifndef __JD_AUTH_RFC_2617_H__
#define __JD_AUTH_RFC_2617_H__

#define MAX_KEY_LEN     256
#define MAX_REALM_LEN   128
#define MAX_OPAQUE_LEN  30
#define MAX_ALG_LEN     30
#define MAX_QOP_LEN     30
class CMd5
{
public:
	unsigned long long mLen;
	unsigned char mBlock[64];
	unsigned long mAbcd[4];
	
	void Sum(unsigned char *dst, const unsigned char *src, const int len);
	void Final( unsigned char *dst);
	void Update(const unsigned char *src, const int len);
	void Init();

private:
	void Body(unsigned long ABCD[4], unsigned long X[16]);
};

class CJdAuthRfc2167
{
public:
	typedef enum _AuthType {
		AUTH_NONE = 0,
		AUTH_BASIC, 
		AUTH_DIGEST,
	} AuthType;

	CJdAuthRfc2167()
	{
		m_AuthType = AUTH_NONE;
		m_Algorithm[0] = 0;
		m_Opaque[0] = 0;
		m_Qop[0] = 0;
	}
    char      m_Nonce[MAX_KEY_LEN];      // Server specified nonce
    char      m_Algorithm[MAX_ALG_LEN];  // Server specified digest algorithm */
    char      m_Qop[MAX_QOP_LEN];        // Quality of protection
    char      m_Opaque[MAX_OPAQUE_LEN];  // A server-specified string that should be included in authentication responses
    int       m_Nc;                      // Nonce count
    AuthType  m_AuthType;
    char      m_Realm[MAX_REALM_LEN];

	void HandleHeader(const char *key,  const char *value);
	char *CreateResponse(const char *auth, const char *path, const char *method);
	int ParseDigestAuth(const char *pszAuth);
	int ParseAuth(const char *pszAuth);

	char *MakeDigestAuth(
			const char *username,
			const char *password, 
			const char *uri,
			const char *method);

	int ParseKeyValue(const char *pszKey);

};
#endif //__JD_AUTH_RFC_2617_H__