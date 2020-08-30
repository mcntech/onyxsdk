/*
 *  JdAwsConnection.cpp
 *
 *  Copyright 2011 MCN Technologies Inc.. All rights reserved.
 *
 */

#include <assert.h>
#include <openssl/bio.h> 
#include <openssl/evp.h>

#include "JdAwsContext.h"

#ifdef WIN32
#include <malloc.h>
#else
#include <sys/types.h>
#include <sys/time.h>
#endif

const char *gpWeekdayStr = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
const char *gpMonthStr = "Jan\0Feb\0Mar\0Apr\0May\0Jun\0"
"Jul\0Aug\0Sep\0Oct\0Nov\0Dec";

void CJdAwsContext::ToBase64(const char* aContent, 
                             size_t aContentSize,
                             /*OUT*/ std::string &strOut)
{
#ifdef ENABLE_OPENSSL
    char* lEncodedString;
    long aBase64EncodedStringLength;
    
    // initialization for base64 encoding stuff                                                                                                                                                        
    BIO* lBio = BIO_new(BIO_s_mem());
    BIO* lB64 = BIO_new(BIO_f_base64());
    BIO_set_flags(lB64, BIO_FLAGS_BASE64_NO_NL);
    lBio = BIO_push(lB64, lBio);
    
    BIO_write(lBio, aContent, aContentSize);
    BIO_flush(lBio);
    aBase64EncodedStringLength = BIO_get_mem_data(lBio, &lEncodedString);
    
    strOut.assign(lEncodedString, aBase64EncodedStringLength);
    BIO_free_all(lBio);    
#else
	//TODO: Use routines from JdAuthRfc2167
#endif
}

void CJdAwsContext::GetCurrentDate(/*OUT*/ std::string &dateOut)
{
    char tempbuf[128];
    struct tm *date;
    time_t currTime = time(NULL);
    date = gmtime(&currTime);
    sprintf(tempbuf,
            "%s, %02d %s %d %02d:%02d:%02d GMT",
            &gpWeekdayStr[date->tm_wday * 4],
            date->tm_mday, &gpMonthStr[date->tm_mon * 4],
            date->tm_year + 1900, date->tm_hour,
            date->tm_min, date->tm_sec);
    dateOut.assign(tempbuf); 
}

CJdAwsContext::CJdAwsContext(const char *pId,
                             const char *pSecretKey,
                             const char *pUserAgent,
                             const char *pDefaultHost,
                             const char *pUserToken,
                             const char *pProductToken)
{
    assert(pId && pSecretKey && pDefaultHost);
    idM.assign(pId);
    secretKeyM.assign(pSecretKey);
    defaultHostM.assign(pDefaultHost);
    if (pUserAgent) userAgentM.assign(pUserAgent);
    if (pUserToken) userTokenM.assign(pUserToken);
    if (pProductToken) productTokenM.assign(pProductToken);
}
