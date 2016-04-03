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
	speakerman.cpp

speakerman_LDADD=$(UNITTESTCPP_LIBS) $(JACK_LIBS) $(GLIB_LIBS) -lpthread 
speakerman_CXXFLAGS=-I$(top_srcdir)/src/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(UNITTESTCPP_CFLAGS)
speakerman_CFLAGS=$(testsuite_CXXFLAGS) 
speakerman_DATA=web/index.html
speakermandir=$(prefix)/share/@PACKAGE@/web