bin_PROGRAMS=speakerman

speakerman_SOURCES=\
	ErrorHandler.cpp \
	SignalHandler.cpp \
	SpeakermanConfig.cpp \
	Stream.cpp \
	SocketStream.cpp \
	ServerSocket.cpp \
	HttpMessage.cpp \
	SpeakermanWebServer.cpp \
	JackClient.cpp \
	JackProcessor.cpp \
	Names.cpp \
	Port.cpp \
	PortDefinition.cpp \
	SingleThreadFileCache.cpp \
	speakerman.cpp


speakerman_LDADD=$(UNITTESTCPP_LIBS) $(JACK_LIBS) $(GLIB_LIBS) -lpthread 
speakerman_CXXFLAGS=-I$(top_srcdir)/src/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(UNITTESTCPP_CFLAGS) -DINSTALL_DIR=\"$(prefix)\"
speakerman_CFLAGS=$(testsuite_CXXFLAGS) 
speakerman_DATA=web/index.html web/speakerman.css web/speakerman.js
speakermandir=$(prefix)/share/@PACKAGE@/web