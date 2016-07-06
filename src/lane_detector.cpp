/*
 * lane_detector.cpp
 *
 *      Author:
 *         Nicolas Acero
 */

#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <lane_detector/preprocessor.h>
#include <lane_detector/featureExtractor.h>
#include <lane_detector/fitting.h>
#include <cv.h>
#include <sstream>
#include <dynamic_reconfigure/server.h>
#include <lane_detector/DetectorConfig.h>
#include <swri_profiler/profiler.h>
#include <geometry_msgs/PointStamped.h>
#include <geometry_msgs/PolygonStamped.h>
#include <lane_detector/LaneDetector.hh>
#include <lane_detector/CameraInfoOpt.h>
#include <lane_detector/LaneDetectorOpt.h>
#include <lane_detector/mcv.hh>
#include <lane_detector/utils.h>

cv_bridge::CvImagePtr currentFrame_ptr;
Preprocessor preproc;
FeatureExtractor extractor;
Fitting fitting_phase;
image_transport::Publisher resultImg_pub;
ros::Publisher detectedPoints_pub;
lane_detector::DetectorConfig dynConfig;
LaneDetector::CameraInfo cameraInfo;
LaneDetector::LaneDetectorConf lanesConf;


void configCallback(lane_detector::DetectorConfig& config, uint32_t level)
{
        preproc.setConfig(config);
        extractor.setConfig(config);
        fitting_phase.setConfig(config);
        dynConfig = config;
        ROS_DEBUG("Config was set");
}

void processImage(LaneDetector::CameraInfo& cameraInfo, LaneDetector::LaneDetectorConf& lanesConf) {

  if(currentFrame_ptr) {

    // detect lanes
    std::vector<LaneDetector::Line> lanes;
    cv::Mat originalImg = currentFrame_ptr->image;
    preproc.preprocess(originalImg);
    extractor.extract(originalImg, lanes);
    fitting_phase.fitting(originalImg, originalImg, lanes);
    cv::imshow("Out", originalImg);
    cv::waitKey(1);
       // print lanes
        /*for(int i=0; i<splines.size(); i++)
         {
           if (splines[i].color == LaneDetector::LINE_COLOR_YELLOW)
             mcvDrawSpline(raw_ptr, splines[i], CV_RGB(255,255,0), 3);
           else
             mcvDrawSpline(raw_ptr, splines[i], CV_RGB(0,255,0), 3);
         }

        std::cout << "frame#" << setw(8) << setfill('0') << 0 <<
          " has " << splines_world.size() << " splines" << endl;
        for (int i=0; i<splines_world.size(); ++i)
        {
          std::cout << "\tspline#" << i+1 << " has " <<
            splines_world[i].degree+1 << " points and score " <<
            splineScores[i] << endl;
          for (int j=0; j<=splines_world[i].degree; ++j)
            std::cout<< "\t\t" <<
              splines_world[i].points[j].x << ", " <<
              splines_world[i].points[j].y << endl;
          char str[256];
          sprintf(str, "%d", i);
          LaneDetector::mcvDrawText(raw_ptr, str,
                      cvPointFrom32f(splines[i].points[splines[i].degree]),
                                      1, CV_RGB(0, 0, 255));
        }*/
  }
}

void readImg(const sensor_msgs::ImageConstPtr& img)
{

        try
        {
                currentFrame_ptr = cv_bridge::toCvCopy(img, sensor_msgs::image_encodings::BGR8);
                processImage(cameraInfo, lanesConf);
                //if(dynConfig.image != 0) {
                  //      currentFrame_ptr->encoding = sensor_msgs::image_encodings::MONO8;
                //}
                resultImg_pub.publish(*currentFrame_ptr->toImageMsg());
                //currentFrame_ptr.reset();
        }
        catch (cv_bridge::Exception& e)
        {
                ROS_ERROR("cv_bridge exception: %s", e.what());
                return;
        }

}


int main(int argc, char **argv){

        //processImage(cameraInfo, lanesConf);
        mcvInitCameraInfo("/home/n/lane-detector/src/CameraInfo.conf", &cameraInfo);
        preproc.setCameraInfo(cameraInfo);
        ros::init(argc, argv, "lane_detector");

        /**
         * NodeHandle is the main access point to communications with the ROS system.
         * The first NodeHandle constructed will fully initialize this node, and the last
         * NodeHandle destructed will close down the node.
         */
        ros::NodeHandle nh;
        image_transport::ImageTransport it(nh);

        dynamic_reconfigure::Server<lane_detector::DetectorConfig> server;
        dynamic_reconfigure::Server<lane_detector::DetectorConfig>::CallbackType f;

        f = boost::bind(&configCallback, _1, _2);
        server.setCallback(f);

        /**
         * The advertise() function is how you tell ROS that you want to
         * publish on a given topic name. This invokes a call to the ROS
         * master node, which keeps a registry of who is publishing and who
         * is subscribing. After this advertise() call is made, the master
         * node will notify anyone who is trying to subscribe to this topic name,
         * and they will in turn negotiate a peer-to-peer connection with this
         * node.  advertise() returns a Publisher object which allows you to
         * publish messages on that topic through a call to publish().  Once
         * all copies of the returned Publisher object are destroyed, the topic
         * will be automatically unadvertised.
         *
         * The second parameter to advertise() is the size of the message queue
         * used for publishing messages.  If messages are published more quickly
         * than we can send them, the number here specifies how many messages to
         * buffer up before throwing some away.
         */

        image_transport::Subscriber image_sub = it.subscribe("/kinect_mono_throttled", 1, readImg);
        resultImg_pub = it.advertise("lane_detector/result", 1);
        detectedPoints_pub = nh.advertise<geometry_msgs::PolygonStamped>("lane_detector/vanishing_point", 1);

        //ros::MultiThreadedSpinner spinner(0); // Use one thread for core
        //spinner.spin(); // spin() will not return until the node has been shutdown
        ros::spin();
        return 0;
}
