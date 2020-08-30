#
#  Copyright 2011 MCN Technologies Inc.. All rights reserved.
#
set(jdnet_sources JdDrbRtspClnt.cpp	
	JdDrbRtspSrv.cpp 
	JdDtcpRcv.cpp 	 
	JdDtcpSnd.cpp 	 
	JdHttpClnt.cpp	
	JdHttpSrv.cpp	
	JdHttpLiveSgmt.cpp 
	JdNetUtil.cpp 	 
	JdRfc2250.cpp 	 
	JdRtp.cpp 	 
	JdRtpRcv.cpp 
	JdRtpSnd.cpp 
	JdRtspClnt.cpp 	 
	JdRtspSrv.cpp 	 
	JdStun.cpp)

set(jdnet_headers ../include/JdHttpClnt.h 
	../include/JdRfc2250.h 	 
	../include/JdRtp.h 	 
	../include/JdDtcpRcv.h 	 
	../include/JdDtcpSnd.h 
	../include/JdRtsp.h 
	../include/JdRtspClnt.h  
	../include/JdSdp.h  
	../include/JdRtspSrv.h 
	../include/JdHttpSrv.h 
	../include/JdStrmSched.h 
	../include/JdNetUtil.h 
	../include/JdDrbRtspClnt.h 
	../include/JdDrbRtspSrv.h 
	../include/JdHttpLiveSgmt.h 
	../include/TsParse.h 
	../include/JdSock.h 
	../include/JdStun.h)

add_library(jdnet SHARED ${jdnet_sources})
target_link_libraries(jdnet pthread)
install(TARGETS jdnet DESTINATION lib)
install(FILES ${jdnet_headers}
        DESTINATION include
        COMPONENT Headers)

