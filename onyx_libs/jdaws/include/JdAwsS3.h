/*
 *  JdAwsS3.h
 *
 *  Copyright 2011 MCN Technologies Inc.. All rights reserved.
 *
 */

#ifndef __JD_AWS_S3_H__
#define __JD_AWS_S3_H__

#include <string>

//#include "JdLib.h"

#include "JdAwsS3Request.h"

typedef enum  
{
	JD_OK, 
	JD_ERROR = 0x80000000,
	JD_ERROR_INVALID_ARG  = 0x80000001, 
	JD_ERROR_CONNECTION   = 0x80000002,
	JD_ERROR_PUT_WRITE    = 0x80000003,
	JD_ERROR_PUT_RESPONSE = 0x80000004,
	JD_ERROR_PUT_REJECT   = 0x80000005
} JD_STATUS;

class CJdAwsS3
{
public:
    /**
     * Creates a signature for use with the Amazon S3 service. 
     *
     * @note In case of query authentication, the returned signatured won't be URL encoded. 
     * It's the caller's responsibility to do that
     *
     * @return JD_OK if successful. 
     *         JD_ERROR_INVALID_ARG If a required parameter in the request object is not present. 
     *         JD_ERROR  otherwise.
     */ 
    static JD_STATUS CreateSignature(const CJdAwsS3Request &request,
                                     /*OUT*/ std::string &signature);
    
    /**
     * Make a URI which contains all the headers in the query string. This is uses query string
     * authentication, and is used to generate pre-authorized URIs for use in browsers. The
     * process is further described in the Amazon S3 docs.
     */ 
    static JD_STATUS MakeQueryStringUri(const CJdAwsS3Request &request,
                                        /*OUT*/ std::string &uri);
    
    /**
     * Makes a standard URI for issue requests programatically.
     */
    static JD_STATUS MakeStandardUri(const CJdAwsS3Request &request,
                                     /*OUT*/ std::string &uri);

	static JD_STATUS MakeHost(const CJdAwsS3Request &request,
                                    /*OUT*/ std::string &host);
};

#endif // __JD_AWS_S3_H__