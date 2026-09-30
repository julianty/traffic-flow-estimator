build/main.exe: src/main.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ -IC:/msys64/ucrt64/include/opencv5 -lopencv_videoio

	
build/hello_opencv.exe: scratch/hello_opencv.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ $(shell pkg-config --cflags --libs opencv5)
