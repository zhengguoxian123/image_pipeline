/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include <ros/ros.h>
#include <nodelet/loader.h>

/// @todo Would be nicer to make the nodelet names sub-namespaces of the
/// stereo_image_proc node, but see #4460.

void loadMonocularNodelets(nodelet::Loader& manager, const std::string& side,
                           const XmlRpc::XmlRpcValue& rectify_params)
{
  nodelet::M_string remappings;
  nodelet::V_string my_argv;
  
  // Debayer nodelet: image_raw -> image_mono, image_color
  remappings["image_raw"]   = side + "/image_raw";
  remappings["image_mono"]  = side + "/image_mono";
  remappings["image_color"] = side + "/image_color";
  std::string debayer_name = ros::this_node::getName() + "_debayer_" + side;
  manager.load(debayer_name, "image_proc/debayer", remappings, my_argv);

  // Rectify nodelet: image_mono -> image_rect
  remappings.clear();
  remappings["image_mono"]  = side + "/image_mono";
  remappings["camera_info"] = side + "/camera_info";
  remappings["image_rect"]  = side + "/image_rect";
  std::string rectify_mono_name = ros::this_node::getName() + "_rectify_mono_" + side;
  if (rectify_params.valid())
    ros::param::set(rectify_mono_name, rectify_params);
  manager.load(rectify_mono_name, "image_proc/rectify", remappings, my_argv);

  // Rectify nodelet: image_color -> image_rect_color
  remappings.clear();
  remappings["image_mono"]  = side + "/image_color";
  remappings["camera_info"] = side + "/camera_info";
  remappings["image_rect"]  = side + "/image_rect_color";
  std::string rectify_color_name = ros::this_node::getName() + "_rectify_color_" + side;
  if (rectify_params.valid())
    ros::param::set(rectify_color_name, rectify_params);
  manager.load(rectify_color_name, "image_proc/rectify", remappings, my_argv);
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "stereo_image_proc");

  // Check for common user errors
  if (ros::names::remap("camera") != "camera")
  {
    ROS_WARN("Remapping 'camera' has no effect! Start stereo_image_proc in the "
             "stereo namespace instead.\nExample command-line usage:\n"
             "\t$ ROS_NAMESPACE=%s rosrun stereo_image_proc stereo_image_proc",
             ros::names::remap("camera").c_str());
  }
  if (ros::this_node::getNamespace() == "/")
  {
    ROS_WARN("Started in the global namespace! This is probably wrong. Start "
             "stereo_image_proc in the stereo namespace.\nExample command-line usage:\n"
             "\t$ ROS_NAMESPACE=my_stereo rosrun stereo_image_proc stereo_image_proc");
  }

  // Shared parameters to be propagated to nodelet private namespaces
  ros::NodeHandle private_nh("~");
  XmlRpc::XmlRpcValue shared_params;
  int queue_size;
  if (private_nh.getParam("queue_size", queue_size))
    shared_params["queue_size"] = queue_size;

  nodelet::Loader manager(false); // Don't bring up the manager ROS API
  nodelet::M_string remappings;
  nodelet::V_string my_argv;

  // Load equivalents of image_proc for left and right cameras
  loadMonocularNodelets(manager, "left",  shared_params);
  loadMonocularNodelets(manager, "right", shared_params);

  // Stereo nodelets also need to know the synchronization policy
  bool approx_sync;
  if (private_nh.getParam("approximate_sync", approx_sync))
    shared_params["approximate_sync"] = XmlRpc::XmlRpcValue(approx_sync);

  // Disparity nodelet
  // Inputs: left/image_rect, left/camera_info, right/image_rect, right/camera_info
  // Outputs: disparity
  // NOTE: Using node name for the disparity nodelet because it is the only one using
  // dynamic_reconfigure so far, and this makes us backwards-compatible with cturtle.
  std::string disparity_name = ros::this_node::getName();
  manager.load(disparity_name, "stereo_image_proc/disparity", remappings, my_argv);

  // PointCloud2 nodelet
  // Inputs: left/image_rect_color, left/camera_info, right/camera_info, disparity
  // Outputs: points2
  std::string point_cloud2_name = ros::this_node::getName() + "_point_cloud2";
  if (shared_params.valid())
    ros::param::set(point_cloud2_name, shared_params);
  manager.load(point_cloud2_name, "stereo_image_proc/point_cloud2", remappings, my_argv);

  // PointCloud (deprecated) nodelet
  // Inputs: left/image_rect_color, left/camera_info, right/camera_info, disparity
  // Outputs: points
  std::string point_cloud_name = ros::this_node::getName() + "_point_cloud";
  if (shared_params.valid())
    ros::param::set(point_cloud_name, shared_params);
  manager.load(point_cloud_name, "stereo_image_proc/point_cloud", remappings, my_argv);

  /// @todo Would be nice to disable nodelet input checking and consolidate it here

  ros::spin();
  return 0;
}
