#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/video/background_segm.hpp>
#include "iostream"
#include <ctime>
#include <stdio.h>
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions

// #define DEBUG 0

//Cropped area size.
#define SQUARE_SIZE 200
#define CROPMIDPOINT 100

//Marker relations
#define BLUE_RED 400
#define GREEN_FRONT 700
//Red Marker Position
#define RED_X 0
#define RED_Y 0
//Green Marker Position
#define GREEN_X BLUE_RED
#define GREEN_Y 0
//Blue Marker Position
#define BLUE_X BLUE_RED*0.5
#define BLUE_Y GREEN_FRONT
//Purple Marker Position
#define PURPLE_X BLUE_RED*0.5
#define PURPLE_Y GREEN_FRONT
#define PURPLE_Z 40

#define OPEN_CLOSE 1 //1 uses and opening procedure, 0 uses a closing procedure.
#define YUV_RGB 0 //1 = YUV, 0 = RGB.

//Canny edge detection threshold, it is quite low due to the low 
//brightness of the images.
#define THRESH 60
#define AREA_THRESH 100

#define ELEMENT_TYPE 0
#define OPENING_SIZE 3
#define CLOSING_SIZE 3

//Used to access all of the images.
#define ONESCOLUMN 6
#define TENSCOLUMN 5
#define NUMIMAGES 16

//Middle point of images.
#define MIDDLEX 292
#define MIDDLEY 360

//Colours used in image analysis.
#define WHITE 255
#define MANICOLOUR 127

//Size of the images when displayed.
#define WINDOWX 500
#define WINDOWY 400

using namespace cv;
using namespace std;

Point brightLoc, mousePos;

char input;
int cnt = 0, i, brightness = 0, contourIndex = 0, intense, eleType;

double largestArea, area;

Mat openElement, closeElement;
Rect bRect;
Scalar intenseAvg, yuvColour, color[3], yuvAvg;

vector<vector<Point> > contours;
vector<Vec4i> hierarchy;
vector<Vec3f> circles;

Mat frame, threshImage, erodedImage, colourImages[3], yuvImage, yuvSplit[3], contoursImage, blueImage, redImage, brightImage, mogFrame;
Mat fgMaskMOG; //fg mask generated by MOG method
Mat fgMaskMOG2; //fg mask fg mask generated by MOG2 method
Ptr<BackgroundSubtractor> pMOG; //MOG Background subtractor
Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor
int keyboard;

int USB = open( "/dev/rfcomm0", O_RDWR| O_NOCTTY );

//Function Prototypes

void onMouse(int event, int x, int y, int flags, void* userdata);
void disImage(char* winName, Mat Image, int Position);
int write(char message[10]);
void processVideo(char* videoFilename);
void processImages(char* firstFrameFilename);

int main(int argc, char* argv[])
{
	//check for the input parameter correctness
	// if(argc != 3) {
	// 	cerr <<"Incorret input list" << endl;
	// 	cerr <<"exiting..." << endl;
	// 	return EXIT_FAILURE;
	// }

	if( ELEMENT_TYPE == 0 ){ eleType = MORPH_RECT; }
	else if( ELEMENT_TYPE == 1 ){ eleType = MORPH_CROSS; }
	else if( ELEMENT_TYPE == 2) { eleType = MORPH_ELLIPSE; }

	openElement = getStructuringElement(eleType , Size( 2*OPENING_SIZE + 1, 2*OPENING_SIZE+1 ), Point(OPENING_SIZE, OPENING_SIZE));
	closeElement = getStructuringElement(eleType , Size( 2*CLOSING_SIZE + 1, 2*CLOSING_SIZE+1 ), Point(CLOSING_SIZE, CLOSING_SIZE));

	color[0] = Scalar(255, 0, 0);
	color[1] = Scalar(0, 0, 255);

	pMOG= new BackgroundSubtractorMOG(); //MOG approach
	pMOG2 = new BackgroundSubtractorMOG2(); //MOG2 approach


	//input data coming from a video
	processVideo(argv[2]);

	//destroy GUI windows
	destroyAllWindows();
	return EXIT_SUCCESS;
}

void processVideo(char* videoFilename) {

	#ifdef DEBUG
	cout << "Elements Created\n\r";
	#endif

	waitKey(0);
	//create the capture object
	VideoCapture capture(videoFilename);
	if(!capture.isOpened()){
		//error in opening the video input
		cerr << "Unable to open video file: " << videoFilename << endl;
		exit(EXIT_FAILURE);
	}

	cnt = 0;

	time_t tstart, tend; 
	tstart = time(0);
	
	while(1){
		cnt++;
		//read the current frame
		if(!capture.read(frame)) {
			cerr << "Unable to read next frame." << endl;
			cerr << "Exiting..." << endl;
			cout << "Number of frames: " << cnt << endl;
			tend = time(0);
			cout << "It took "<< difftime(tend, tstart) <<" second(s)."<< endl;
			exit(EXIT_FAILURE);
		}

		#ifdef DEBUG 
		cout << "Frame taken\n\r";
		#endif
		//update the background model
			//AND HERE!!!
		pMOG->operator()(frame, fgMaskMOG);
		pMOG2->operator()(frame, fgMaskMOG2);
		//get the frame number and write it on the current frame
		stringstream ss;
		rectangle(frame, cv::Point(10, 2), cv::Point(100,20),
			cv::Scalar(255,255,255), -1);
		ss << capture.get(CV_CAP_PROP_POS_FRAMES);
		string frameNumberString = ss.str();
		putText(frame, frameNumberString.c_str(), cv::Point(15, 15),
			FONT_HERSHEY_SIMPLEX, 0.5 , cv::Scalar(0,0,0));
		//show the current frame and the fg masks
		dilate(fgMaskMOG, fgMaskMOG, closeElement);
		frame.copyTo(mogFrame, fgMaskMOG);
		// bitwise_and(frame, fgMaskMOG, mogFrame);

		#ifdef DEBUG 
		cout << "Background mask dilated\n\r";
		#endif
		
		//Processing on frame.
		cvtColor(mogFrame, mogFrame, CV_BGR2YCrCb);
		yuvAvg = mean(mogFrame);
		split(mogFrame, yuvSplit);

		#ifdef DEBUG 
		cout << "Image split\n\r";
		#endif

		threshold(yuvSplit[0], brightImage, yuvAvg[0]*1.5, 255, THRESH_TOZERO);
		threshold(yuvSplit[1], redImage, yuvAvg[1]*1.1, 255, THRESH_TOZERO);
		threshold(yuvSplit[2], blueImage, yuvAvg[2]*1.1, 255, THRESH_TOZERO);

		#ifdef DEBUG 
		cout << "Image split\n\r";
		#endif

		dilate(brightImage, brightImage, closeElement);

		//White lights should be reduced, as the markers are distinct colours.
		colourImages[0] = blueImage; // - redImage*0.3;
		colourImages[1] = redImage; // - blueImage*0.3;

		bitwise_and(blueImage, brightImage, colourImages[0]);
		bitwise_and(redImage, brightImage, colourImages[1]);

		disImage((char *)"Blue Image 1", colourImages[0], 2);
		disImage((char *)"Red Image 1", colourImages[1], 4);

		#ifdef DEBUG
		cout << "Brightness mask applied\n\r";
		#endif

		for(int k = 0; k <= 1; k++) {

			intenseAvg = mean(colourImages[k]);

			threshold(colourImages[k], threshImage, intenseAvg[0]*1.2, 255, THRESH_BINARY);

			erode(colourImages[k], erodedImage, openElement);
			dilate(erodedImage, threshImage, openElement);

			contoursImage = threshImage.clone();

			blur(contoursImage, contoursImage, Size(4,4));

			findContours(contoursImage, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0,0));

			#ifdef DEBUG 
			cout << "Contours found\n\r";
			#endif

			if (k == 0) {
				disImage((char *)"Blue Image 2", threshImage, 3);
			}
			else {
				disImage((char *)"Red Image 2", threshImage, 6);
			}

				largestArea = 0;
				area = 0;
				contourIndex = 0;
				for(int i = 0; i<contours.size(); i++) {
					area = contourArea(contours[i], false);
					if(area > AREA_THRESH) {
						// largestArea = area;
						// contourIndex = i;
						bRect = boundingRect(contours[i]);
						rectangle(frame, bRect, color[k]);
					}
				}
			}

		disImage((char *)"Frame", frame, 1);


		keyboard = waitKey(30);
	}
	//delete capture object

	capture.release();
}

void disImage(char* winName, Mat Image, int Position) {
	namedWindow(winName, WINDOW_NORMAL);
	imshow(winName, Image);
	resizeWindow(winName, WINDOWX, WINDOWY);

	switch (Position) {
		case 1:
			moveWindow(winName, 0, 0);
			break;
		case 2:
			moveWindow(winName, WINDOWX+5, 0);
			break;
		case 3:
			moveWindow(winName, (WINDOWX*2)+10, 0);
			break;
		case 4:
			moveWindow(winName, 0, WINDOWY+25);
			break;
		case 5:
			moveWindow(winName, WINDOWX+5, WINDOWY+25);
			break;
		case 6:
			moveWindow(winName, (WINDOWX*2)+10, WINDOWY+25);
			break;
	}
}