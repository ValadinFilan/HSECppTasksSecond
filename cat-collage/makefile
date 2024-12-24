

all: build run
build:
	g++ -I/usr/include/opencv4/ -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio -lopencv_highgui -lstdc++ -o exec.bin main.cpp -std=c++17 
run:
	./exec.bin