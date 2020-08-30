#ifndef _JD_MUTEX_h
#define _JD_MUTEX_h

#ifndef _WIN32
#include <pthread.h>
#endif

class CJdMutex
{
protected:
#ifdef _WIN32
	void* mHandle;
#else
    pthread_mutex_t mHandle;
#endif
public:

CJdMutex()
{
#ifdef _WIN32
	mHandle=0;
#else
    pthread_mutex_init(&mHandle,0);
#endif
}
~CJdMutex()
{
#ifdef _WIN32
	if (mHandle) CloseHandle(mHandle);
#else
    pthread_mutex_destroy(&mHandle);
#endif
}
void lock()
{
#ifdef _WIN32
	if (!mHandle) mHandle=CreateMutex(0,0,0);
	WaitForSingleObject(mHandle,INFINITE);
#else
    pthread_mutex_lock(&mHandle);
#endif
}
void unlock()
{
#ifdef _WIN32
	ReleaseMutex(mHandle);
#else
    pthread_mutex_unlock(&mHandle);
#endif
}
};
#endif // _JD_MUTEX_h
