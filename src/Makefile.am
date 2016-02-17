bin_PROGRAMS=speakerman

speakerman_SOURCES=ErrorHandler.cpp JackClient.cpp speakerman.cpp 

speakerman_LDADD=$(TDAP_LIBS) $(UNITTESTCPP_LIBS) $(JACK_LIBS) $(GLIB_LIBS) -lpthread 
speakerman_CXXFLAGS=-I$(top_srcdir)/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(TDAP_CFLAGS) $(UNITTESTCPP_CFLAGS)
speakerman_CFLAGS=$(testsuite_CXXFLAGS) 