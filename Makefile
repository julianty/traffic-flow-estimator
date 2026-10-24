MAIN_SRCS := src/main.cpp src/args.cpp src/ccl.cpp src/nms.cpp src/heap.cpp src/meta.cpp
MAIN_HDRS := src/args.hpp src/ccl.hpp src/nms.hpp src/heap.hpp src/meta.hpp

build/main.exe: $(MAIN_SRCS) $(MAIN_HDRS)
	@mkdir -p build
	g++ -std=c++17 -g $(MAIN_SRCS) -o $@ $(shell pkg-config --cflags opencv5) $(shell pkg-config --libs-only-L opencv5) -lopencv_videoio -lopencv_core -lopencv_highgui -lopencv_video -lopencv_imgproc


# Scratch builds

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

build/mog2_tune.exe: scratch/mog2_tune.cpp src/ccl.cpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/ccl.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/warmup_check.exe: scratch/warmup_check.cpp src/ccl.cpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/ccl.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/frame_grid.exe: scratch/frame_grid.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ $(shell pkg-config --cflags --libs opencv5)
