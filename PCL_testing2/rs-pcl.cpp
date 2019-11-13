// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/rsutil.h>
#include "../../../examples/example.hpp" // Include short list of convenience functions for rendering
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
#include <iostream>
#include <pcl/io/pcd_io.h>
#include <pcl/common/common.h>
#include <pcl/common/time.h>
// 3rd party header for writing png files
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <single-file/stb_image_write.h>

// Struct for managing rotation of pointcloud view
struct state {
	state() : yaw(0.0), pitch(0.0), last_x(0.0), last_y(0.0),
		ml(false), offset_x(0.0f), offset_y(0.0f) {}
	double yaw, pitch, last_x, last_y; bool ml; float offset_x, offset_y;
};

using pcl_ptr = pcl::PointCloud<pcl::PointXYZ>::Ptr;
pcl::PointXYZ minZ;

// Helper functions
void register_glfw_callbacks(window& app, state& app_state);
void draw_pointcloud(window& app, state& app_state, const std::vector<pcl_ptr>& points);

pcl_ptr points_to_pcl(const rs2::points& points)
{
	pcl_ptr cloud(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::StopWatch watch;
	auto sp = points.get_profile().as<rs2::video_stream_profile>();
	cloud->width = sp.width();
	cloud->height = sp.height();
	cloud->is_dense = true;
	cloud->points.resize(points.size());
	auto ptr = points.get_vertices();
	for (auto& p : cloud->points)
	{
		p.x = ptr->x;
		p.y = ptr->y;
		p.z = ptr->z;
		ptr++;
	}
	pcl::console::print_highlight("Time taken to convert: %f\n", watch.getTimeSeconds());
	pcl_ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PassThrough<pcl::PointXYZ> pass;
	pass.setInputCloud(cloud);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.6, 0.72);
	pass.filter(*cloud_filtered);
	pcl::console::print_highlight("Time taken to filter: %f\n", watch.getTimeSeconds());

	/*std::string filename = "test_pcd.pcd";
	pcl::io::savePCDFileASCII(filename, *cloud_filtered);*/
	
	//printf("123");
	/*size_t num_points1 = cloud->size();
	size_t num_points2 = cloud_filtered->size();
	std::cout << "size of cloud: " << num_points1 << std::endl;
	std::cout << "size of cloud_filtered: " << num_points2 << std::endl;

	pcl::PointXYZ minPt, maxPt;
	pcl::getMinMax3D(*cloud_filtered, minPt, maxPt);
	std::cout << "Max x: " << maxPt.x << std::endl;
	std::cout << "Max y: " << maxPt.y << std::endl;
	std::cout << "Max z: " << maxPt.z << std::endl;
	std::cout << "Min x: " << minPt.x << std::endl;
	std::cout << "Min y: " << minPt.y << std::endl;
	std::cout << "Min z: " << minPt.z << std::endl;
	pcl::console::print_highlight("Time taken to std minmax: %f\n", watch.getTimeSeconds());*/
	
	minZ.x = 0;
	minZ.y = 0;
	minZ.z = 1.0;
	for (size_t i = 1; i < cloud_filtered->points.size(); ++i) {
		if (cloud_filtered->points[i].z <= minZ.z)
		{
			//std::cout << "i: " << i << " , points.z: " << cloud_filtered->points[i].z << " , minz.z: " << minZ.z << std::endl;
			minZ.x = cloud_filtered->points[i].x;
			minZ.y = cloud_filtered->points[i].y;
			minZ.z = cloud_filtered->points[i].z;
		}
	}
	pcl::console::print_highlight("Time taken to alt min: %f\n", watch.getTimeSeconds());
	std::cout << "Alt Min x: " << minZ.x << std::endl;
	std::cout << "Alt Min y: " << minZ.y << std::endl;
	std::cout << "Alt Min z: " << minZ.z << std::endl;

	//return cloud;
	return cloud_filtered;
}

float3 colors[]{ { 0.8f, 0.1f, 0.3f },
{ 0.1f, 0.9f, 0.5f },
};

int main(int argc, char * argv[]) try
{
	// Create a simple OpenGL window for rendering:
	window app(1280, 720, "RealSense PCL Pointcloud Example");
	// Construct an object to manage view state
	state app_state;
	// register callbacks to allow manipulation of the pointcloud
	register_glfw_callbacks(app, app_state);

	// Declare pointcloud object, for calculating pointclouds and texture mappings
	rs2::pointcloud pc;
	// We want the points object to be persistent so we can display the last cloud when a frame drops
	rs2::points points;

	rs2::config cfg;
	cfg.enable_stream(RS2_STREAM_COLOR, 1280, 720);
	cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720);

	// Declare RealSense pipeline, encapsulating the actual device and sensors
	rs2::pipeline pipe;
	// Start streaming with default recommended configuration
	pipe.start(cfg);

	// Wait for the next set of frames from the camera
	auto frames = pipe.wait_for_frames();

	auto depth = frames.get_depth_frame();

	std::cout << "Depth width: " << depth.get_width() << std::endl;
	std::cout << "Depth height: " << depth.get_height() << std::endl;

	// Generate the pointcloud and texture mappings
	points = pc.calculate(depth);

	// Get RGB frame
	auto color = frames.get_color_frame();

	std::cout << "Color width: " << color.get_width() << std::endl;
	std::cout << "Color height: " << color.get_height() << std::endl;

	// Tell pointcloud object to map to this color frame
	pc.map_to(color);

	const rs2_intrinsics i = pipe.get_active_profile().get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>().get_intrinsics();

	auto pcl_points = points_to_pcl(points);

	float minZpixel[2];
	float minZpoint[3] = { minZ.x, minZ.y, minZ.z };
	rs2_project_point_to_pixel(minZpixel, &i, minZpoint);
	std::cout << "Image x: " << minZpixel[0] << std::endl;
	std::cout << "Image y: " << minZpixel[1] << std::endl;

	// Write images to disk
	std::stringstream png_file;
	png_file << "c:/Bin/rs-save-to-disk-output-" << "test.png";
	stbi_write_png("c:/Bin/test.png", color.get_width(), color.get_height(),
		color.get_bytes_per_pixel(), color.get_data(), color.get_stride_in_bytes());
	std::cout << "Saved " << png_file.str() << std::endl;

	pcl_ptr cloud_filtered(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PassThrough<pcl::PointXYZ> pass;
	pass.setInputCloud(pcl_points);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.0, 1.0);
	pass.filter(*cloud_filtered);

	std::vector<pcl_ptr> layers;
	layers.push_back(pcl_points);
	layers.push_back(cloud_filtered);

	while (app) // Application still alive?
	{
		draw_pointcloud(app, app_state, layers);
	}

	return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
	std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}

// Registers the state variable and callbacks to allow mouse control of the pointcloud
void register_glfw_callbacks(window& app, state& app_state)
{
	app.on_left_mouse = [&](bool pressed)
	{
		app_state.ml = pressed;
	};

	app.on_mouse_scroll = [&](double xoffset, double yoffset)
	{
		app_state.offset_x += static_cast<float>(xoffset);
		app_state.offset_y += static_cast<float>(yoffset);
	};

	app.on_mouse_move = [&](double x, double y)
	{
		if (app_state.ml)
		{
			app_state.yaw -= (x - app_state.last_x);
			app_state.yaw = std::max(app_state.yaw, -120.0);
			app_state.yaw = std::min(app_state.yaw, +120.0);
			app_state.pitch += (y - app_state.last_y);
			app_state.pitch = std::max(app_state.pitch, -80.0);
			app_state.pitch = std::min(app_state.pitch, +80.0);
		}
		app_state.last_x = x;
		app_state.last_y = y;
	};

	app.on_key_release = [&](int key)
	{
		if (key == 32) // Escape
		{
			app_state.yaw = app_state.pitch = 0; app_state.offset_x = app_state.offset_y = 0.0;
		}
	};
}

// Handles all the OpenGL calls needed to display the point cloud
void draw_pointcloud(window& app, state& app_state, const std::vector<pcl_ptr>& points)
{
	// OpenGL commands that prep screen for the pointcloud
	glPopMatrix();
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	float width = app.width(), height = app.height();

	glClearColor(153.f / 255, 153.f / 255, 153.f / 255, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	gluPerspective(60, width / height, 0.01f, 10.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	gluLookAt(0, 0, 0, 0, 0, 1, 0, -1, 0);

	glTranslatef(0, 0, +0.5f + app_state.offset_y*0.05f);
	glRotated(app_state.pitch, 1, 0, 0);
	glRotated(app_state.yaw, 0, 1, 0);
	glTranslatef(0, 0, -0.5f);

	glPointSize(width / 640);
	glEnable(GL_TEXTURE_2D);

	int color = 0;

	for (auto&& pc : points)
	{
		auto c = colors[(color++) % (sizeof(colors) / sizeof(float3))];
		glPointSize(1);
		glBegin(GL_POINTS);
		glColor3f(c.x, c.y, c.z);

		/* this segment actually prints the pointcloud */
		for (int i = 0; i < pc->points.size(); i++)
		{
			auto&& p = pc->points[i];
			if (p.z)
			{
				// upload the point and texture coordinates only for points we have depth data for
				glVertex3f(p.x, p.y, p.z);
			}
		}

		glEnd();

		// Added to display minZ
		// *********BEGIN************
		glPointSize(6);  // Make it bigger so we can see it
		glBegin(GL_POINTS);
		glColor3f(0.8, 0.1, 0.3);  // Red color

								   // upload the point and texture coordinates only for points we have depth data for
		glVertex3f(minZ.x, minZ.y, minZ.z);

		glEnd();
		// **********END*************
	}

	// OpenGL cleanup
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
	glPushMatrix();
}