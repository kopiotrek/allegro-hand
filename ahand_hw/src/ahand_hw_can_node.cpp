// SYS

#include <sys/mman.h>
#include <signal.h>
#include <stdexcept>

// ROS

#include <ros/ros.h>
#include <controller_manager/controller_manager.h>

#include "ahand_hw/ahand_hw_can.hpp"

bool g_quit = false;

void quitRequested(int sig){
  g_quit = true;
}


// Get the URDF XML from the parameter server
// Get the URDF XML from the parameter server
std::string getURDF(ros::NodeHandle &model_nh_, std::string param_name)
{
  std::string urdf_string;
  std::string robot_description = "/robot_description";

  // search and wait for robot_description on param server
  while (urdf_string.empty())
  {
    std::string search_param_name;
    if (model_nh_.searchParam(param_name, search_param_name))
    {
      ROS_INFO_ONCE_NAMED("LWRHWFRIL", "LWRHWFRIL node is waiting for model"
        " URDF in parameter [%s] on the ROS param server.", search_param_name.c_str());

      model_nh_.getParam(search_param_name, urdf_string);
    }
    else
    {
      ROS_INFO_ONCE_NAMED("LWRHWFRIL", "LWRHWFRIL node is waiting for model"
        " URDF in parameter [%s] on the ROS param server.", robot_description.c_str());

      model_nh_.getParam(param_name, urdf_string);
    }

    usleep(100000);
  }
  ROS_DEBUG_STREAM_NAMED("LWRHWFRIL", "Recieved urdf from param server, parsing...");

  return urdf_string;
}

int main(int argc, char** argv){

    // initialize ROS
    ros::init(argc, argv, "ahand_hw_interface", ros::init_options::NoSigintHandler);

    // ros spinner
    ros::AsyncSpinner spinner(1);
    spinner.start();

    // custom signal handlers
    signal(SIGTERM, quitRequested);
    signal(SIGINT, quitRequested);
    signal(SIGHUP, quitRequested);

    // create a node
    ros::NodeHandle ahand_nh("/ahand");

    // get params or give default values
    std::string file;
    std::string name;
    //#lwr_nh.param("file", file, std::string(""));
    //lwr_nh.param("name", name, std::string("ahand"));

    // get the general robot description, the lwr class will take care of parsing what's useful to itself
    std::string urdf_string = getURDF(ahand_nh, "/robot_description");

    // construct and start the real ahand
    AhandHWCAN ahand_robot;
    ahand_robot.create("ahand", urdf_string);

    if(!ahand_robot.init()){
       ROS_FATAL_NAMED("ahand_hw","Could not initialize robot real interface");
       return -1;
    }

    // timer variables
    struct timespec ts = {0, 0};
    ros::Time last(ts.tv_sec, ts.tv_nsec), now(ts.tv_sec, ts.tv_nsec);
    ros::Duration period(1.0);

    //the controller manager
    controller_manager::ControllerManager manager(&ahand_robot, ahand_nh);


    // run as fast as possible
    while( !g_quit ){
      // get the time / period
      if (!clock_gettime(CLOCK_REALTIME, &ts)){
        now.sec = ts.tv_sec;
        now.nsec = ts.tv_nsec;
        period = now - last;
        last = now;
      }else{
        ROS_FATAL("Failed to poll realtime clock!");
        break;
      }

      // read the state from the lwr
      ahand_robot.read(now, period);

      // update the controllers
      manager.update(now, period);

      // write the command to the lwr
      ahand_robot.write(now, period);
    }

    std::cerr<<"Stopping spinner..."<<std::endl;
    spinner.stop();

    std::cerr<<"Stopping ahand_hw..."<<std::endl;
    //ahand_robot.stop();

    std::cerr<<"This node was killed!"<<std::endl;



    return 0;
}
