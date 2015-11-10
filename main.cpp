/*********************************************************************************************************
Copyright (c) 2013 EAVISE, KU Leuven, Campus De Nayer
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:
- A referral to the origal author of this software (Steven Puttemans)
- A notice send at the original author (steven.puttemans[at]kuleuven.be)
The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************************************************/

/**********************************************************************************
* Software for accessing AVT cameras of Prosilica and Manta series
* using the builtin PvAPI interface of OpenCV
**********************************************************************************/

// --------------------------------------------------------------------------------
// Some remarks for ensuring the correct working of the interface between camera
// and the pc from which you will capture data
//
// You have to be sure that OpenCV is built with the PvAPI interface enabled.
//
// FIRST CONFIGURE IP SETTINGS
// - Change the IP address of your pc to 169.254.1.1
// - Change the subnet mask of your pc to 255.255.0.0
// - Change the gateway of your pc to 169.254.1.2
//
// CHANGE SOME NETWORK CARD SETTINGS
// - sudo ifconfig [eth0 - eth1] mtu 9000 - or 9016 ideally if your card supports that
//
// LINK THE OPENCV DEPENDENT LIBRARIES USING THE ADDITIONAL LINKER OPTION
//	`pkg-config opencv --libs`
// --------------------------------------------------------------------------------
/*
  File: main.cpp
  Written by: M. Taha Koroglu (I just modified the code provided by Steven Puttemans)
  Comments: This code successfully opens two cameras and displays them on screen.
  But frame rate is low, i.e., reaching 13Hz approximately at maximum.
  Both cameras are in "Freerun" "FrameStartTriggerMode"
*/

#include <iostream>
#include "Windows.h" /* data type DWORD and function GetTickCount() are defined in Windows.h */
#include "opencv2/opencv.hpp"
#include <cassert>
#include <ppl.h> // Parallel patterns library, concurrency namespace. Young-Jin uses this, so do I. It's very important; if not used, when images are processed, fps drops dramatically.
#if !defined VK_ESCAPE /* VK stands for virtual key*/
#define VK_ESCAPE 0x1B /* ASCII code for ESC character is 27 */
#endif

using namespace std;
using namespace cv;

const unsigned long numberOfCameras = 2;
bool displayImages = true;

int main()
{
	Mat frame[numberOfCameras], imgResized[numberOfCameras];
	double f = 0.4; /* f is a scalar in [0-1] range that scales the raw image. Output image is displayed in the screen. */
	DWORD timeStart, timeEnd; // these variables are used for computing fps and avg_fps.
	double fps = 1.0; // frame per second
	double sum_fps(0.);
	double avg_fps(0.); // average fps
	int frameCount = 0;
	VideoCapture camera[numberOfCameras]; // (0 + CV_CAP_PVAPI); /* open the default camera; VideoCapture is class, camera is object. */
	for (int i = 0; i < numberOfCameras; i++)
	{
		camera[i].open(i + CV_CAP_PVAPI);
		if (!camera[i].isOpened())
		{
			cerr << "Cannot open camera " << i << "." << endl;
			return EXIT_FAILURE;
		}
	}
	double rows = camera[0].get(CV_CAP_PROP_FRAME_HEIGHT); /* Height of the frames in the video stream. */
	double cols = camera[0].get(CV_CAP_PROP_FRAME_WIDTH); /* Width of the frames in the video stream. */
	if (numberOfCameras == 2)
		assert(rows == camera[1].get(CV_CAP_PROP_FRAME_HEIGHT) && cols == camera[0].get(CV_CAP_PROP_FRAME_WIDTH));
	for (int i = 0; i<numberOfCameras; i++) // initializing monochrome images.
	{
		frame[i] = Mat(Size(cols, rows), CV_8UC1); /* Mat(Size size, int type) */
		resize(frame[i], imgResized[i], Size(0, 0), f, f, INTER_LINEAR);
	}
	/* combo is a combined image consisting of left and right resized images. images are resized in order to be displayed at a smaller region on the screen. */
	Mat combo(Size(imgResized[0].size().width * 2, imgResized[0].size().height), imgResized[0].type()); /* This is the merged image (i.e., side by side) for real-time display. */
	Rect roi[numberOfCameras]; /* roi stands for region of interest. */
	for (int i = 0; i < numberOfCameras; i++)
		roi[i] = Rect(0, 0, imgResized[0].cols, imgResized[0].rows);
	/* Setting locations of images coming from different cameras in the combo image. */
	if (numberOfCameras > 1) /* I assume max camera number is 2. */
	{
		roi[1].x = imgResized[0].cols;
		roi[1].y = 0;
	}
	
	double exposure, exposureTimeInSecond = 0.06; /* As exposureTimeInSecond variable decreases, fps should increase */
	for (int i = 0; i < numberOfCameras; i++)
	{
		exposure = camera[i].get(CV_CAP_PROP_EXPOSURE);
		cout << "Exposure value of the camera " << i << " at the beginning is " << exposure << endl;
		exposure = exposureTimeInSecond * 1000000; /* esposure time in us */
		camera[i].set(CV_CAP_PROP_EXPOSURE, exposure);
	} 
	double frameRate[numberOfCameras]; /* built-in fps */
	cout << "Frame size of the camera is " << cols << "x" << rows << "." << endl;
	cout << "Exposure value of both cameras is set to " << exposure << endl;
	char* winname = "real-time image acquisition";
	if (displayImages)
		namedWindow(winname, WINDOW_AUTOSIZE);
	cout << "Press ESC to terminate real-time acquisition." << endl;

	while (true)
	{
		timeStart = GetTickCount();
		Concurrency::parallel_for((unsigned long)0, numberOfCameras, [&](unsigned long i)
		{
			camera[i] >> frame[i];
			frameRate[i] = camera[i].get(CV_CAP_PROP_FPS); /* Built-in frame rate in Hz. */
			resize(frame[i], imgResized[i], Size(), f, f, INTER_LINEAR); /* void cv::resize(InputArray src, OutputArray dst, Size dsize, double fx = 0, double fy = 0, int interpolation = INTER_LINEAR) */
			imgResized[i].copyTo(combo(roi[i])); /* This is C++ API. */
		});
		frameCount++;
		if (displayImages)
		{
			imshow(winname, combo);
			moveWindow(winname, 780, 50);
		}
		int key = waitKey(10);
		if (key == VK_ESCAPE)
		{
			destroyWindow(winname);
			break;
		}
		/* Calculating FPS in my own way. */
		timeEnd = GetTickCount();
		fps = 1000.0 / (double)(timeEnd - timeStart); /* 1s = 1000ms */
		sum_fps += fps;
		avg_fps = sum_fps / frameCount;
		for (int i = 0; i < numberOfCameras; i++)
			cout << "FPScam" << i << "=" << frameRate[i] << " ";
		cout << "frame#" << frameCount << " my_fps=" << fps << " avg_fps=" << avg_fps << endl;
	}

	cout << "Compiled with OpenCV version " << CV_VERSION << endl; /* thanks to Shervin Emami */
	system("pause");
	return EXIT_SUCCESS;
}


// double triggerMode = camera.get(CV_CAP_PROP_PVAPI_FRAMESTARTTRIGGERMODE);
////camera.set(CV_CAP_PROP_PVAPI_FRAMESTARTTRIGGERMODE, 4.0);
//
//if (triggerMode == 0.)
//cout << "Trigger mode is Freerun" << endl;
//else if (triggerMode == 1.0)
//cout << "Trigger mode is SyncIn1" << endl;
//else if (triggerMode == 2.0)
//cout << "Trigger mode is SyncIn2" << endl;
//else if (triggerMode == 3.0)
//cout << "Trigger mode is FixedRate" << endl;
//else if (triggerMode == 4.0)
//cout << "Trigger mode is Software" << endl;
//else
//cout << "There is no trigger mode!!!";