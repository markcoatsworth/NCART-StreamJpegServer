#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED

#include <sstream>
#include <iostream>
#include <opencv2/opencv.hpp>


#define DIV 1

extern std::stringstream DepthStringStream; //(stringstream::in | stringstream::out);
extern std::stringstream ImageStringStream; //(stringstream::in | stringstream::out);
extern int serversock, clientsock;
extern pthread_mutex_t mutex;
extern int is_data_ready;
extern int fileLength;
extern char charFileLength[10];
extern cv::VideoCapture capture;
extern IplImage *compressed_img;
extern int done;
extern int capture_dev;
extern int restart_server;
extern int width;
extern int height;


#endif // GLOBALS_H_INCLUDED
