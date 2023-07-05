/*
 * OCVvid2fulldome.cpp
 * 
 * Warps "flat" video files to fit 180 degree fisheye "fulldome masters" using the OpenCV framework. 
 * Appends 'F.avi' to the first filename and saves as default codec (XVID avi) in the same folder.
 * 
 * first commit:
 * Hari Nandakumar
 * 10 May 2020
 * 
 * 
 */

//#define _WIN64
//#define __unix__

// references 
// http://paulbourke.net/geometry/transformationprojection/
// equations in figure at http://paulbourke.net/dome/dualfish2sphere/
// http://paulbourke.net/dome/dualfish2sphere/diagram.pdf

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
#include "tinyfiledialogs.h"

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


int main(int argc,char *argv[])
{
	bool doneflag = 0;
	bool showdisplay = 1, interactivemode=0;
	bool skipinputs = 0;
	int outputw;
	double outputfps;
	char outputfourcc[5] = {'X','V','I','D', '\0'};
	std::string fourccstr;
	int numvids;
	std::string VidFileName[100];
	std::string NAME;
	int vidlongi[100];
	int vidlati[100];
	int vidw[100];
	float aspectratio[100];
	int looptemp=0;
	cv::VideoCapture inputVideo[100];
	bool inputEnded[100];
	std::string escapedpath;
	cv::VideoWriter outputVideo;
	int  fps, key;
	int t_start, t_end;
    unsigned long long framenum = 0;
    std::string tempstring, inistr;
    char const * lTmp;
    char * ptr;
     
    cv::Mat src, dst, res;
    cv::Mat dstfloat, dstmult, dstres, dstflip;
    cv::Size Sout;
    
    std::vector<cv::Mat> spl;
    cv::Mat dst2, dst3, dsts;	// temp dst, for eachvid
    
    cv::Mat map_x[100], map_y[100];
    cv::Mat dst_x[100], dst_y[100];
    cv::Mat dsts_x[100], dsts_y[100];
    
    if(argc <= 1)
    {
		char const * FilterPatternsini[2] =  { "*.ini","*.*" };
		char const * OpenFileNameini;
		
		OpenFileNameini = tinyfd_openFileDialog(
				"Open an ini file if it exists",
				"",
				2,
				FilterPatternsini,
				NULL,
				0);

		if (! OpenFileNameini)
		{
			skipinputs = 0;
		}
		else
		{
			skipinputs = 1;
			inistr = OpenFileNameini;
		}
	}
	    
    if(argc > 1)
    {
		// argument can be ini file path
		skipinputs = 1;
		inistr = argv[1];
	}
	
	if(skipinputs)
    {
		std::ifstream infile(inistr);
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
	
	std::cout << std::endl << "Finished writing." << std::endl;
	return 0;
}
