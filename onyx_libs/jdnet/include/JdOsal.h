#ifndef __JD_OSAL_H__
#define __JD_OSAL_H__

#ifdef WIN32
#include <winsock2.h>
#else
#include <pthread.h>
#endif

class COsalMutex
{
public:
	COsalMutex()
	{
#ifdef WIN32
		m_Mutex = CreateMutex(NULL, FALSE, NULL);
#else
		pthread_mutex_init(&m_Mutex, NULL);
#endif
	}
	~COsalMutex()
	{
#ifdef WIN32
		CloseHandle(m_Mutex);
#else
		 pthread_mutex_destroy(&m_Mutex);
#endif


	}
	void Acquire()
	{
#ifdef WIN32
		WaitForSingleObject(m_Mutex,INFINITE);
#else
		pthread_mutex_lock(&m_Mutex);
#endif
	}
	void Release()
	{
#ifdef WIN32
	ReleaseMutex(m_Mutex);
#else
	pthread_mutex_unlock(&m_Mutex);
#endif
	}
private:
#ifdef WIN32
	HANDLE          m_Mutex;
#else
	pthread_mutex_t m_Mutex;
#endif
};

#if defined (WIN32)
#define JD_OAL_SLEEP(x)   Sleep(x);
#elif defined (TI_SYS_BIOS)
#else // LINUX
#define JD_OAL_SLEEP(x)   usleep((x) * 1000);
#endif


typedef void *(*jdoalThrdFunction_t) (void *);
int jdoalThreadCreate(void **pthrdHandle, jdoalThrdFunction_t thrdFunction, void *thrdCrx);
int jdoalThreadJoin(void *pthrdHandle, int nTimeOutms);
#endif