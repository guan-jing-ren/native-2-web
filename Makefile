all:
	time -p clang++ -O3 -Os -std=c++1z -I ../preprocessor/include/ -o n2w native-2-web.cpp &
	time -p clang++ -O3 -Os -std=c++1z -I ../preprocessor/include/ native-2-web-test.cpp &