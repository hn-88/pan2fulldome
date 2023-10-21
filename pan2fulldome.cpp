/*
 * pan2fulldome.cpp
 * 
 * Warps "flat" panoramic images to fit 180 degree fisheye "fulldome masters" using the OpenCV framework. 
 * Appends 'F.jpg' to the filename and saves in the same folder by default.
 * 
 * first code edits:
 * Hari Nandakumar
 * 6 Jul 2023
 * 
 * 
 */

//#define _WIN64
//#define __unix__

// references 
// https://hnsws.blogspot.com/2012/11/displaying-panoramas-on-fulldome.html
// https://github.com/hn-88/OCVvid2fulldome
// https://github.com/hn-88/OCVWarp/
// https://github.com/Dovyski/cvui

#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <time.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include "tinyfiledialogs.h"

#define CVUI_IMPLEMENTATION
#include "cvui.h"

#define WINDOW_NAME "PAN2FULLDOME - HIT <esc> TO CLOSE"

#define CV_PI   3.1415926535897932384626433832795

cv::Mat ocvwarp1(cv::Mat equirect, int rotate_down, int outputw, int outputh) {
	// from https://github.com/hn-88/OCVWarp/blob/master/OCVWarp.cpp
	// line 924
	cv::Size Sout = cv::Size(outputw,outputh);
	// taking vars from line 955
	cv::Mat src, res, tmp;
	cv::Mat dstfloat, dstmult, dstres, dstflip;
	    
	std::vector<cv::Mat> spl;
	cv::Mat dst(Sout, CV_8UC3); // S = src.size, and src.type = CV_8UC3
	cv::Mat dst2;	// temp dst, for double remap
	cv::Mat dst_x, dst_y;

	//////////////////////////////////////////////
	// Equirectangular 360 to 180 degree fisheye
	// from void update_map( double anglex, double angley, Mat &map_x, Mat &map_y, int transformtype )
	
		// using the transformations at
		// http://paulbourke.net/dome/dualfish2sphere/diagram.pdf
  		cv::Mat map_x, map_y; //  initialize these, line 987
		map_x = cv::Mat(Sout, CV_32FC1);
		map_y = cv::Mat(Sout, CV_32FC1);
		// line 1003
		map_x = cv::Scalar((outputw+outputh)*10);
    		map_y = cv::Scalar((outputw+outputh)*10);
    		// initializing so that it points outside the image
    		// so that unavailable pixels will be black
	
		int xcd = floor(map_x.cols/2) - 1 ;
		int ycd = floor(map_x.rows/2) - 1 ;
		float halfcols = map_x.cols/2;
		float halfrows = map_x.rows/2;

		int anglex = -90;
		int angley = rotate_down;		
		
		float longi, lat, Px, Py, Pz, theta;						// X and Y are map_x and map_y
		float xfish, yfish, rfish, phi, xequi, yequi;
		float PxR, PyR, PzR;
		float aperture = CV_PI;
		float angleyrad = -angley*CV_PI/180;	// made these minus for more intuitive feel
		float anglexrad = -anglex*CV_PI/180;
		
		//Mat inputmatrix, rotationmatrix, outputmatrix;
		// https://en.wikipedia.org/wiki/Rotation_matrix#Basic_rotations
		//rotationmatrix = (Mat_<float>(3,3) << cos(angleyrad), 0, sin(angleyrad), 0, 1, 0, -sin(angleyrad), 0, cos(angleyrad)); //y
		//rotationmatrix = (Mat_<float>(3,3) << 1, 0, 0, 0, cos(angleyrad), -sin(angleyrad), 0, sin(angleyrad), cos(angleyrad)); //x
		//rotationmatrix = (Mat_<float>(3,3) << cos(angleyrad), -sin(angleyrad), 0, sin(angleyrad), cos(angleyrad), 0, 0, 0, 1); //z
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					// normalizing to [-1, 1]
					xfish = (j - xcd) / halfcols;
					yfish = (i - ycd) / halfrows;
					rfish = sqrt(xfish*xfish + yfish*yfish);
					theta = atan2(yfish, xfish);
					phi = rfish*aperture/2;
					
					// Paul's co-ords - this is suitable when phi=0 is Pz=0
					
					//Px = cos(phi)*cos(theta);
					//Py = cos(phi)*sin(theta);
					//Pz = sin(phi);
					
					// standard co-ords - this is suitable when phi=pi/2 is Pz=0
					Px = sin(phi)*cos(theta);
					Py = sin(phi)*sin(theta);
					Pz = cos(phi);
					
					if(angley!=0 || anglex!=0)
					{
						// cos(angleyrad), 0, sin(angleyrad), 0, 1, 0, -sin(angleyrad), 0, cos(angleyrad));
						
						PxR = Px;
						PyR = cos(angleyrad) * Py - sin(angleyrad) * Pz;
						PzR = sin(angleyrad) * Py + cos(angleyrad) * Pz;
						
						Px = cos(anglexrad) * PxR - sin(anglexrad) * PyR;
						Py = sin(anglexrad) * PxR + cos(anglexrad) * PyR;
						Pz = PzR;
					}
					
					
					longi 	= atan2(Py, Px);
					lat	 	= atan2(Pz,sqrt(Px*Px + Py*Py));	
					// this gives south pole centred, ie yequi goes from [-1, 0]
					// Made into north pole centred by - (minus) in the final map_y assignment
					
					xequi = longi / CV_PI;
					// this maps to [-1, 1]
					yequi = 2*lat / CV_PI;
					// this maps to [-1, 0] for south pole
					
					//if (rfish <= 1.0)		// outside that circle, let it be black
					// removed the black circle to help transformtype=5
					// avoid bottom pixels black
					{
						map_x.at<float>(i, j) =  abs(xequi * map_x.cols / 2 + xcd);
						//map_y.at<float>(i, j) =  yequi * map_x.rows / 2 + ycd;
						// this gets south pole centred view
						
						// the abs is to correct for -0.5 xequi value at longi=0
						
						map_y.at<float>(i, j) =  yequi * map_x.rows / 2 + ycd;
						//debug
						//~ if (rfish <= 1.0/500)
						//if ((longi==0)||(longi==CV_PI)||(longi==-CV_PI))
						//if (lat==0)	// since these are floats, probably doesn't work
						//~ {
							//~ std::cout << "i,j,mapx,mapy=";
							//~ std::cout << i << ", ";
							//~ std::cout << j << ", ";
							//~ std::cout << map_x.at<float>(i, j) << ", ";
							//~ std::cout << map_y.at<float>(i, j) << std::endl;
						//~ }
					}
					
				 } // for j
				   
			} // for i
	// this completes update_map()
	////////////////////////////////
	cv::convertMaps(map_x, map_y, dst_x, dst_y, CV_16SC2);	// supposed to make it faster to remap
	cv::resize( src, res, cv::Size(outputw, outputh), 0, 0, cv::INTER_CUBIC);
	cv::remap( res, dst, dst_x, dst_y, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0) );
	return dst;			
	
}

cv::Mat equirectToFisheye(cv::Mat inputMat, int sky_threshold, int horizontal_extent, int move_down, int rotate_down, int outputw)
{
	int equirectw = 8192;
	int equirecth = 4096;
	// set intermediate equirect image size
	if (outputw < 513) {
		equirectw=1024;
		equirecth=512;
	}
	if (outputw < 1025) {
		equirectw=2048;
		equirecth=1024;
	}
	if (outputw < 2049) {
		equirectw=4096;
		equirecth=2048;
	}
	// sky_threshold has a range 0 to 400. scaling this to 0 to Input Mat h
	sky_threshold = (int)((float)inputMat.rows/400.)*sky_threshold;
	// horizontal_extent has a range 0 to 360. scaling this to 0 to equirectw
	// https://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
	horizontal_extent = ceil(((float)equirectw/360.)*(float)horizontal_extent);
	// move_down has a range 0 to 400. scaling this to 0 to half of equirecth
	move_down = (int)((float)equirecth/800.)*move_down;
	
	cv::Mat dst, dst2, tmp, tmpcropped, sky, equirect;
	cv::Size dstsize = cv::Size(outputw,outputw);
	// for testing large Mat, 
	cv::Size equirectsize = cv::Size(equirectw,equirecth);
	// initialize dst with the same datatype as inputMat
	// cv::resize(inputMat, dst, dstsize, 0, 0, cv::INTER_CUBIC);
	// with the "sky" region stretched to fit
	// For now, we take the sky to be the top 5 pixels of inputMat if sky_threshold is very small
	if (sky_threshold < 5) {
		inputMat.rowRange(0,5).copyTo(sky);
	}
	else {
		inputMat.rowRange(0,sky_threshold).copyTo(sky);
	}
	cv::resize(sky, dst, dstsize, 0, 0, cv::INTER_LINEAR);
	cv::resize(sky, equirect, equirectsize, 0, 0, cv::INTER_LINEAR);
	// we want the tmp to contain the inputMat without any distortion, 
	// resized with x/y aspect ratio unchanged.
	cv::resize(inputMat, tmp, cv::Size(horizontal_extent, ceil(inputMat.rows*(float)horizontal_extent/(float)inputMat.cols)), 0, 0, cv::INTER_CUBIC);
	//tmp.rowRange(1, outputw-sky_threshold).copyTo(dst.rowRange(sky_threshold+1, outputw));
	int x =  (int)(equirectw-horizontal_extent)/2;
	int y =  move_down;
	if ((y+tmp.rows) > equirect.rows) {
		// then we have to truncate tmp
		// and remember first y, then x
		tmpcropped = tmp(cv::Range(0,equirect.rows-y), cv::Range(0,horizontal_extent));
	}
	else {
		tmpcropped = tmp;
	}
		
	if (x<2) { x=0;}
	if (y<(inputMat.rows-2)) {// otherwise don't copy, since tmp may be too small
		tmpcropped.copyTo(equirect(cv::Rect(x,y,tmpcropped.cols, tmpcropped.rows)));
	}
	// todo the equirectToFisheye here
	//cv::resize(equirect, dst, dstsize, 0, 0, cv::INTER_LINEAR);// this is just for testing.
	dst = ocvwarp1(equirect, rotate_down, outputw, outputw)
	// "horiz extent" would determine the "zoom" level
	// "rotate_down" would determine the angle tilt above or below the horizon
	return dst;
}

void makesmall_map( int vidw, float aspectratio, cv::Mat &maps_x, cv::Mat &maps_y )
{
	int wpix = ((float)vidw/360.0 ) *  maps_x.cols;
	int hpix = round((float)wpix/aspectratio)*2;	// the 2* is due to converting
											// equirect 2:1 image into a square outpu
	 
	int leftmargin = (maps_x.cols - wpix) / 2;
	int topmargin = (maps_x.rows - hpix) / 2;
	int botmargin = topmargin+hpix;
	int rightmargin = leftmargin+wpix;
	
	// maps_x in [leftmargin, leftmargin+wpix] maps to [0,maps_x.cols-1] in src
	
	for(int j = topmargin; j < botmargin; j++ )	// j is for y, i is for x
	{ 
		for( int i = leftmargin; i < rightmargin; i++ )
		{
			maps_y.at<float>(j,i) = (float)(maps_x.rows-1) * (botmargin-j)  / (hpix);
			maps_x.at<float>(j,i) = maps_x.cols - (float)(maps_x.cols-1) * (i-leftmargin) / (wpix);			
		}
	}
	
	
}

void update_map( int vidlongi, int vidlati, int vidw, float aspectratio, cv::Mat &map_x, cv::Mat &map_y )
{
		// direct copy-paste of OCVWarp code, transformtype=1
		int xcd = floor(map_x.cols/2) - 1 ;
		int ycd = floor(map_x.rows/2) - 1 ;
		float halfcols = map_x.cols/2;
		float halfrows = map_x.rows/2;
		
		
		float longi, lat, Px, Py, Pz, theta;						// X and Y are map_x and map_y
		float xfish, yfish, rfish, phi, xequi, yequi;
		float PxR, PyR, PzR;
		float aperture = CV_PI;
		float angleyrad = -vidlati*CV_PI/180;
		float anglexrad = -90.0*CV_PI/180;
		double angle = (vidlongi - 180) ; // this parameter is in degrees.
		cv::Mat rot_mat;
		cv::Point2f image_centre(map_x.cols/2,map_x.rows/2);
		
		cv::Mat mapb_x, mapb_y;
		map_x.copyTo(mapb_x);
		map_y.copyTo(mapb_y);
		
		for ( int i = 0; i < map_x.rows; i++ ) // here, i is for y and j is for x
			{
				for ( int j = 0; j < map_x.cols; j++ )
				{
					// normalizing to [-1, 1]
					xfish = (j - xcd) / halfcols;
					yfish = (i - ycd) / halfrows;
					rfish = sqrt(xfish*xfish + yfish*yfish);
					theta = atan2(yfish, xfish);
					phi = rfish*aperture/2;
					
					// standard co-ords - this is suitable when phi=pi/2 is Pz=0
					Px = sin(phi)*cos(theta);
					Py = sin(phi)*sin(theta);
					Pz = cos(phi);
					
					
					PxR = Px;
					PyR = cos(angleyrad) * Py - sin(angleyrad) * Pz;
					PzR = sin(angleyrad) * Py + cos(angleyrad) * Pz;
					
					Px = cos(anglexrad) * PxR - sin(anglexrad) * PyR;
					Py = sin(anglexrad) * PxR + cos(anglexrad) * PyR;
					Pz = PzR;
					
					longi 	= atan2(Py, Px);
					lat	 	= atan2(Pz,sqrt(Px*Px + Py*Py));	
					// this gives south pole centred, ie yequi goes from [-1, 0]
					// Made into north pole centred by - (minus) in the final map_y assignment
					
					xequi = longi / CV_PI;
					// this maps to [-1, 1]
					yequi = 2*lat / CV_PI;
					// this maps to [-1, 0] for south pole
					
					mapb_x.at<float>(i, j) =  abs(xequi * map_x.cols / 2 + xcd);
					mapb_y.at<float>(i, j) =  yequi * map_x.rows / 2 + ycd;
					
					
				 } // for j
				   
			} // for i
			
			// now rotate the map
			rot_mat=cv::getRotationMatrix2D(image_centre, angle, 1.0);
			cv::warpAffine(mapb_x, map_x, rot_mat, mapb_x.size());
			cv::warpAffine(mapb_y, map_y, rot_mat, mapb_x.size());
  
				
}



cv::Mat simplePolar(cv::Mat inputMat, int sky_threshold, int horizontal_extent, int outputw)
{
	// sky_threshold has a range 0 to 400. scaling this to 0 to outputw
	sky_threshold = (int)((float)outputw/400.)*sky_threshold;
	// horizontal_extent has a range 1 to 360. scaling this to 0 to outputw
	horizontal_extent = (int)((float)outputw/360.)*horizontal_extent;
	cv::Mat dst, tmp, sky;
	cv::Size dstsize = cv::Size(outputw,outputw);
	// initialize dst with the same datatype as inputMat
	// cv::resize(inputMat, dst, dstsize, 0, 0, cv::INTER_CUBIC);
	// with the "sky" region stretched to fit
	// For now, we take the sky to be the top 5 pixels of inputMat
	inputMat.rowRange(1,5).copyTo(sky);
	cv::resize(sky, dst, dstsize, 0, 0, cv::INTER_CUBIC);

	cv::resize(inputMat, tmp, cv::Size(horizontal_extent, outputw-sky_threshold), 0, 0, cv::INTER_CUBIC);
	//tmp.rowRange(1, outputw-sky_threshold).copyTo(dst.rowRange(sky_threshold+1, outputw));
	int x =  (int)(outputw-horizontal_extent)/2;
	int y =  sky_threshold;
	if (x<2) { x=0;}
	if (y<398) {// otherwise don't copy, since tmp may be too small
	tmp.copyTo(dst(cv::Rect(x,y,tmp.cols, tmp.rows)));
	}
	
	cv::Point2f centrepoint( (float)dst.cols / 2, (float)dst.rows / 2 );
	double maxRadius = (double)dst.cols / 2;
	    
	int flags = cv::INTER_CUBIC + cv::WARP_FILL_OUTLIERS + cv::WARP_INVERSE_MAP;
	cv::rotate(dst, dst, cv::ROTATE_90_COUNTERCLOCKWISE);
	// this rotate is needed, since opencv's warpPolar() is written that way
	
	cv::warpPolar(dst, dst, dstsize, centrepoint, maxRadius, flags);
		    // one more rotate is needed
	cv::rotate(dst, dst, cv::ROTATE_90_COUNTERCLOCKWISE);
	return dst;
}
	

std::string escaped(const std::string& input)
{
	// https://stackoverflow.com/questions/48260879/how-to-replace-with-in-c-string
	// needed for windows paths for opencv
	std::string output;
	output.reserve(input.size());
	for (const char c: input) {
		switch (c) {
		    case '\a':  output += "\\a";        break;
		    case '\b':  output += "\\b";        break;
		    case '\f':  output += "\\f";        break;
		    case '\n':  output += "\\n";        break;
		    case '\r':  output += "\\r";        break;
		    case '\t':  output += "\\t";        break;
		    case '\v':  output += "\\v";        break;
		    default:    output += c;            break;
		}
	}
	return output;
}

int main(int argc,char *argv[])
{
bool doneflag = 0;
bool showdisplay = 1, interactivemode=0;
bool skipinputs = 0;
int outputw;
std::string PanFileName;
std::string NAME;
float aspectratio[100];
int looptemp=0;
std::string escapedpath;
std::string escapedsavepath;
    
char const * lTmp;
char * ptr;

cv::Mat src, dst, res, dstdisplay;
cv::Mat dstfloat, dstmult, dstres, dstflip;
cv::Size Sout;

std::vector<cv::Mat> spl;
cv::Mat dst2, dst3, dsts;	// temp dst, for eachvid

    
    if(argc <= 1)
    {
		char const * FilterPatternsimg[2] =  { "*.jpg","*.png" };
		char const * OpenFileNameimg;
		
		OpenFileNameimg = tinyfd_openFileDialog(
				"Open input pan image file",
				"",
				2,
				FilterPatternsimg,
				NULL,
				0);

		if (! OpenFileNameimg)
		{
			skipinputs = 0;
		}
		else
		{
			skipinputs = 1;
			escapedpath = escaped(std::string(OpenFileNameimg));
		}
	} // end if arc <= 1
	    
    if(argc > 1)
    {
		// argument can be ini file path
		skipinputs = 1;
		escapedpath = argv[1];
    }
	// https://www.oreilly.com/library/view/c-cookbook/0596007612/ch10s17.html
	
    escapedsavepath = escapedpath;
    std::string::size_type i = escapedsavepath.rfind('.', escapedsavepath.length());

   if (i != std::string::npos) {
      escapedsavepath.replace(i, 5, "F.jpg");
   }
	else
   {
	   escapedsavepath = escapedsavepath + "F.jpg";
   }
	
    if(skipinputs==1)
    {	
	lTmp = tinyfd_inputBox(
		"Please Input", "Output image width (=height)", "1024");
	if (!lTmp) return 1 ;
	
	outputw = atoi(lTmp);
	cv::Mat img = cv::imread(escapedpath, cv::IMREAD_COLOR);
		
	cv::Size dstdisplaysize = cv::Size(400,400);
	cv::Size dstsize = cv::Size(outputw,outputw);
	
	dstdisplay = equirectToFisheye(img, 0, 360, 0, -160, 400);
	
	if(img.empty())
		 {
		 std::cout << "Could not read the image: " << escapedpath << std::endl;
		 return 1;
		 }
	
	////////// CVUI ///////////////
	// Create a frame where components will be rendered to.
	cv::Mat frame = cv::Mat(680, 680, CV_8UC3);
	int sky_threshold = 0;
	int horizontal_extent = 360;
	int move_down = 0;
	int rotate_down = -160;

	// Init cvui and tell it to create a OpenCV window, i.e. cv::namedWindow(WINDOW_NAME).
	cvui::init(WINDOW_NAME);

	while (true) {
		// Fill the frame with a nice color
		frame = cv::Scalar(49, 52, 49);

		// Render UI components to the frame
		cvui::text(frame, 350, 10, "Preview");
		cvui::button(frame, 140, 30, dstdisplay, dstdisplay, dstdisplay);

		cvui::text(frame, 35, 580, "Sky");
		if (cvui::trackbar(frame, 15, 600, 135, &sky_threshold, 0, 400)) {
			if (sky_threshold > 395) { 
				sky_threshold = 395;  // to prevent crashes
			}
			dstdisplay = equirectToFisheye(img, sky_threshold, horizontal_extent, move_down, rotate_down, 400);
		}

		cvui::text(frame, 170, 580, "Horizontal extent");
		if (cvui::trackbar(frame, 165, 600, 135, &horizontal_extent, 1, 360)) {
			if (horizontal_extent < 5) {
				horizontal_extent = 5;   // to prevent crashes
			}
			dstdisplay = equirectToFisheye(img, sky_threshold, horizontal_extent, move_down, rotate_down, 400);
		}

		cvui::text(frame, 335, 580, "Move down");
		if (cvui::trackbar(frame, 315, 600, 135, &move_down, 0, 400)) {
			if (move_down > 395) {
				move_down = 395;   // to prevent crashes
			}
			dstdisplay = equirectToFisheye(img, sky_threshold, horizontal_extent, move_down, rotate_down, 400);
		}

		cvui::text(frame, 485, 580, "Rotate down");
		if (cvui::trackbar(frame, 465, 600, 200, &rotate_down, -180, 180)) {
			if (rotate_down > 355) {
				rotate_down = 355;   // to prevent crashes
			}
			dstdisplay = equirectToFisheye(img, sky_threshold, horizontal_extent, move_down, rotate_down, 400);
		}

		if (cvui::button(frame, 350, 650, "Close")) {
		    // close button was clicked
			break;
		}
		if (cvui::button(frame, 200, 650, "Save")) {
		    // save button was clicked
		    dst = equirectToFisheye(img, sky_threshold, horizontal_extent, move_down, rotate_down, outputw);
			// ask for filename
			char const * FilterPatternsimgsave[2] =  { "*.jpg","*.png" };
			char const * SaveFileNameimg;
		
			SaveFileNameimg = tinyfd_saveFileDialog(
				"Output image file",
				"",
				2,
				FilterPatternsimgsave,
				NULL);

			if (SaveFileNameimg) {			
				escapedsavepath = escaped(std::string(SaveFileNameimg));
				////////////////////
			    cv::imwrite(escapedsavepath, dst);
			}
		}
		
		// Update cvui stuff and show everything on the screen
		cvui::imshow(WINDOW_NAME, frame);

		if (cv::waitKey(20) == 27) { // ESC was pressed
			break;
		}
		
	}

	return 0;		
		
	} // end if skipinputs
	    
	std::cout << std::endl << "Finished writing." << std::endl;
	return 0; 
}
