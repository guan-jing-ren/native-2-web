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
BOOST_INCLUDES=
BEAST_INCLUDES=$(BOOST_INCLUDES) -I ../Beast/include/

libn2w-fs.so: n2w-fs.cpp native-2-web-plugin.hpp native-2-web-manglespec.hpp native-2-web-js.hpp
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) $(BOOST_INCLUDES) -shared -fPIC -pthread -o libn2w-fs.so n2w-fs.cpp -ldl $(STDLIBFLAGS)

n2w-server: native-2-web-server.cpp native-2-web-plugin.hpp libn2w-fs.so
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) $(BEAST_INCLUDES) -pthread -o n2w-server native-2-web-server.cpp -ldl $(STDLIBFLAGS)

all:
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) -I ../preprocessor/include/ -o n2w native-2-web.cpp &
	time -p $(CXX) $(OPFLAGS) $(STDFLAGS) -I ../preprocessor/include/ native-2-web-test.cpp &

clean:
	rm ./n2w-server ./libn2w-fs.so a.out n2w