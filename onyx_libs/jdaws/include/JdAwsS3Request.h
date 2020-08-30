/*
 *  JdAwsS3Request.h
 *
 *  Copyright 2011 MCN Technologies Inc.. All rights reserved.
 *
 */

#ifndef __JD_AWS_S3_REQUEST_H__
#define __JD_AWS_S3_REQUEST_H__

#include <string>
#include <vector>

#include "JdAwsContext.h"

/**
 * Contains the information required for the SDK to make a request to the Amazon S3 service.
 *
 * @note This is a data-only class. It doesn't define any additional methods.
 */
class CJdAwsS3Request
{
public:
    enum EJdAwsS3RequestMethod {
        PUT,
        GET,
        REMOVE,
        HEAD,
        UNDEFINED
    };
    
    CJdAwsS3Request(CJdAwsContext *pContext) : 
    methodM(UNDEFINED), pContextM(pContext), 
    hostM(pContextM->GetDefaultHost()),
    fUseHttpsM(true), 
    fIncludeResponseHeadersM(false),
    contentLengthM(0), timeoutM(30), fUseRestM(true) 
	{
	}
    /** The type of method to use to make the request */
    EJdAwsS3RequestMethod methodM;  
    /** The AWS context which contains the id, and secret key. 
     * Required. */
    CJdAwsContext *pContextM;
    /** The host name of the AWS server. Ex: s3.amazonaws.com. 
     * Required. */
    std::string hostM;
    /** The path for the url. Ex: /photos/1.jpg. 
     * Required */
    std::string pathM;
    /** The name of the bucket being accessed. 
     *
     * Optional if virtual hosting for buckets is used. 
     */
    std::string bucketNameM;
    /**  Lexographically sorted subresources without the '?' prefix.
     * Ex: 'acl&versionId=value' 
     * Optional. */
    std::string subResourceM;
    /** The query string for the URI without the '?' prefix
     *  Ex: 'var1=val1&var2=val2'.
     *  @note This can be included in addition to a subresource.
     * Optional. */
    std::string queryStringM;
    /** 
     * The timestamp of the request. Use CJdAwsContext::GetCurrentDate() to get the current
     * timestamp with the appropriate formatting. If this is empty, then the date value
     * won't be used when generating the signature, and the 'Date' header won't be sent
     * with the request.
     *
     * Optional.
     */
    std::string dateM;
    /** The expiration time. Application only for making requests using query string authentication.
     * If non-empty then query string authentication will be used. 
     *
     * @note Currently query string authentication is not supported by the API. There's no need since
     * using the SDK allows specifying the HTTP headers directly. However this field is useful for
     * generating signatures using the CJdAwsS3::CreateSignature() method.
     */
    std::string expiresM;
    /** Sorted AMZ header names. These are headers that being with 'x-amz-"
     *  These headers should be in lower case, and leading and trailing spaces trimmed.
     * Optional. 
     */
    std::vector<std::string> amzHeaderNamesM;
    /** Sorted AMZ header values. These are headers that being with 'x-amz-"
     *  These headers should be in lower case, and leading and trailing spaces trimmed.
     * Optional.
     */
    std::vector<std::string> amzHeaderValuesM;
    /** Other HTTP headers that should be included in the request. 
     * @note The SDK automatically generates headers for 'Date', 'Authorization', and 'Host'.
     * These headers should be of the following format:
     *
     * <header-name>: <header-value>\r\n
     *
     * Optional.
     */
    std::vector<std::string> otherHeadersM;
    /** If true use HTTPS to make the request. Otherwise HTTP is used.
     *  Default is true. 
     *
     * Optional.
     */
    bool fUseHttpsM;
    /** If true response headers will be included in the response.
     *  Default is false. 
     *
     * Optional
     */
    bool fIncludeResponseHeadersM;
    /** The content-type of the content being sent to the server. 
     *
     * Optional
     */
    std::string contentTypeM;
    /** The MD5 of the content being sent.
     *
     * Optional
     */
    std::string contentMd5M;
    /** The length of the content being sent.
     *
     * Optional
     */
    int contentLengthM;
    /** The timeout for the request. Use negative values for infinite timeout.
     *  Default value is 30 seconds.
     *
     * Optional
     */
    int timeoutM;
private:
    /**
     * Uses the REST API to make the request. Currently POST is unsupported. This
     * argument is defined for future use.
     */
    bool fUseRestM;
};


#endif // __JD_AWS_S3_REQUEST_H__