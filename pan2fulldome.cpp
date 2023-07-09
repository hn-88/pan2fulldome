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

#define WINDOW_NAME "CVUI"

#define CV_PI   3.1415926535897932384626433832795

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
int panw;
float aspectratio[100];
int looptemp=0;
std::string escapedpath;
int  fps, key;
int t_start, t_end;
    
char const * lTmp;
char * ptr;

cv::Mat src, dst, res;
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
	
    if(skipinputs==1)
    {	
	lTmp = tinyfd_inputBox(
		"Please Input", "Output video width (=height)", "1024");
	if (!lTmp) return 1 ;
	
	outputw = atoi(lTmp);
	cv::Mat img = cv::imread(escapedpath, cv::IMREAD_COLOR);
	
	if(img.empty())
		 {
		 std::cout << "Could not read the image: " << escapedpath << std::endl;
		 return 1;
		 }
	cv::imshow("Input image", img);
	
	////////// CVUI ///////////////
	    // Create a frame where components will be rendered to.
	cv::Mat frame = cv::Mat(600, 600, CV_8UC3);

	// Init cvui and tell it to create a OpenCV window, i.e. cv::namedWindow(WINDOW_NAME).
	cvui::init(WINDOW_NAME);

	while (true) {
		// Fill the frame with a nice color
		frame = cv::Scalar(49, 52, 49);

		// Render UI components to the frame
		cvui::text(frame, 110, 80, "Hello, world!");
		cvui::text(frame, 110, 120, "cvui is awesome!");
		cvui::button closeButton = cvui::button(frame, 500, 500, "Close");
	

		// Update cvui stuff and show everything on the screen
		cvui::imshow(WINDOW_NAME, frame);

		if (cv::waitKey(20) == 27) {
			break;
		}
		if (closeButton) {
		    // close button was clicked
			break;
		}
	}

	return 0;		
		
	} // end if skipinputs
	    /*
	    
		if (infile.is_open())
		  {
			  try
			  {
				std::getline(infile, tempstring);
				std::getline(infile, tempstring);
				std::getline(infile, tempstring);
				// first three lines of ini file are comments
				std::getline(infile, NAME);
				infile >> tempstring;
				infile >> outputw;
				infile >> tempstring;
				infile >> outputfps;
				infile >> tempstring;
				infile >> outputfourcc;
				infile >> tempstring;
				infile >> numvids;
				// https://stackoverflow.com/questions/6649852/getline-not-working-properly-what-could-be-the-reasons
				// dummy getline after the >> on previous line
				std::getline(infile, tempstring);
				
				for(int i=0; i<numvids; i++)
				{
					std::getline(infile, tempstring);
					std::getline(infile, tempstring);
					VidFileName[i] = tempstring;
					infile >> tempstring;
					infile >> vidlongi[i];
					infile >> tempstring;
					infile >> vidlati[i];
					infile >> tempstring;
					infile >> vidw[i];
					// dummy getline after the >> on previous line
					std::getline(infile, tempstring);
				}
				infile.close();
				skipinputs = 1;
			  } // end try
			  catch(std::ifstream::failure &readErr) 
				{
					std::cerr << "\n\nException occured when reading ini file\n"
						<< readErr.what()
						<< std::endl;
					skipinputs = 0;
				} // end catch
			 
		  } // end if isopen
		  
	  } // end if argc > 1

	
	if(!skipinputs)
	{
	// better to use getline than cin
	// https://stackoverflow.com/questions/4999650/c-how-do-i-check-if-the-cin-buffer-is-empty
	tinyfd_messageBox("Please Note", "ini file not supplied or unreadable. So, manual inputs ...", "ok", "info", 1);
	//std::cout << "Enter Output video width (=height): ";
	
	lTmp = tinyfd_inputBox(
		"Please Input", "Output video width (=height)", "1024");
	if (!lTmp) return 1 ;
	
	outputw = atoi(lTmp);
	//std::cin >> outputw;
	
	//~ std::cout << "Enter Output video framerate (fps): ";
	//~ std::cin >> outputfps;
	lTmp = tinyfd_inputBox(
		"Please Input", "Desired output video framerate (fps)", "30");
	if (!lTmp) return 1 ;
	outputfps = strtod(lTmp, &ptr);
	if (outputfps == 0.0) return 1 ;
	
	//~ std::cout << "Enter Output video fourcc (default is XVID): ";
	//~ // a dummy getline is needed
	//~ std::getline(std::cin, tempstring);
	//~ std::getline(std::cin, fourccstr);
	
	lTmp = tinyfd_inputBox(
		"Please Input", "Output video fourcc", "XVID");
	if (!lTmp) return 1 ;
	fourccstr = lTmp;
	
	if(fourccstr.empty())
	{
		// use the default
	}
	else
	{
		outputfourcc[0]=fourccstr.at(0);
		outputfourcc[1]=fourccstr.at(1);
		outputfourcc[2]=fourccstr.at(2);
		outputfourcc[3]=fourccstr.at(3);
	}
	//~ std::cout << "Enter number of videos (max=99): ";
	//~ std::cin >> numvids;
	lTmp = tinyfd_inputBox(
		"Please Input", "Number of input videos (max=99)", "2");
	if (!lTmp) return 1 ;
	numvids = atoi(lTmp);
	if (numvids>99 || numvids<1)
	{
			tinyfd_messageBox(
			"Error",
			"Number of videos must be between 1 and 99. ",
			"ok",
			"error",
			1);
		return 1 ;
	}
		
	char const * FilterPatterns[2] =  { "*.avi","*.*" };
	char const * OpenFileName;
	
	while(looptemp<numvids)
	{
		 OpenFileName = tinyfd_openFileDialog(
			"Open a video file",
			"",
			2,
			FilterPatterns,
			NULL,
			0);

		if (! OpenFileName)
		{
			tinyfd_messageBox(
				"Error",
				"No file chosen. ",
				"ok",
				"error",
				1);
			return 1 ;
		}
		
		VidFileName[looptemp] = escaped(std::string(OpenFileName));
		//~ std::cout << "Enter desired position for this video as degrees longitude, back = 0: ";
		//~ std::cin >> vidlongi[looptemp];
		lTmp = tinyfd_inputBox(
		"Please Input", 
		"Desired position for this video as \n degrees longitude, back = 0", 
		"180");
		if (!lTmp) return 1 ;
		vidlongi[looptemp] = atoi(lTmp);
	
		//~ std::cout << "Enter desired height for this video as degrees latitude, horizon = 0: ";
		//~ std::cin >> vidlati[looptemp];
		lTmp = tinyfd_inputBox(
		"Please Input", 
		"Desired height for this video as \n degrees latitude, horizon = 0", 
		"20");
		if (!lTmp) return 1 ;
		vidlati[looptemp] = atoi(lTmp);
		
		//~ std::cout << "Enter desired width for this video in degrees: ";
		//~ std::cin >> vidw[looptemp];
		lTmp = tinyfd_inputBox(
		"Please Input", 
		"Desired width for this video in degrees", 
		"20");
		if (!lTmp) return 1 ;
		vidw[looptemp] = atoi(lTmp);
		
		looptemp++;
	}
	
	std::string OpenFileNamestr = VidFileName[0];
	std::string::size_type pAt = OpenFileNamestr.find_last_of('.');		// Find extension point
    NAME = OpenFileNamestr.substr(0, pAt) + "F" + ".avi";   // Form the new name with container
    
	char const * SaveFileName = tinyfd_saveFileDialog(
		"Now enter the output video file name, like output.mp4",
		"",
		0,
		NULL,
		NULL);

	if (! SaveFileName)
	{
		tinyfd_messageBox(
			"No output file chosen.",
			"Will be saved as inputfilename + F.avi",
			"ok",
			"info",
			1);
		 
	}
	else
	{
		escapedpath = escaped(std::string(SaveFileName));
		NAME = escapedpath;
	}
	
    // save parameters as an ini file
    pAt = NAME.find_last_of('.');                  // Find extension point
    std::string ininame = NAME.substr(0, pAt) + ".ini";   // Form the ini name 
    std::ofstream inioutfile(ininame, std::ofstream::out);
    inioutfile << "#ini_file_for_OCVvid2fulldome--Comments_start_with_#" << std::endl;
    inioutfile << "#Each_parameter_is_entered_in_the_line_below_the_comment." << std::endl;
    inioutfile << "#Output_file_path" << std::endl;
    inioutfile << NAME << std::endl;
    inioutfile << "#Outputw_pixels__=height" << std::endl;
    inioutfile << outputw << std::endl;
    inioutfile << "#Output_fps" << std::endl;
    inioutfile << outputfps << std::endl;
    inioutfile << "#Output_fourcc" << std::endl;
    inioutfile << outputfourcc << std::endl;
    inioutfile << "#Number_of_input_videos_max_is_99" << std::endl;
    inioutfile << numvids << std::endl;
	for (int i=0;i<numvids;i++)
	{
		inioutfile << "#Filename" << i << std::endl;
		inioutfile << VidFileName[i] << std::endl;
		inioutfile << "#vidlongi" << i << std::endl;
		inioutfile << vidlongi[i] << std::endl;
		inioutfile << "#vidlati" << i << std::endl;
		inioutfile << vidlati[i] << std::endl;
		inioutfile << "#vidw" << i << std::endl;
		inioutfile << vidw[i] << std::endl;
	}
		
	} // end if !skipinputs
	
	Sout = cv::Size(outputw,outputw);
	outputVideo.open(NAME, outputVideo.fourcc(outputfourcc[0], outputfourcc[1], outputfourcc[2], outputfourcc[3]), 
        outputfps, Sout, true);
    if (!outputVideo.isOpened())
    {
        std::cout  << "Could not open the output video for write: " << NAME << std::endl;
        return -1;
    }
	
	for (int i=0;i<numvids;i++)
	{
		//std::cout<<VidFileName[i]<<" " << vidlongi[i] << " " << vidw[i] << std::endl;
		escapedpath = VidFileName[i];
		inputVideo[i] = cv::VideoCapture(escapedpath);
		inputVideo[i] >> src;
		aspectratio[i] = (float)src.cols / (float)src.rows; // assuming square pixels
		inputVideo[i].set(cv::CAP_PROP_POS_FRAMES, 0);
		// reset the video to the first frame.
				
		map_x[i] = cv::Mat(Sout, CV_32FC1);
		map_y[i] = cv::Mat(Sout, CV_32FC1);
		map_x[i] = cv::Scalar((outputw+outputw)*10);
		map_y[i] = cv::Scalar((outputw+outputw)*10);
		// initializing so that it points outside the image
		// so that unavailable pixels will be black
		makesmall_map(vidw[i], aspectratio[i], map_x[i], map_y[i]);
		cv::convertMaps(map_x[i], map_y[i], dsts_x[i], dsts_y[i], CV_16SC2);	
		
		update_map(vidlongi[i], vidlati[i], vidw[i], aspectratio[i], map_x[i], map_y[i]);
		cv::convertMaps(map_x[i], map_y[i], dst_x[i], dst_y[i], CV_16SC2);	
		// supposed to make it faster to remap
		std::cout<<"\r"<<"Process vid"<<i<<" x:"<<vidlongi[i]<<" y:"<<vidlati[i]<<std::flush;;
	}
	
	t_start = time(NULL);
	fps = 0;
	
	for (int i=0;i<numvids;i++)
	{
		inputEnded[i]=0;
	}
	
	// https://stackoverflow.com/questions/5907031/printing-the-correct-number-of-decimal-points-with-cout
	std::cout << std::fixed << std::setprecision(1);
	
	std::cout << std::endl << "Output codec type: " << outputfourcc << std::endl;
	
	cv::namedWindow("Display", cv::WINDOW_NORMAL ); // 0 = WINDOW_NORMAL
	cv::resizeWindow("Display", 600, 600); // this doesn't work?
	cv::moveWindow("Display", 0, 0);
		
	while(1)
	{
		dst = cv::Mat::zeros(Sout, CV_8UC3);
		// loop through all frames of input vids
		for (int i=0;i<numvids;i++)
		{
			inputVideo[i] >> src;              // read
			if (src.empty()) // check if at end
			{
				inputEnded[i]=1;
				
			}
			else
			{
			//	inputEnded[i]=0;	
			//imshow("Display",src);
			cv::resize(src, res, Sout, 0, 0, cv::INTER_LINEAR);
			// first remap to small size
			cv::remap(res, dsts, dsts_x[i], dsts_y[i], cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0) );
			// remap using OCVWarp code
			cv::remap(dsts, dst2, dst_x[i], dst_y[i], cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0) );
			// and rotate to desired longitude
			// https://stackoverflow.com/questions/9041681/opencv-python-rotate-image-by-x-degrees-around-specific-point
			
			dst = dst + dst2;
			}
			
		}
		
		doneflag = 1;
		for (int i=0;i<numvids;i++)
		{
			doneflag = doneflag && inputEnded[i];
			// doneflag = 1 when all inputs are Ended
		}
		
		
		key = cv::waitKey(10);
		
		if(showdisplay)
			cv::imshow("Display", dst);
			
        std::cout << "\r";
        
        fps++;
        t_end = time(NULL);
		if (t_end - t_start >= 5)
		{
			std::cout << "Frame: " << framenum++ << "  fps: " << (float)fps/5 << "           \r";
			// extra spaces to delete previous line's characters if any
			fflush(stdout);
			t_start = time(NULL);
			fps = 0;
		}
		else
		{
			std::cout << "Frame: " << framenum++ <<  "\r";
			fflush(stdout);
		}
			
        
       //outputVideo.write(res); //save or
       outputVideo << dst;
       
       
       switch (key)
				{

				case 27: //ESC key
				case 'x':
				case 'X':
					doneflag = 1;
					break;

				case 'u':
				case '+':
				case '=':	// increase angley
					for (int i=0;i<numvids;i++)
					{
						vidlati[i]=vidlati[i] + 1;
					}
					interactivemode = 1;
					break;
					
				case 'm':
				case '-':
				case '_':	// decrease angley
					for (int i=0;i<numvids;i++)
					{
						vidlati[i]=vidlati[i] - 1;
					}
					interactivemode = 1;
					break;
					
				case 'k':
				case '}':
				case ']':	// increase anglex
					for (int i=0;i<numvids;i++)
					{
						vidlongi[i]=vidlongi[i] + 1;
					}
					interactivemode = 1;
					break;
					
				case 'h':
				case '{':
				case '[':	// decrease anglex
					for (int i=0;i<numvids;i++)
					{
						vidlongi[i]=vidlongi[i] - 1;
					}
					interactivemode = 1;
					break;
				
				case 'U':
					// increase angley
					for (int i=0;i<numvids;i++)
					{
						vidlati[i]=vidlati[i] + 10;
					}
					interactivemode = 1;
					break;
					
				case 'M':
					// decrease angley
					for (int i=0;i<numvids;i++)
					{
						vidlati[i]=vidlati[i] - 10;
					}
					interactivemode = 1;
					break;
					
				case 'K':
					// increase anglex
					for (int i=0;i<numvids;i++)
					{
						vidlongi[i]=vidlongi[i] + 10;
					}
					interactivemode = 1;
					break;
					
				case 'H':
					// decrease anglex
					for (int i=0;i<numvids;i++)
					{
						vidlongi[i]=vidlongi[i] - 10;
					}
					interactivemode = 1;
					break;	
					
				case 'D':
				case 'd':
					// toggle display
					if(showdisplay)
						showdisplay=0;
					else
						showdisplay=1;
					break;	
					
				default:
					break;
				
				
				}
				
		if (doneflag == 1)
		{
			break;
		}
		
		if(interactivemode == 1)
        {
			for (int i=0;i<numvids;i++)
			{
				map_x[i] = cv::Scalar((outputw+outputw)*10);
				map_y[i] = cv::Scalar((outputw+outputw)*10);
				update_map(vidlongi[i], vidlati[i], vidw[i], aspectratio[i], map_x[i], map_y[i]);
				cv::convertMaps(map_x[i], map_y[i], dst_x[i], dst_y[i], CV_16SC2);	
				// supposed to make it faster to remap
				std::cout<<"\r"<<"Process vid"<<i<<" x:"<<vidlongi[i]<<" y:"<<vidlati[i]<<std::flush;;
				
			}
			interactivemode = 0;
		}
        
			
	} // end of while(1)
	*/
	std::cout << std::endl << "Finished writing." << std::endl;
	return 0; 
}
