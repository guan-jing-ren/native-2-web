n2w-server: native-2-web-server.cpp native-2-web-plugin.hpp
	time -p clang++ -O3 -Os -std=c++1z -I ../system/include/ -I ../preprocessor/include/ -I ../asio/include/ -I ../Beast/include/ -pthread -o n2w-server native-2-web-server.cpp &

all:
	time -p clang++ -O3 -Os -std=c++1z -I ../preprocessor/include/ -o n2w native-2-web.cpp &
	time -p clang++ -O3 -Os -std=c++1z -I ../preprocessor/include/ native-2-web-test.cpp &