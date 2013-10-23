#include <fstream>
#include <iostream>
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

// Local includes
#include "process_jpeg.h"
#include "globals.h"
#include "osc_handlers.h"

//Liblo includes
#include <lo/lo.h>

#define DEBUG 0         	// 1  to print debug msgs, 0 to print nothing
#define PORT 8888
#define USE_LO 1

#define OPENNI_BGR_IMAGE 5 	// this is defined somewhere in an OpenGL header, but I can't find it!

using namespace std;


// *****************************************************************************
// READ ME!!!!!

// Global vars: if you edit anything here, also edit globals.h and (if need be) globals.cpp
cv::VideoCapture capture;
cv::Mat image;
char* cstr2;
char charFileLength[10];
int is_data_ready = 0;
int serversock;
int clientsock;
int fileLength;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void* streamServer(void* arg);
void quit(char* msg, int retval);


int main(int argc, char **argv) {
    
	// Initialize + setup variables
	char key = '0';
	pthread_t thread_s;
	int lo_fd;
    

	// Initialize the liblo server, which permits system message passing over the OSC protocol
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
	
    // Initialize video capture, make sure it opens correctly
	cv::VideoCapture capture(CV_CAP_OPENNI);
	if (!capture.isOpened()) 
	{
	    fprintf(stderr, "Cannot initialize webcam!\n");
		return 1;
	}

    // Run the streaming server as a separate thread
    if (pthread_create(&thread_s, NULL, streamServer, NULL)) 
	{
		quit("pthread_create failed.", 1);
    }

	// Set up the live stream_server window	
	cv::namedWindow("stream_server", CV_WINDOW_AUTOSIZE);


   	// Main capture + display video loop
	cv::Mat image(640, 480, CV_8UC3); // initialize the capture matrix
    for (int i = 0; i < 1000; i ++) 
	{
		// Capture a frame.	Note this MUST be done via grab() then retrieve(), *NOT* read()
		capture.grab();	
		capture.retrieve(image, OPENNI_BGR_IMAGE);
		if (image.empty()) 
		{
			printf("Error: Device not connected? - RGB capture failed\n");
			return -1;
		}

		// Lock the thread       	
		pthread_mutex_lock(&mutex);
       			        
		// Convert image to a jpeg in system memory, and display in the stream_server window
		IplImage* ConvertedIplImage = new IplImage(image);	
		writeJpeg(stringstrm, ConvertedIplImage);
		imshow("stream_server", image);

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
    if(capture.isOpened())
	{ 	
		capture.release();
		image.release();
		cvDestroyWindow("stream_server");
	}

    // Graceful exit via quit, which shuts down the streaming server
    quit(NULL, 0);
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
    if ((serversock = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
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
        quit("bind() failed", 1);
    } 
	else 
	{
        printf("bind successful\n");
    }


    // Wait for connection
    if (listen(serversock, 10) == -1) 
	{
        quit("listen() failed.", 1);
    } 
	else 
	{
        printf("listen successful\n");
    }


    // Accept a client
    if ((clientsock = accept(serversock, NULL, NULL)) == -1) 
	{
        quit("accept() failed", 1);
    } 
	else 
	{
        printf("accept successful\n");
    }

    int bytes;
    int size_ss = 0;

    // Start sending images
    while(1) 
	{
        // send the compressed jpeg data, thread safe
        pthread_mutex_lock(&mutex);
        if (is_data_ready) 
		{
            stringstrm.seekg(0, ios::end);
            size_ss = stringstrm.tellg();
            //printf("before: %d\n", size_ss);
            //size_ss += DEPTH_IMG_SIZE;
            //printf("after: %d\n", size_ss);
            sprintf(charFileLength, "%i", size_ss);
            if (DEBUG == 1)
			{
                printf("%d ----------------- %s\n", size_ss, charFileLength);
			}
            bytes = send(clientsock, charFileLength, 10, 0);
            //printf("BYTES: %d\n", bytes);
            //printf("Sent first\n");
            bytes = send(clientsock, stringstrm.str().c_str(), size_ss, 0);
            //printf("BYTES: %d\n", bytes);

            is_data_ready = 0;
        }

        pthread_mutex_unlock(&mutex);
        stringstrm.str(""); //clear the stringstream

        // if something went wrong, restart the connection
        if (bytes != size_ss) 
		{
            fprintf(stderr, "Connection closed.\n");
            close(clientsock);

            if ((clientsock = accept(serversock, NULL, NULL)) == -1) 
			{
                quit("accept() failed", 1);
            }
        }


        // have we terminated yet?
        pthread_testcancel();

        // no, take a rest for a while
        usleep(1000);
    }
}

/**
 * this function provides a way to exit nicely from the system
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
    if(capture.isOpened())
	{ 	
		capture.release();
		image.release();
	}

    pthread_mutex_destroy(&mutex);

    exit(retval);
}
