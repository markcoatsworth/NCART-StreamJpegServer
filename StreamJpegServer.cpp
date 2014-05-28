#include <errno.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

// Socket includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// PThread include
#include <pthread.h>

// OpenCV include
#include <opencv2/opencv.hpp>

// Jpeg Lib includes
#include <jpeglib.h>

// ZLib include
#include <zlib.h>

// Local includes
#include "process_jpeg.h"
#include "globals.h"
#include "osc_handlers.h"

//Liblo includes
//#include <lo/lo.h>

#define DEBUG 1         	// 1  to print debug msgs, 0 to print nothing
#define PORT 8888
#define USE_LO 0

#define OPENNI_DEPTH_MAP 0 	// this is defined somewhere in an OpenGL header, but I can't find it!
#define OPENNI_BGR_IMAGE 5

using namespace std;


// *****************************************************************************
// READ ME!!!!!

// Global vars: if you edit anything here, also edit globals.h and (if need be) globals.cpp
cv::VideoCapture captureDepth;
cv::VideoCapture captureVideo;
cv::Mat depth;
cv::Mat image;

std::stringstream DepthStringStream;
std::stringstream ImageStringStream;

char* cstr2;
char DepthFileLengthString[10];
char ImageFileLengthString[10];
int is_data_ready = 0;
int serversock;
int clientsock;
int DepthDataLength;
int ImageDataLength;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct sigaction SignalActionManager;

// Function headers

void SignalHandler(int sigNum);
void* streamServer(void* arg);
void WriteDepthData(std::stringstream& _outstream, cv::Mat _depth);
void quit(char* msg, int retval);
cv::Mat visualizeDepth(cv::Mat DepthFrame);


/**
 * Main program
 */

int main(int argc, char **argv) {
    
	// Initialize + setup variables
	char key = '0';
	pthread_t thread_s;
	int lo_fd;

	// Set up signal handler, which will mainly be used to capture SIGINT and SIGPIPE
	SignalActionManager.sa_handler = SignalHandler;
	sigaction(SIGINT, &SignalActionManager, NULL);	
	sigaction(SIGPIPE, &SignalActionManager, NULL);
    

	// Initialize the liblo server, which permits system message passing over the OSC protocol
	/*    
	if (USE_LO == 1)
    {
		// Start a new server on port 7770
        lo_server_thread st = lo_server_thread_new("7770", error);

        // Add method that will match any path and args
        lo_server_thread_add_method(st, NULL, NULL, generic_handler, NULL);

        // Add method that will match the path /foo/bar, with two numbers, coerced to float and int
        lo_server_thread_add_method(st, "/foo/bar", "fi", foo_handler, NULL);

        // Add method that will match the path /quit with no args
        lo_server_thread_add_method(st, "/quit", "", quit_handler, NULL);

		// Add method to request captures        
		lo_server_thread_add_method(st, "/capture", "s", capture_handler, NULL);

		// Start the server
        lo_server_thread_start(st);
        lo_fd = lo_server_get_socket_fd(st);
    }
	*/
	
    // Initialize OpenCV video capture, make sure it opens correctly
	cv::VideoCapture captureVideo(CV_CAP_OPENNI_ASUS);
	if (!captureVideo.isOpened()) 
	{
	    fprintf(stderr, "Error initializing video capture! Make sure your webcam is connected.\n");
		return 1;
	}

	    // Run the streaming server as a separate thread
    if (pthread_create(&thread_s, NULL, streamServer, NULL)) 
	{
		quit("pthread_create failed.", 1);
    }

	// Initialize OpenCV Matrix objects
	// Reference: http://docs.opencv.org/2.4.6/doc/user_guide/ug_highgui.html
	cv::Mat image(640, 480, CV_8UC3); // initialize the RGB capture matrix
	cv::Mat depth(640, 480, CV_16UC1); // initialize the depth capture matrix

	// cv::namedWindow("stream_server");
    
	// Debug: show stream server video window
	// Main capture + display video loop
	while(1) 
	{
		// Capture an RGB frame. Note this MUST be done via grab() then retrieve(), *NOT* read()
		captureVideo.grab();	
		captureVideo.retrieve(image, OPENNI_BGR_IMAGE);
		if (image.empty()) 
		{
			printf("Error: Device not connected? - RGB capture failed\n");
			return -1;
		}

		// Capture a depth frame
		/*** Attempt 1 ***/		
		//captureVideo.grab();
		//captureVideo.retrieve(depth, OPENNI_DEPTH_MAP);
		/*** End Attempt 1 ***/
		/*** Attempt 2 ***/
		captureVideo.retrieve(depth, CV_CAP_OPENNI_DEPTH_MAP);
		
		/*** End Attempt 2 ***/

		

		cv::Mat DepthRGBFrame = visualizeDepth(depth);

		// Debug: convert the depth frame to RGB and display in the server window		
		//imshow("stream_server", DepthRGBFrame);

		if (depth.empty()) 
		{
			printf("Error: Device not connected? - Depth capture failed\n");
			return -1;
		}
		
		// Lock the thread       	
		pthread_mutex_lock(&mutex);
       			        
		// Clear the image and depth string streams
		ImageStringStream.str("");
		DepthStringStream.str("");

		// Convert image to a jpeg in system memory
		IplImage* ConvertedIplImage = new IplImage(image);
		writeJpeg(ImageStringStream, ConvertedIplImage);

		// Now append the depth frame to the jpeg in memory
		WriteDepthData(DepthStringStream, depth);

		// Resume the thread
        is_data_ready = 1;
        pthread_mutex_unlock(&mutex);

		// Check for key input which will break us out of the loop        
		key = cvWaitKey(2);
        if (key == 27) break;

    }
	

    // If user has pressed 'q', terminate the streaming server
    if (pthread_cancel(thread_s)) 
	{
        quit("pthread_cancel failed.", 1);
    }

    // Free memory + close window
    if(captureVideo.isOpened())
	{ 	
		captureVideo.release();
		image.release();
		//cvDestroyWindow("stream_server");
	}
	if(captureDepth.isOpened())
	{ 	
		captureDepth.release();
		depth.release();
	}

    // Graceful exit via quit, which shuts down the streaming server
    quit(NULL, 0);
}

/**
 * Signal Handler function is designed to handle two signals:
 * SIGINT: close all sockets and OpenCV objects and exit gracefully
 * SIGPIPE: handle network disconnection gracefully and wait to reconnect
 */

void SignalHandler(int sigNum)
{
	if(sigNum == SIGINT)
	{
		printf("\nCaught SIGINT, exiting gracefully...\n");
		quit(NULL, 0);
	}
	else if(sigNum == SIGPIPE)
	{
		// Do nothing. The thread will detect the dropped connection, and wait for reconnection.
		// However default Unix SIGPIPE handling shuts down the process entirely, so this fake
		// handler prevents that.
		//printf("Caught SIGPIPE...\n");

		// For now our dropped connection detection is gone, so just kill the program on SIGPIPE
		printf("\nCaught SIGPIPE, exiting gracefully...\n");
		quit(NULL, 0);
	}
}

/**
 * This is the streaming server, run as a separate thread
 * This function waits for a client to connect, and sends the compressed jpeg images
 * These images are contained in memory
 */
void* streamServer(void* arg) 
{
    struct sockaddr_in server;

    // Make this thread cancellable using pthread_cancel()
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // Open socket
    if ((serversock = socket(PF_INET, SOCK_STREAM, 0)) == -1) // TCP socket
	{
        quit("socket() failed", 1);
    }

    // Setup server IP and port
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(serversock, (const sockaddr*)&server, sizeof(server)) == -1) 
	{
		quit("Could not bind socket", 1);
    } 
	else 
	{
        printf("Socket successfully bound\n");
    }


    // Wait for connection
    if (listen(serversock, 10) == -1) 
	{
		printf("Failed, errno=%d\n", errno);
        quit("Could not listen for clients on socket", 1);
    } 
	else 
	{
        printf("Waiting for clients...\n");
    }

    // Accept a client
    if ((clientsock = accept(serversock, NULL, NULL)) == -1) 
	{
        quit("Client connection failed", 1);
    } 
	else 
	{
        printf("Client connected successfully!\n");
    }

    int bytes;
	int DepthStreamSize = 0;
    int ImageStreamSize = 0;

    // Start sending images
    while(1) 
	{
        // send the compressed jpeg data, thread safe
        pthread_mutex_lock(&mutex);

        if (is_data_ready) 
		{
			//printf("Sending data...\n");
			
			// Set up image data for socket connections
			ImageStringStream.seekg(0, ios::end);
            ImageStreamSize = ImageStringStream.tellg();
            sprintf(ImageFileLengthString, "%i", ImageStreamSize);
			//printf("ImageFileLengthString: %s\n", ImageFileLengthString);            
			//printf("Position in input stream: %d\n--\n", ImageStreamSize);

			// Set up depth data for socket connections
			DepthStringStream.seekg(0, ios::end);
            DepthStreamSize = DepthStringStream.tellg();
            sprintf(DepthFileLengthString, "%i", DepthStreamSize);
			//printf("DepthFileLengthString: %s\n", DepthFileLengthString);            
			//printf("Position in input stream: %d\n--\n", DepthStreamSize);
            
			if (DEBUG == 1)
			{
                cout << "Sending image data (size " << ImageStreamSize << " bytes), depth data (" << DepthStreamSize << " bytes)" << endl;	
			}
            
			bytes = send(clientsock, ImageFileLengthString, 10, 0);
            bytes = send(clientsock, ImageStringStream.str().c_str(), ImageStreamSize, 0);
			cout << "Sent " << bytes << " bytes of image data." << endl;
			bytes = send(clientsock, DepthFileLengthString, 10, 0);
            bytes = send(clientsock, DepthStringStream.str().c_str(), DepthStreamSize, 0);
			cout << "Sent " << bytes << " bytes of depth data." << endl;
            is_data_ready = 0;
        }
		//printf("Unlocking thread and clearing string stream...\n");
        pthread_mutex_unlock(&mutex);

		//printf("Checking if something went wrong...\n");
        // if something went wrong, restart the connection
		
		/* This error check doesn't work anymore, fix it
		if (bytes != ImageStreamSize) 
		{
			//printf("Something went wrong!\n");
            fprintf(stderr, "Connection closed, waiting for reconnect...\n");
            close(clientsock);

            if ((clientsock = accept(serversock, NULL, NULL)) == -1) 
			{
                quit("accept() failed", 1);
            }
        }
		*/

        // have we terminated yet?
        pthread_testcancel();

        // no, take a rest for a while
        usleep(1000);
    }
}

/**
 * Compress the depth data and add it to the output stream
 * zLib usage example: http://bobobobo.wordpress.com/2008/02/23/how-to-use-zlib/
 */

void WriteDepthData(std::stringstream& _outstream, cv::Mat _depth) 
{
	int SizeDataOriginal = 640*480*2;
	ulong SizeDataCompressed  = (SizeDataOriginal * 1.1) + 12;
	unsigned char* DataCompressed = (unsigned char*)malloc(SizeDataCompressed);

	int z_result = compress(
        
        DataCompressed,         // destination buffer,
                                // must be at least
                                // (1.01X + 12) bytes as large
                                // as source.. we made it 1.1X + 12bytes

        &SizeDataCompressed,    // pointer to var containing
                                // the current size of the
                                // destination buffer.
                                // WHEN this function completes,
                                // this var will be updated to
                                // contain the NEW size of the
                                // compressed data in bytes.

        _depth.datastart,           // source data for compression
        
        SizeDataOriginal ) ;

	switch( z_result )
    {
		case Z_OK:
			printf("***** SUCCESS! ***** Compressed size is %d bytes\n", SizeDataCompressed );
		    break;

		case Z_MEM_ERROR:
		    printf("out of memory\n");
		    exit(1);    // quit.
		    break;

		case Z_BUF_ERROR:
		    printf("output buffer wasn't large enough!\n");
		    exit(1);    // quit.
		    break;
    }

	// Write the compressed data to the output stream
	for(int i = 0; i < SizeDataCompressed; i++)
	{
		_outstream << (char)DataCompressed[i];
	}

	/*
	// Visit each node in the depth matrix and write values to the stringstream	
	for (int row = 0; row < _depth.rows; row ++)
	{
		for (int col = 0; col < _depth.cols; col ++)
		{
			unsigned short DepthValue = _depth.at<short>(row, col);
			unsigned short MajorDepthValue = DepthValue / 256;
			unsigned short MinorDepthValue = DepthValue % 256;
			//cout << "[" << row << "," << col << "]=" << _depth.at<short>(row, col) << ", Encoded=" << MajorDepthValue << MinorDepthValue << endl;
			_outstream << (char)MajorDepthValue << (char)MinorDepthValue;
			
		}
	}
	*/
}

/* 
 * Visualize depth data in RGB spectrum
 * Inputs: 
 * - xyzList = 3 * n where n=WIDTH*HEIGHT (x1, y1, z1,..., xn, yn, zn)
 * Outputs:
 * - reference to cv matrix 8-bit 3 channels
 */
cv::Mat visualizeDepth(cv::Mat DepthFrame) {
    cv::Mat depthRGB(480, 640, CV_8UC3);
    int lb, ub;
	unsigned short MajorDepthValue, MinorDepthValue;
	int DepthValue;
    
	//cout << "DepthFrame.size()=" << DepthFrame.size() << endl;
  
    for (int i = 0; i < 480*640; i++) {
		
		MajorDepthValue = DepthFrame.datastart[(i*2)+1];		
		MinorDepthValue = DepthFrame.datastart[i*2];
		
		DepthValue = (MajorDepthValue*256) + MinorDepthValue;
        lb = DepthValue/5 % 256;
        ub = DepthValue/5 / 256;
		
		//cout << "i=" << i << "DepthValue=" << DepthValue << ", ub=" << ub << endl;

        switch (ub) {
            case 0:
                depthRGB.datastart[3*i+2] = 255;
                depthRGB.datastart[3*i+1] = 255-lb;
                depthRGB.datastart[3*i+0] = 255-lb;
            break;
            case 1:
                depthRGB.datastart[3*i+2] = 255;
                depthRGB.datastart[3*i+1] = lb;
                depthRGB.datastart[3*i+0] = 0;
            break;
            case 2:
                depthRGB.datastart[3*i+2] = 255-lb;
                depthRGB.datastart[3*i+1] = 255;
                depthRGB.datastart[3*i+0] = 0;
            break;
            case 3:
                depthRGB.datastart[3*i+2] = 0;
                depthRGB.datastart[3*i+1] = 255;
                depthRGB.datastart[3*i+0] = lb;
            break;
            case 4:
                depthRGB.datastart[3*i+2] = 0;
                depthRGB.datastart[3*i+1] = 255-lb;
                depthRGB.datastart[3*i+0] = 255;
            break;
            case 5:
                depthRGB.datastart[3*i+2] = 0;
                depthRGB.datastart[3*i+1] = 0;
                depthRGB.datastart[3*i+0] = 255-lb;
            break;
            default:
                depthRGB.datastart[3*i+2] = 0;
                depthRGB.datastart[3*i+1] = 0;
                depthRGB.datastart[3*i+0] = 0;
            break;
        }
    }
    return depthRGB;
} 

/**
 * Quit function provides a graceful exit from the system
 */
void quit(char* msg, int retval) 
{
    if (retval == 0) 
	{
        fprintf(stdout, (msg == NULL ? "" : msg));
        fprintf(stdout, "\n");
    } 
	else 
	{
        fprintf(stderr, (msg == NULL ? "" : msg));
        fprintf(stderr, "\n");
    }

    if (clientsock) close(clientsock);
    if (serversock) close(serversock);
    
	if(captureVideo.isOpened())
	{ 	
		captureVideo.release();
		image.release();
	}
	if(captureDepth.isOpened())
	{
		captureDepth.release();
		depth.release();
	}

    pthread_mutex_destroy(&mutex);

    exit(retval);
}
