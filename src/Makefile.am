bin_PROGRAMS=speakerman

speakerman_SOURCES=ErrorHandler.cpp SpeakermanConfig.cpp SocketStream.cpp ServerSocket.cpp SpeakermanWebServer.cpp speakerman.cpp

speakerman_LDADD=$(UNITTESTCPP_LIBS) $(JACK_LIBS) $(GLIB_LIBS) -lpthread 
speakerman_CXXFLAGS=-I$(top_srcdir)/src/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(UNITTESTCPP_CFLAGS)
speakerman_CFLAGS=$(testsuite_CXXFLAGS) 