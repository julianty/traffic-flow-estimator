build/hello_opencv.exe: scratch/hello_opencv.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ $(shell pkg-config --cflags --libs opencv5)