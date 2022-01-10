/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <ros/ros.h>

#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>

#include <image_transport/image_transport.h>

#include <jetson-utils/gstCamera.h>

#include "image_converter.h"



// globals	
gstCamera* camera = NULL;

imageConverter* camera_cvt = NULL;
image_transport::Publisher* camera_pub = NULL;


// aquire and publish camera frame
bool aquireFrame()
{
	float4* imgRGBA = NULL;

	// get the latest frame
	if( !camera->CaptureRGBA((float**)&imgRGBA, 1000) )
	{
		ROS_ERROR("failed to capture camera frame");
		return false;
	}

	// assure correct image size
	if( !camera_cvt->Resize(camera->GetWidth(), camera->GetHeight(), IMAGE_RGBA32F) )
	{
		ROS_ERROR("failed to resize camera image converter");
		return false;
	}

	// populate the message
	sensor_msgs::Image msg;

	if( !camera_cvt->Convert(msg, imageConverter::ROSOutputFormat, imgRGBA) )
	{
		ROS_ERROR("failed to convert camera frame to sensor_msgs::Image");
		return false;
	}

	// header
	static uint32_t seq = 0;
	static const char* frame_id = camera->GetResource().c_str();
	msg.header.seq = seq;
	msg.header.stamp = ros::Time::now();
	msg.header.frame_id = frame_id;
	seq++;

	// publish the message
	camera_pub->publish(msg);
	//ROS_INFO("published frame");
	ROS_DEBUG("published camera frame");
	return true;
}


// node main loop
int main(int argc, char **argv)
{
	// Accept --width --height --fps
	
	//printf("ARGC: %d\n", argc);
	//printf("%s\n", argv[1]);

	int width = 640, height = 480;
	float framerate = 30.0;

	for (int i=1; i<argc; i++ ){
		
		if (strcmp("--width",argv[i])==0 && i+1<argc) {
			width = atoi(argv[i+1]);
		}

		if (strcmp("--height",argv[i])==0 && i+1<argc){
                        height = atoi(argv[i+1]);
		}

		if (strcmp("--fps",argv[i])==0 && i+1<argc) {
                        framerate = atof(argv[i+1]);
		}
	}

	ros::init(argc, argv, "jetbot_camera");
 
	ros::NodeHandle nh;
	ros::NodeHandle private_nh("~");

	/*
	 * retrieve parameters
	 */
	std::string camera_device = "csi://0";	// MIPI CSI camera by default

	private_nh.param<std::string>("device", camera_device, camera_device);
	private_nh.param("width", width, width);
	private_nh.param("height", height, height);
	private_nh.param("framerate", framerate, framerate);

	ROS_INFO("opening camera device %s @ %dx%d %ffps", camera_device.c_str(), width, height, framerate);


	/*
	 * open camera device
	 */
	videoOptions opt;

	opt.resource = camera_device;
	opt.width = width;
	opt.height = height;
	opt.frameRate = framerate;
	opt.ioType = videoOptions::INPUT;
	opt.flipMethod = videoOptions::FLIP_ROTATE_180;

	camera = gstCamera::Create(opt);

	if( !camera )
	{
		ROS_ERROR("failed to open camera device %s", camera_device.c_str());
		return 0;
	}


	/*
	 * create image converter
	 */
	camera_cvt = new imageConverter();

	if( !camera_cvt )
	{
		ROS_ERROR("failed to create imageConverter");
		return 0;
	}


	/*
	 * advertise publisher topics using image transport to support JPEG compression
	 */
	image_transport::ImageTransport it(private_nh);
	image_transport::Publisher camera_publisher = it.advertise("raw", 2);
	camera_pub = &camera_publisher;


	/*
	 * start the camera streaming
	 */
	if( !camera->Open() )
	{
		ROS_ERROR("failed to start camera streaming");
		return 0;
	}


	/*
	 * start publishing video frames
	 */
	while( ros::ok() )
	{
		//if( raw_pub->getNumSubscribers() > 0 )
			aquireFrame();

		ros::spinOnce();
	}

	delete camera;
	return 0;
}

