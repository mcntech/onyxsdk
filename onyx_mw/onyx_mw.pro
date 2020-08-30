#QT += webkit \
#    network
LIBS += -lgthread-2.0
ONYX_LIBS = $(EZSDK)/onyx_em/onyx_libs
JDNET = $$ONYX_LIBS/jdnet
JDAWS = $$ONYX_LIBS/jdaws
TS_DEMUX = $$ONYX_LIBS/tsdemux
TS_MUX = $$ONYX_LIBS/tsmux
LIBRTMP = $$ONYX_LIBS/librtmp

DEFINES += ENABLE_OPENSSL
LIBS += -lssl -lcrypto 

CONFIG+=debug

CODEC_DM81XX = $(EZSDK)/component-sources/omx_05_02_00_48/examples/ti/omx/demos/codecdm81xx/src

INCLUDEPATH += $$JDNET/include $$JDAWS/include $$CODEC_DM81XX ./OpenMAXAL $$TS_DEMUX $$TS_MUX $$LIBRTMP
HEADERS = $$JDNET/include/JdRtspClntRec.h

TARGET = onyx_mw
SOURCES += $$JDNET/src/JdRtspClntRec.cpp $$JDNET/src/JdRtspSrv.cpp $$JDNET/src/JdRtpSnd.cpp $$JDNET/src/JdRtspClnt.cpp $$JDNET/src/JdAuthRfc2617.cpp $$JDNET/src/JdNetUtil.cpp \
           $$JDNET/src/JdMediaResMgr.cpp $$JDNET/src/JdHttpLiveSgmt.cpp $$JDNET/src/JdHttpLiveClnt.cpp $$JDNET/src/JdHttpSrv.cpp $$JDNET/src/JdHttpClnt.cpp \
           $$JDNET/src/JdRfc2250.cpp $$JDNET/src/JdRfc3984.cpp $$JDNET/src/JdRtp.cpp $$JDNET/src/JdRfc5391.cpp $$JDNET/src/JdRfc3640.cpp $$JDNET/src/JdOsal.cpp \
           $$JDAWS/src/JdAwsConfig.cpp $$JDAWS/src/JdAwsContext.cpp $$JDAWS/src/JdAwsS3.cpp $$JDAWS/src/JdAwsS3UpnpHttpConnection.cpp \
		   $$LIBRTMP/amf.c $$LIBRTMP/hashswf.c $$LIBRTMP/log.c $$LIBRTMP/parseurl.c  $$LIBRTMP/rtmp.c \
		   onyx_mw_main.cpp sock_ipc.cpp minini.c OmxIf.cpp OpenMAXAL/OpenMAXAL_IID.c \
		   RtspPublishBridge.cpp RtmpPublishBridge.cpp HlsSrvBridge.cpp EncOut.cpp \
		   StrmInBridgeBase.cpp RtspClntBridge.cpp  HlsClntBridge.cpp \
		   $$CODEC_DM81XX/strmconn.c $$CODEC_DM81XX/dbglog.c \
		   $$CODEC_DM81XX/h264parser.cpp \
		   $$TS_DEMUX/TsDemux.cpp $$TS_DEMUX/TsPsi.cpp $$TS_DEMUX/tsfilter.cpp \
  		   $$TS_MUX/SimpleTsMux.cpp

# install
target.path = .
INSTALLS += target
