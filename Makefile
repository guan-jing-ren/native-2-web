CXX=clang++
BOOST_INCLUDES=
BEAST_INCLUDES=$(BOOST_INCLUDES) -I ../asio/include/ -I ../Beast/include/

libn2w-fs.so: n2w-fs.cpp native-2-web-plugin.hpp native-2-web-manglespec.hpp native-2-web-js.hpp
	time -p $(CXX) -O3 -Os -std=c++1z $(BOOST_INCLUDES) -shared -fPIC -pthread -o libn2w-fs.so n2w-fs.cpp -ldl -lstdc++fs 

n2w-server: native-2-web-server.cpp native-2-web-plugin.hpp libn2w-fs.so
	time -p $(CXX) -O3 -Os -std=c++1z $(BEAST_INCLUDES)  -pthread -o n2w-server native-2-web-server.cpp -ldl -lstdc++fs 

all:
	time -p $(CXX) -O3 -Os -std=c++1z -I ../preprocessor/include/ -o n2w native-2-web.cpp &
	time -p $(CXX) -O3 -Os -std=c++1z -I ../preprocessor/include/ native-2-web-test.cpp &