#ifndef VEHICLE_H
#define VEHICLE_H

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <ros/ros.h>
#include <tf/tf.h>
#include <std_msgs/Empty.h>
#include <std_msgs/String.h>
#include <std_msgs/Bool.h>
#include <geographic_msgs/GeoPoint.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Vector3.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf2/LinearMath/Quaternion.h>
#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/BatteryState.h>
#include <mavros_msgs/State.h>
#include <mavros_msgs/SetMode.h>
#include <mavros_msgs/CommandTOL.h>
#include <mavros_msgs/CommandBool.h>
#include <mavros_msgs/CommandHome.h>
#include <mavros_msgs/HomePosition.h>
#include <mavros_msgs/GlobalPositionTarget.h>
#include <uv_base/srvGoToVehicle.h>
#include <uv_base/srvMultiSetpointLocal.h>
#include <uv_base/srvMultiSetpointGlobal.h>

#define CONSTANTS_RADIUS_OF_EARTH 6371000 /* meters (m)		*/
#define M_DEG_TO_RAD (M_PI / 180.0)
#define M_RAD_TO_DEG (180.0 / M_PI)
typedef struct vehicle_info
{
	int vehicle_id_;
	std::string vehicle_name_;
} VehicleInfo;

/* 	Drone modes : 
	MANUAL, STABILIZED,	OFFBOARD, ACRO, ALTCTL,	POSCTL,	RATTITUDE, 
	AUTO.TAKEOFF, AUTO.LAND, AUTO.MISSION, AUTO.LOITER,	AUTO.RTL,
	AUTO.RTGS, AUTO.READY,*/

class Vehicle
{
  private:
	VehicleInfo vehicle_info_;

	ros::NodeHandle nh_;
	ros::NodeHandle &nh_mul_;
	ros::NodeHandle &nh_global_;

	ros::NodeHandle nh_trigger_;

	/*drone state*/
	mavros_msgs::State cur_state_;
	sensor_msgs::BatteryState cur_battery_;

	/* ros subscriber*/
	ros::Subscriber state_sub_;
	ros::Subscriber battery_sub_;
	ros::Subscriber home_sub_;
	ros::Subscriber local_pos_sub_;
	ros::Subscriber global_pos_sub_;
	ros::Subscriber obstacle_pos_sub_;
	ros::Subscriber turn_yaw;

	/* ros publisher*/
	ros::Publisher setpoint_vel_pub_;
	ros::Publisher setpoint_local_pub_;
	ros::Publisher setpoint_global_pub_;

	/* ros service client*/
	ros::ServiceClient arming_client_;
	ros::ServiceClient set_mode_client_;
	ros::ServiceClient set_home_client_;
	ros::ServiceClient takeoff_client_;
	ros::ServiceClient land_client_;

	/* ros multi sub client */
	ros::Subscriber multi_arming_sub_;
	ros::Subscriber multi_set_mode_sub_;
	ros::Subscriber multi_set_home_sub_;
	ros::Subscriber multi_takeoff_sub_;
	ros::Subscriber multi_land_sub_;

	/* local coordinate*/
	geometry_msgs::PoseStamped home_local_;
	geometry_msgs::PoseStamped cur_local_;
	geometry_msgs::PoseStamped tar_local_;

	/* global coordinate*/
	sensor_msgs::NavSatFix home_global_;
	sensor_msgs::NavSatFix cur_global_;
	sensor_msgs::NavSatFix tar_global_;

	tf2::Vector3 pos_;
	tf2::Vector3 sum_sp_;
	tf2::Vector3 err_;
	tf2::Vector3 setpoint_pos_;
	std::pair<int, int> scen_pos_;

	bool setpoint_publish_flag_;
	/* yaw direction when arming */
	double arming_roll_, arming_pitch_, arming_yaw_;
	double turned_yaw_;
	double roll_, pitch_, yaw_;

	/*fermware version=> diagnositic_msgs/DiagnosticStatus*/

	void vehicleInit();
	void release();
	void stateCB(const mavros_msgs::State::ConstPtr &);
	void batteryCB(const sensor_msgs::BatteryState::ConstPtr &);
	void homeCB(const mavros_msgs::HomePosition::ConstPtr &);
	void globalPositionCB(const sensor_msgs::NavSatFix::ConstPtr &);
	void localPositionCB(const geometry_msgs::PoseStamped::ConstPtr &);
	void turnCB(const std_msgs::Bool::ConstPtr &);
	/* multi callback functions */
	void multiArming(const std_msgs::Bool::ConstPtr &);
	void multiSetMode(const std_msgs::String::ConstPtr &);
	void multiSetHome(const std_msgs::Empty::ConstPtr &);
	void multiTakeoff(const std_msgs::Empty::ConstPtr &);
	void multiLand(const std_msgs::Empty::ConstPtr &);

  public:
	Vehicle() = delete;
	Vehicle(ros::NodeHandle &, ros::NodeHandle &);
	Vehicle(ros::NodeHandle &, ros::NodeHandle &, const VehicleInfo &);
	Vehicle(const Vehicle &);
	const Vehicle &operator=(const Vehicle &);
	~Vehicle();

	void setVehicleInfo(const VehicleInfo &);
	VehicleInfo getInfo() const;
	mavros_msgs::State getState() const;
	sensor_msgs::BatteryState getBattery() const;

	/*main drone function*/
	bool arming(const bool &);
	bool setMode(const std::string &);
	bool takeoff(const double &);
	bool land();
	void gotoGlobal(const sensor_msgs::NavSatFix &);
	void setLocalTarget(const geometry_msgs::PoseStamped &);
	void gotoLocal();

	/* setpoint control method */
	void setPos(const tf2::Vector3 &);
	tf2::Vector3 getPos() const;
	void setSumOfSp(const tf2::Vector3 &);
	tf2::Vector3 getSumOfSp() const;
	void setErr(const tf2::Vector3 &);
	tf2::Vector3 getErr() const;
	void setSetpointPos(const tf2::Vector3 &);
	tf2::Vector3 getSetpointPos() const;
	void setScenPos(const std::pair<int, int> &);
	std::pair<int,int> getScenPos() const;

	//global position
	bool setHomeGlobal();
	sensor_msgs::NavSatFix getHomeGlobal() const;
	sensor_msgs::NavSatFix getGlobalPosition() const;
	sensor_msgs::NavSatFix getTargetGlobal() const;

	//local position
	void setHomeLocal();
	geometry_msgs::PoseStamped getHomeLocal() const;
	geometry_msgs::PoseStamped getLocalPosition() const;
	geometry_msgs::PoseStamped getTargetLocal() const;

	double getArmingYaw(){return arming_yaw_;}

	bool isPublish() const;
};

class SwarmVehicle
{
  private:
	/* swarm_info */
	std::string swarm_name_;
	int num_of_vehicle_;

	std::vector<Vehicle> camila_;
	std::vector<Vehicle>::iterator iter_;
	std::vector<tf2::Vector3> offset_;
	std::vector<uint8_t> scen_hex_;

	ros::NodeHandle nh_;
	ros::NodeHandle nh_mul_;
	ros::NodeHandle &nh_global_;

	ros::Subscriber trigger_sub_;
	ros::Publisher trigger_arm_;
	ros::Publisher trigger_mode_;

	ros::ServiceServer multi_setpoint_local_server_;
	ros::ServiceServer multi_setpoint_global_server_;
	ros::ServiceServer goto_vehicle_server_;

	tf2::Vector3 swarm_target_local_;

	std::string formation_;
	bool multi_setpoint_publish_flag_;
	bool target_changed_flag_;
	double angle_;
	ros::Time prev_;

	static double kp_seek_;
	static double kp_sp_;
	static double range_sp_;
	static double vector_speed_limit_;
	static int scen_num_;
	static std::string scen_str_;

	void swarmServiceInit();
	void release();
	void updateOffset();

	void limit(tf2::Vector3 &, const double &);
	void getVehiclePos();
	void separate(Vehicle &);
	void seek(Vehicle &);
	void formationGenerator();
	void scenario2();
	void scenario3();
	void scenario4();
	void scenario5();
	void scenario6();
	void hexToCoord(std::vector<std::pair<int,int>> &, const uint8_t &, const int &, const bool &);

	bool multiSetpointLocal(uv_base::srvMultiSetpointLocal::Request &req,
							uv_base::srvMultiSetpointLocal::Response &res);
	bool multiSetpointGlobal(uv_base::srvMultiSetpointGlobal::Request &req,
							 uv_base::srvMultiSetpointGlobal::Response &res);
	bool gotoVehicle(uv_base::srvGoToVehicle::Request &req,
					 uv_base::srvGoToVehicle::Response &res);

	static tf2::Vector3 convertGeoToENU(const sensor_msgs::NavSatFix &,
										const sensor_msgs::NavSatFix &);
	static geographic_msgs::GeoPoint convertENUToGeo(const geometry_msgs::PoseStamped &,
													 const sensor_msgs::NavSatFix &);
	bool isPublish();
	
	void triggerCB(const std_msgs::Empty::ConstPtr &);

  public:
	SwarmVehicle(ros::NodeHandle &, const std::string &swarm_name = "camila", const int &num_of_vehicle = 1);
	SwarmVehicle(const SwarmVehicle &);
	const SwarmVehicle &operator=(const SwarmVehicle &);
	~SwarmVehicle();

	void setSwarmInfo(const std::string &, const int &);
	std::string getSwarmInfo() const;

	void addVehicle(const VehicleInfo &);
	void deleteVehicle(const VehicleInfo &);
	void showVehicleList() const;

	void run();
};

#endif