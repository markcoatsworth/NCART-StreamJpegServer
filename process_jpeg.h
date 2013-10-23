#ifndef PROCESS_JPEG_H_INCLUDED
#define PROCESS_JPEG_H_INCLUDED


#include <stdio.h>
#include <iostream>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <sstream>

//Socket includes
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//OpenCV2.2 includes
#include <opencv/cv.h>
#include <opencv/highgui.h>
//#include "libfreenect_cv.h"

//Jpeg Lib includes
#include <jpeglib.h>

//Local includes
#include "process_jpeg.h"
#include "globals.h"


static void my_init_source(j_decompress_ptr cinfo);
static boolean my_fill_input_buffer(j_decompress_ptr cinfo);
static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes);
static void my_term_source(j_decompress_ptr cinfo);
static void my_set_source_mgr(j_decompress_ptr cinfo, std::istream& is);
IplImage* readJpeg(std::stringstream& is);
void my_init_destination (j_compress_ptr cinfo);
boolean my_empty_output_buffer(j_compress_ptr cinfo);
void my_term_destination (j_compress_ptr cinfo);
void my_set_dest_mgr(j_compress_ptr cinfo, std::ostream& out_stream);
void writeJpeg(std::stringstream& out_stream, const IplImage* img_);

#endif // PROCESS_JPEG_H_INCLUDED
