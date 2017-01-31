all:
	time -p clang++ -O3 -Os -std=c++1z -I ../preprocessor/include/ native-2-web.cpp