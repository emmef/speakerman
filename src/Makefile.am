bin_PROGRAMS=speakerman

speakerman_SOURCES=Client.cpp speakerman.cpp 

speakerman_LDADD=$(GLIB_LIBS) $(JACK_LIBS) $(SIMPLEDSP_LIBS) $(UNITTESTCPP_LIBS) 
speakerman_CXXFLAGS=-I$(top_srcdir)/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(SIMPLEDSP_CFLAGS) $(UNITTESTCPP_CFLAGS)
speakerman_CFLAGS=$(testsuite_CXXFLAGS) 
