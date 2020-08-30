#include "JdOsal.h"
#include <assert.h>
typedef void *(*jdoalThrdFunction_t) (void *);
int jdoalThreadCreate(void **pthrdHandle, jdoalThrdFunction_t thrdFunction, void *thrdCrx)
{
#ifdef WIN32
	DWORD dwThreadId;
	*pthrdHandle = (void *)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thrdFunction, thrdCrx, 0, &dwThreadId);

#else
	pthread_t  _thrdHandle;
	if (pthread_create (&_thrdHandle, NULL, (jdoalThrdFunction_t)thrdFunction, thrdCrx) != 0)	{
		assert(0);
	}
	*pthrdHandle = (void *)_thrdHandle;
#endif
	return 0;
}

int jdoalThreadJoin(void *pthrdHandle, int nTimeOutms)
{
#ifdef WIN32
	WaitForSingleObject(pthrdHandle, nTimeOutms);
#else
	void *ret_value;
	pthread_join ((pthread_t)pthrdHandle, (void **) &ret_value);
#endif
	return 0;
}
