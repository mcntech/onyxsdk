/*
 *  JdAwsS3UpnpHttpConnectionTest.cpp
 *
 *  Copyright 2011 MCN Technologies Inc. All rights reserved.
 *
 */

#include <upnp/upnpconfig.h>
#include <assert.h>
#include <openssl/ssl.h>
#include <sstream>
#include <string>
#include <time.h>
#include <upnp/upnp.h>

#include "JdAwsS3.h"
#include "JdAwsS3UpnpHttpConnection.h"
#include "JdDbgOut.h"
#include "JdLib.h"
#include "JdUpnpWebFileDownloadJob.h"
#include "JdUtil.h"

//#define RUN_SERVER_TESTS
#define DEBUG_JD_AWS_S3_SESSION_TEST
#ifndef DEBUG_JD_AWS_S3_SESSION_TEST
#undef JD_DEBUG_PRINT
#define JD_DEBUG_PRINT(format, ...)
#endif

ithread_cond_t gCond;
ithread_mutex_t gMutex;
bool gTestFinished = false;
CJdWorkerThread *gpThread;
bool gUseHttps = false;

class CDelegate : public CJdUpnpWebFileDownloadJobDelegate 
{
public:        
    virtual void OnUpdate(unsigned int length,
                          unsigned int total,
                          int status)
    {
        if (status != UPNP_E_SUCCESS) {
            assert(status == UPNP_E_FINISH);
            JD_LOG_INFO("Finished download\n");
            ithread_mutex_lock(&gMutex);
            gTestFinished = true;
            ithread_cond_broadcast(&gCond);
            ithread_mutex_unlock(&gMutex);
        }
    }
};

class CTestSignature
{
public:
    static void Run()
    {
        /* note the userId, and secret key here are dummy values obtained from
         * http://docs.amazonwebservices.com/AmazonS3/latest/dev/RESTAuthentication.html
         */
        CJdAwsContext context("0PN5J17HBGZHT7JJ3X82", 
                              "uV3F3YluFJax1cknvbcGwgjvx4QpvB+leU8dUj2o",
                              "JdAwsS3SessionTest",
                              "s3.amazonaws.com");        
        
        {
            /* Example Object GET */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/photos/puppy.jpg";
            request.dateM = "Tue, 27 Mar 2007 19:36:42 +0000";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "xXjDGYUmKxnwqr5KXNPGldn5LbA=");
        }
        {
            /* Example Object PUT */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::PUT;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/photos/puppy.jpg";
            request.dateM = "Tue, 27 Mar 2007 21:15:45 +0000";
            request.contentLengthM = 94328;
            request.contentTypeM = "image/jpeg";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "hcicpDDvL9SsO6AkvxqmIWkmOuQ=");
        }
        {
            /* Example List */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/";
            request.dateM = "Tue, 27 Mar 2007 19:42:41 +0000";
            request.queryStringM = "prefix=photos&max-keys=50&marker=puppy";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "jsRt/rhG+Vtp88HrYL706QhE4w4=");
        }
        {
            /* Example Fetch */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/";
            request.dateM = "Tue, 27 Mar 2007 19:44:46 +0000";
            request.subResourceM = "acl";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "thdUi9VAkzhkniLj96JIrOPGi0g=");
        }
        {
            /* Example Delete */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::DELETE;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/photos/puppy.jpg";
            request.amzHeaderNamesM.push_back("x-amz-date");
            request.amzHeaderValuesM.push_back("Tue, 27 Mar 2007 21:20:26 +0000");
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "k3nL7gH3+PadhTEVn5Ip83xlYzk=");
        }
        {
            /* Example Upload */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::PUT;
            request.hostM = "static.johnsmith.net:8080";
            request.bucketNameM = "static.johnsmith.net";
            request.pathM = "/db-backup.dat.gz";
            request.contentMd5M = "4gJE4saaMU4BqNR0kLY+lw==";
            request.contentTypeM = "application/x-download";
            request.dateM = "Tue, 27 Mar 2007 21:06:08 +0000";
            request.contentLengthM = 5913339;
            request.amzHeaderNamesM.push_back("x-amz-acl");
            request.amzHeaderValuesM.push_back("public-read");
            request.amzHeaderNamesM.push_back("x-amz-meta-checksumalgorithm");
            request.amzHeaderValuesM.push_back("crc32");
            request.amzHeaderNamesM.push_back("x-amz-meta-filechecksum");
            request.amzHeaderValuesM.push_back("0x02661779");
            request.amzHeaderNamesM.push_back("x-amz-meta-reviewedby");
            request.amzHeaderValuesM.push_back("joe@johnsmith.net,jane@johnsmith.net");
            request.otherHeadersM.push_back("Content-Disposition: attachment; filename=database.dat");
            request.otherHeadersM.push_back("Content-Encoding: gzip");
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "C0FlOtU8Ylb9KDTpZqYkZPX91iI=");
        }
        {
            /* Example List All My Buckets */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.pathM = "/";
            request.dateM = "Wed, 28 Mar 2007 01:29:59 +0000";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "Db+gepJSUbZKwpx1FR0DLtEYoZA=");
        }
        {
            /* Example Unicode Keys */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.pathM = "/dictionary/fran%C3%A7ais/pr%c3%a9f%c3%a8re";
            request.dateM = "Wed, 28 Mar 2007 01:49:49 +0000";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            assert(signature == "dxhSBHoI6eVSPcXJqEghlUzZMnY=");
        }
        {
            /* Example Query String Request Authentication */
            CJdAwsS3Request request(&context);
            std::string signature;
            request.pContextM = &context;
            request.methodM = CJdAwsS3Request::GET;
            request.hostM = "s3.amazonaws.com";
            request.bucketNameM = "johnsmith";
            request.pathM = "/photos/puppy.jpg";
            request.expiresM = "1175139620";
            JD_STATUS status = CJdAwsS3::CreateSignature(request, signature);
            assert(status == JD_OK);
            char *pSignature = jd_url_encode(signature.c_str());
            assert(strcmp(pSignature, "rucSbH0yNEcP9oM2XNlouVI3BH4%3D") == 0);
        }
    }
};

class CTestGet
{
public:
    static void Run(CJdAwsContext *pContext)
    {
        JD_DEBUG_PRINT("Making GET Request\n");
        CJdAwsS3Request request(pContext);
        request.methodM = CJdAwsS3Request::GET;
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        CJdAwsS3UpnpHttpResponse response;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::MakeRequest(request, response);
        assert(status == JD_OK);
        assert(response.pHandleM);
        int upnpstatus = UpnpEndHttpRequest(response.pHandleM, request.timeoutM);
        assert(upnpstatus == UPNP_E_SUCCESS);
        int httpStatus;
        upnpstatus = UpnpGetHttpResponse(response.pHandleM, NULL, 
                                         NULL, NULL, &httpStatus, request.timeoutM);
        assert(upnpstatus == UPNP_E_SUCCESS);
        JD_DEBUG_PRINT("Finished Making GET Request :%d\n", httpStatus);
        assert(httpStatus == 200);
        CDelegate *pDelegate = new CDelegate();
        CJdUpnpWebFileDownloadJob *pJob = CJdUpnpWebFileDownloadJob::Create(1024,
                                                                            30,
                                                                            response.pHandleM,
                                                                            "/tmp/dest.txt",
                                                                            pDelegate);
        assert(pJob);
        response.pHandleM = NULL;
        gpThread->AddJob(pJob);
    }
    static void Run2(CJdAwsContext *pContext)
    {
        JD_DEBUG_PRINT("Making GET Request\n");
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::DownloadFile(request, "/tmp/dest.txt");
        assert(status == JD_OK);
        JD_DEBUG_PRINT("Finished Making GET Request");
    }
};

class CTestPut
{
public:
    static void Run(CJdAwsContext *pContext)
    {
        const char* contentType = "mpeg/ts";
        int httpStatus;
        JD_DEBUG_PRINT("Making PUT Request\n");
        const char *pBuf = "blahblahblah";
        size_t size = strlen(pBuf) + 1;
        int timeout = 30;
        CJdAwsS3Request request(pContext);
        request.methodM = CJdAwsS3Request::PUT;
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        request.contentTypeM.assign(contentType);
        request.contentLengthM = size;
        request.fUseHttpsM = gUseHttps;
        CJdAwsContext::GetCurrentDate(request.dateM);
        CJdAwsS3UpnpHttpResponse response;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::MakeRequest(request, response);
        assert(status == JD_OK);
        assert(response.pHandleM);
        JD_DEBUG_PRINT("Finished Making PUT Request\n");
        int upnpstatus = UpnpWriteHttpRequest(response.pHandleM, (char*) pBuf, &size, 30);
        assert(upnpstatus == UPNP_E_SUCCESS);
        upnpstatus = UpnpEndHttpRequest(response.pHandleM, timeout);
        assert(upnpstatus == UPNP_E_SUCCESS);
        upnpstatus = UpnpGetHttpResponse(response.pHandleM, NULL, NULL, NULL,
                                         &httpStatus, timeout);
        assert(upnpstatus == UPNP_E_SUCCESS);
        JD_DEBUG_PRINT("PUT request status: %d\n", httpStatus);
        assert(httpStatus == 200);
    }
    static void Run2(CJdAwsContext *pContext)
    {
        const char* contentType = "mpeg/ts";
        JD_DEBUG_PRINT("Making PUT Request\n");
        const char *pBuf = "blahblahblah";
        size_t size = strlen(pBuf) + 1;
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        request.contentTypeM.assign(contentType);
        request.contentLengthM = size;
        request.fUseHttpsM = gUseHttps;
        CJdAwsContext::GetCurrentDate(request.dateM);
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::UploadBuffer(request, pBuf, size);
        assert(status == JD_OK);
        JD_DEBUG_PRINT("Finished Making PUT Request\n");
    }
    static void Run3(CJdAwsContext *pContext)
    {
        const char* contentType = "mpeg/ts";
        JD_DEBUG_PRINT("Making PUT Request\n");
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        request.contentTypeM.assign(contentType);
        request.fUseHttpsM = gUseHttps;
        CJdAwsContext::GetCurrentDate(request.dateM);
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::UploadFile(request, "/tmp/dest.txt");
        assert(status == JD_OK);
        JD_DEBUG_PRINT("Finished Making PUT Request\n");
    }
};

class CTestDelete
{
public:
    static void Run(CJdAwsContext *pContext)
    {
        JD_DEBUG_PRINT("Making DELETE Request\n");
        CJdAwsS3Request request(pContext);
        request.methodM = CJdAwsS3Request::DELETE;
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        CJdAwsS3UpnpHttpResponse response;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::MakeRequest(request, response);
        assert(status == JD_OK);
        assert(response.pHandleM);
        int upnpstatus = UpnpEndHttpRequest(response.pHandleM, request.timeoutM);
        int httpStatus;
        assert(upnpstatus == UPNP_E_SUCCESS);
        upnpstatus = UpnpGetHttpResponse(response.pHandleM, NULL, 
                                         NULL, NULL, &httpStatus, request.timeoutM);
        assert(upnpstatus == UPNP_E_SUCCESS);
        JD_DEBUG_PRINT("Finished Making DELETE Request: %d\n", httpStatus);
        assert(204 == httpStatus);
    }
    static void Run2(CJdAwsContext *pContext)
    {
        JD_DEBUG_PRINT("Making DELETE Request\n");
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::Delete(request);
        assert(status == JD_OK);
        JD_DEBUG_PRINT("Finished Making DELETE Request\n");
    }
};

class CTestGetFail
{
public:
    static void Run(CJdAwsContext *pContext)
    {
        std::string contentType;
        JD_DEBUG_PRINT("Making GET Request\n");
        CJdAwsS3Request request(pContext);
        request.methodM = CJdAwsS3Request::GET;
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        CJdAwsS3UpnpHttpResponse response;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::MakeRequest(request, response);
        assert(status == JD_OK);
        assert(response.pHandleM);
        int upnpstatus = UpnpEndHttpRequest(response.pHandleM, request.timeoutM);
        int httpStatus;
        assert(upnpstatus == UPNP_E_SUCCESS);
        upnpstatus = UpnpGetHttpResponse(response.pHandleM, NULL, 
                                         NULL, NULL, &httpStatus, request.timeoutM);
        JD_DEBUG_PRINT("Finished Making GET Request :%d\n", httpStatus);
        assert(httpStatus == 404);
    }
    static void Run2(CJdAwsContext *pContext)
    {
        std::string contentType;
        JD_DEBUG_PRINT("Making GET Request\n");
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        CJdAwsContext::GetCurrentDate(request.dateM);
        request.fUseHttpsM = gUseHttps;
        JD_STATUS status = CJdAwsS3UpnpHttpConnection::DownloadFile(request, "/tmp/dest.txt");
        assert(status == JD_ERROR);
        JD_DEBUG_PRINT("Finished Making GET Request\n");
    }
};

class CTestGenerateUri
{
public:
    static void Run(CJdAwsContext *pContext)
    {
#ifdef RUN_SERVER_TESTS
        CTestPut::Run2(pContext);
#endif
        CJdAwsS3Request request(pContext);
        request.pContextM = pContext;
        request.methodM = CJdAwsS3Request::GET;
        request.hostM.assign("s3.amazonaws.com");
        request.pathM.assign("/test1.txt");
        request.bucketNameM.assign("ntvcam");
        {
            time_t seconds = time(NULL) + 120; // 120 seconds from the current time
            std::ostringstream ss;
            ss << seconds;
            request.expiresM = ss.str();
        }
        std::string uri;
        JD_STATUS status = CJdAwsS3::MakeQueryStringUri(request, uri);
        assert(status == JD_OK);
        JD_LOG_INFO("Paste the following uri into the browser:\n %s\n",
                    uri.c_str());
    }
};

void WaitForFinish()
{
    JD_DEBUG_PRINT("BEGIN\n");
    ithread_mutex_lock(&gMutex);
    while(!gTestFinished) {
        ithread_cond_wait(&gCond, &gMutex);
    }
    ithread_mutex_unlock(&gMutex);
    gTestFinished = false;
    JD_DEBUG_PRINT("END\n");
}

int main(const char *argv[], int argc)
{
    JD_STATUS  status = CJdLib::Initialize(false, NULL);
    assert(status == JD_OK);

    {
        JD_LOG_INFO("Testing signature generation\n\n");
        CTestSignature::Run();
    }

    // TODO Generate context
    CJdAwsContext context(argv[0], argv[1], NULL, "s3.amazonaws.com");
    
#ifdef RUN_SERVER_TESTS
    {
        ithread_mutex_init(&gMutex, NULL);
        ithread_cond_init(&gCond, NULL);
        gpThread = new CJdWorkerThread(false);
        gpThread->Start();
        
        // Don't do this since it's unsafe. Only use to debug if HTTPS is not working for
        // some reason.
        JD_LOG_INFO("Running tests against server without using HTTPS\n\n");
        CTestPut::Run(&context);
        CTestGet::Run(&context);
        WaitForFinish();
        CTestDelete::Run(&context);
        CTestGetFail::Run(&context);
        gUseHttps = true;
        JD_LOG_INFO("Running tests against server using HTTPS\n\n");
        UpnpInitSslContext(1, TLSv1_client_method());
        /* rerun all the tests using HTTPS */
        CTestPut::Run(&context);
        CTestGet::Run(&context);
        WaitForFinish();
        CTestDelete::Run(&context);
        CTestGetFail::Run(&context);
        JD_LOG_INFO("Running tests against server using HTTPS using compact API\n\n");
        CTestPut::Run2(&context);
        CTestGet::Run2(&context);
        CTestDelete::Run2(&context);
        CTestPut::Run3(&context);
        CTestGet::Run2(&context);
        CTestDelete::Run2(&context);
        CTestGetFail::Run2(&context);
    }
#endif
    {
        CTestGenerateUri::Run(&context);
    }

    JD_LOG_INFO("All tests passed\n");
    return 0;
}


