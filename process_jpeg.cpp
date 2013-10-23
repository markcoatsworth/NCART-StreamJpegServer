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
//#include <libfreenect/libfreenect_cv.h>
//#include "libfreenect_cv.h"

//Jpeg Lib includes
#include <jpeglib.h>

//Local includes
#include "process_jpeg.h"
#include "globals.h"

const static size_t JPEG_BUF_SIZE = 16384;

using namespace std;


/**
 *  USUAGE:
 *
 *  Read / write jpeg images using C++ iostreams and OpenCV images.
 *  Functions:
 *     void writeJpeg(std::stringstream& out_stream, const IplImage* img_);
 *         writes a jpeg to a stringstream.  This can also be changed to write
 *         directly to a file using ostream:
 *         E.g.> void writeJpeg(std::ostream& os, const IplImage* img);
 *
 *             Example usuage when writing to a file:
 *                 filebuf fb;
 *                 fb.open("test22.jpg", ios::out);
 *                 ostream out_stream(&fb);
 *                 writeJpeg(out_stream, img);
 *
 *     IplImage* readJpeg(std::stringstream& is);
 *          Reads a jpeg from a memory stream.  This can also be changed to read
 *          directly from a file using istream:
 *          E.g.> IplImage* readJpeg(std::istream& is);
 *                 filebuf fb;
 *                 fb.open("test22.jpg", ios::in);
 *                 istream in_stream(&fb);
 *                 readJpeg(in_stream);
 */




struct my_source_mgr {
    struct jpeg_source_mgr pub;
    std::istream* is;
    JOCTET*       buffer;
};


static void my_init_source(j_decompress_ptr cinfo) {}


static boolean my_fill_input_buffer(j_decompress_ptr cinfo) {
    my_source_mgr* src = (my_source_mgr*)cinfo->src;

    src->is->read((char*)src->buffer, JPEG_BUF_SIZE);
    size_t bytes = src->is->gcount();
    if (bytes == 0) {
        /* Insert a fake EOI marker */
        src->buffer[0] = (JOCTET) 0xFF;
        src->buffer[1] = (JOCTET) JPEG_EOI;
        bytes = 2;
    }
    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = bytes;
    return TRUE;
}


static void my_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    my_source_mgr* src = (my_source_mgr*)cinfo->src;
    if (num_bytes > 0) {
        while (num_bytes > (long)src->pub.bytes_in_buffer) {
            num_bytes -= (long)src->pub.bytes_in_buffer;
            my_fill_input_buffer(cinfo);
        }
        src->pub.next_input_byte += num_bytes;
        src->pub.bytes_in_buffer -= num_bytes;
    }
}


static void my_term_source(j_decompress_ptr cinfo) {
    // must seek backward so that future reads will start at correct place.
    my_source_mgr* src = (my_source_mgr*)cinfo->src;
    src->is->clear();
    src->is->seekg( src->is->tellg() - (std::streampos)src->pub.bytes_in_buffer );
}


static void my_set_source_mgr(j_decompress_ptr cinfo, std::istream& is) {
    my_source_mgr* src;
    if (cinfo->src == 0) {	/* first time? */
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)
                     ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));

        src = (my_source_mgr*) cinfo->src;
        src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)
                      ((j_common_ptr) cinfo, JPOOL_PERMANENT, JPEG_BUF_SIZE*sizeof(JOCTET));

    }

    src->is = &is;
    src->pub.init_source = my_init_source;
    src->pub.fill_input_buffer = my_fill_input_buffer;
    src->pub.skip_input_data = my_skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->pub.term_source = my_term_source;
    src->pub.bytes_in_buffer = 0;
    src->pub.next_input_byte = 0;
}


IplImage* readJpeg(std::stringstream& is) {
    IplImage* image = 0;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    // Setup decompression structure
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    my_set_source_mgr(&cinfo, is);

    // read info from header.
    int r = jpeg_read_header(&cinfo, TRUE);
    if (r != JPEG_HEADER_OK) 
	{
        printf("Failed to read JPEG header.");
    } 
	else if (cinfo.num_components != 3 && cinfo.num_components != 1) 
	{
        printf("Unsupported number of color components: %d", cinfo.num_components);
    } 
	else 
	{
        jpeg_start_decompress(&cinfo);
        // resize storage if necessary
        image = cvCreateImage(cvSize(cinfo.output_width, cinfo.output_height), IPL_DEPTH_8U, cinfo.output_components);
        JSAMPARRAY imageBuffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, cinfo.output_width*cinfo.output_components, 1);
        for (int y = 0; y < cinfo.output_height; y++) 
		{
            jpeg_read_scanlines(&cinfo, imageBuffer, 1);
            uint8_t* dstRow = (uint8_t*)image->imageData + image->widthStep*y;
            memcpy(dstRow, imageBuffer[0], cinfo.output_width*cinfo.output_components);
        }
        // for rgb images, reverse octets
        if (cinfo.num_components == 3) 
		{
            cvCvtColor(image, image, CV_RGB2BGR);
        }

        jpeg_finish_decompress(&cinfo);
    }

    // clean up.
    jpeg_destroy_decompress(&cinfo);

    //cvSaveImage("saved_jpeg.jpg", image, 0);

    return image;
}


struct my_destination_mgr {
    struct jpeg_destination_mgr pub; /* public fields */
    std::ostream* out_stream; /* target stream */
    JOCTET * buffer;	   /* start of buffer */
};


void my_init_destination (j_compress_ptr cinfo) {
    my_destination_mgr* dest = (my_destination_mgr*) cinfo->dest;
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                   JPEG_BUF_SIZE * sizeof(JOCTET));
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = JPEG_BUF_SIZE;
}


boolean my_empty_output_buffer(j_compress_ptr cinfo) {
    my_destination_mgr* dest = (my_destination_mgr*)cinfo->dest;

    dest->out_stream->write((const char*)dest->buffer, JPEG_BUF_SIZE);
    if (dest->out_stream->fail()) {
        printf("Couldn't write entire jpeg buffer to stream.");
    }
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = JPEG_BUF_SIZE;
    return TRUE;
}


void my_term_destination (j_compress_ptr cinfo) {
    my_destination_mgr* dest = (my_destination_mgr*) cinfo->dest;
    size_t datacount = JPEG_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0) {
        dest->out_stream->write((const char*)dest->buffer, datacount);
        if (dest->out_stream->fail()) {
            printf("Couldn't write remaining jpeg data to stream.");
        }
    }
    dest->out_stream->flush();
}


void my_set_dest_mgr(j_compress_ptr cinfo, std::ostream& out_stream) {
    my_destination_mgr* dest;

    if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)
                      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                              sizeof(my_destination_mgr));
    }

    dest = (my_destination_mgr*)cinfo->dest;
    dest->pub.init_destination = my_init_destination;
    dest->pub.empty_output_buffer = my_empty_output_buffer;
    dest->pub.term_destination = my_term_destination;
    dest->out_stream = &out_stream;
}


void writeJpeg(std::stringstream& out_stream, const IplImage* img_) {
    const IplImage* img = img_;
    IplImage* rgb = 0;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;

    //print//"%d %d\n", img->widthStep, img->imageSize);

    if (img->nChannels != 1 && img->nChannels != 3) {
        printf("Unsupported number of image channels.");
    }
    if (img->depth != IPL_DEPTH_8U) {
        printf("Unsupported image depth (server)\n");
    }
    // for rgb images, reverse octets
    if (img->nChannels == 3) {
        rgb = cvCreateImage(cvGetSize(img), img->depth, img->nChannels);
        cvCvtColor(img, rgb, CV_BGR2RGB);
        img = rgb;
    }

    // set up compression structure
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    my_set_dest_mgr(&cinfo, out_stream);

    cinfo.image_width      = img->width;
    cinfo.image_height     = img->height;
    cinfo.input_components = img->nChannels;
    if (img->nChannels == 3) {
        cinfo.in_color_space   = JCS_RGB;
    } else {
        cinfo.in_color_space   = JCS_GRAYSCALE;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality (&cinfo, 30, true); // set max quality. [0..100]
    jpeg_start_compress(&cinfo, true);

    JSAMPROW row;
    while (cinfo.next_scanline < cinfo.image_height) {
        row = (JSAMPROW)((char*)img->imageData + cinfo.next_scanline*img->widthStep);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    cvReleaseImage(&rgb); // release rgb scratch if it was used.
}
