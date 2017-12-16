CL=LLV
GCC=g++-5
LLV=clang++
CXX=$($(CL))
OPFLAGS=-O3 -Os
GCC_STDFLAGS=-std=c++1z
LLV_STDFLAGS=-std=c++1z -stdlib=libc++
GCC_STDLIBFLAGS=-lstdc++fs
LLV_STDLIBFLAGS=-lc++experimental
STDFLAGS=$($(CL)_STDFLAGS)
STDLIBFLAGS=$($(CL)_STDLIBFLAGS)
BOOST_INCLUDES=-I /home/kykwan/include -L /home/kykwan/lib
BEAST_INCLUDES=$(BOOST_INCLUDES) -I ../Beast/include/

libn2w-fs.so: n2w-fs.cpp native-2-web-plugin.hpp native-2-web-manglespec.hpp native-2-web-js.hpp
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) $(BOOST_INCLUDES) -shared -fPIC -pthread -o libn2w-fs.so n2w-fs.cpp -ldl $(STDLIBFLAGS)

n2w-server: native-2-web-server.cpp native-2-web-plugin.hpp libn2w-fs.so
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) $(BEAST_INCLUDES) -pthread -o n2w-server native-2-web-server.cpp -ldl -lboost_system -lboost_thread -lboost_atomic -lboost_chrono -lboost_context -lboost_coroutine -lboost_program_options $(STDLIBFLAGS)

all: libn2w-fs.so n2w-server

### OLD TESTS ###
n2w:
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) -I . -I ../preprocesor/include/ -o n2w oldtests/native-2-web.cpp

n2wt:
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) -I . -I ../preprocessor/include/ -o n2wt oldtests/native-2-web-test.cpp

clean:
	rm ./n2w-server ./libn2w-fs.so ./n2w ./n2wt