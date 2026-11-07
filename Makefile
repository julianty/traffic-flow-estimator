MAIN_SRCS := src/main.cpp src/args.cpp src/ccl.cpp src/nms.cpp src/heap.cpp src/meta.cpp src/tracker.cpp src/unionfind.cpp src/group.cpp
MAIN_HDRS := src/args.hpp src/ccl.hpp src/nms.hpp src/heap.hpp src/meta.hpp src/tracker.hpp src/unionfind.hpp src/group.hpp

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

build/unionfind_check.exe: scratch/unionfind_check.cpp src/unionfind.cpp src/unionfind.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/unionfind.cpp -o $@

build/group_check.exe: scratch/group_check.cpp src/group.cpp src/group.hpp src/unionfind.cpp src/unionfind.hpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/group.cpp src/unionfind.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/mog2_tune.exe: scratch/mog2_tune.cpp src/ccl.cpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/ccl.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

build/warmup_check.exe: scratch/warmup_check.cpp src/ccl.cpp src/ccl.hpp
	@mkdir -p build
	g++ -std=c++17 -g $< src/ccl.cpp -o $@ $(shell pkg-config --cflags --libs opencv5)

DEMO_SRCS := src/ccl.cpp src/nms.cpp src/heap.cpp src/meta.cpp src/tracker.cpp

build/make_demo.exe: scratch/make_demo.cpp $(DEMO_SRCS) $(MAIN_HDRS)
	@mkdir -p build
	g++ -std=c++17 -g $< $(DEMO_SRCS) -o $@ $(shell pkg-config --cflags --libs opencv5)

build/blob_dump.exe: scratch/blob_dump.cpp $(DEMO_SRCS) $(MAIN_HDRS)
	@mkdir -p build
	g++ -std=c++17 -g $< $(DEMO_SRCS) -o $@ $(shell pkg-config --cflags --libs opencv5)

build/frame_grid.exe: scratch/frame_grid.cpp
	@mkdir -p build
	g++ -std=c++17 -g $< -o $@ $(shell pkg-config --cflags --libs opencv5)
