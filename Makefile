build/main.exe: src/main.cpp src/args.cpp src/args.hpp src/ccl.cpp src/ccl.hpp src/nms.cpp src/nms.hpp src/heap.hpp src/heap.cpp
	@mkdir -p build
	g++ -std=c++17 -g src/main.cpp src/args.cpp src/ccl.cpp src/nms.cpp src/heap.cpp -o $@ $(shell pkg-config --cflags opencv5) $(shell pkg-config --libs-only-L opencv5) -lopencv_videoio -lopencv_core -lopencv_highgui -lopencv_video -lopencv_imgproc


build/hello_opencv.exe: scratch/hello_opencv.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ $(shell pkg-config --cflags --libs opencv5)

build/ccl_check.exe: scratch/ccl_check.cpp src/ccl.cpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/ccl.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/heap_check.exe: scratch/heap_check.cpp src/heap.cpp src/heap.hpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/heap.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/nms_check.exe: scratch/nms_check.cpp src/nms.cpp src/nms.hpp src/heap.cpp src/heap.hpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/nms.cpp src/heap.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)