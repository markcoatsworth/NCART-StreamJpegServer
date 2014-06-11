all: StreamJpegServer
	./StreamJpegServer

StreamJpegServer: StreamJpegServer.o process_jpeg.o globals.o
	g++ -o StreamJpegServer StreamJpegServer.o process_jpeg.o globals.o `pkg-config opencv --cflags --libs` -lpthread -lbz2 -lsnappy -w

StreamJpegServer.o: StreamJpegServer.cpp
	g++ -Wall -c -o StreamJpegServer.o StreamJpegServer.cpp `pkg-config opencv --cflags --libs` -lpthread -lbz2 -lsnappy -w

process_jpeg.o: process_jpeg.cpp
	g++ -Wall -c -o process_jpeg.o process_jpeg.cpp `pkg-config opencv --cflags --libs` -lpthread -w

globals.o: globals.cpp
	g++ -Wall -c -o globals.o globals.cpp `pkg-config opencv --cflags --libs` -lpthread -w

clean:
	rm -rf *.o StreamJpegServer
