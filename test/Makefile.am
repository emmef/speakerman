bin_PROGRAMS=test_speakerman

test_speakerman_SOURCES=\
	main.cpp

test_speakerman_LDADD=$(UNITTESTCPP_LIBS) $(JACK_LIBS) $(GLIB_LIBS) -lpthread
test_speakerman_CXXFLAGS=-I$(top_srcdir)/src/include $(GLIB_CFLAGS) $(JACK_CFLAGS) $(UNITTESTCPP_CFLAGS) -DINSTALL_DIR=\"$(prefix)\"
test_speakerman_CFLAGS=$(testsuite_CXXFLAGS)
