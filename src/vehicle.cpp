#include <ros/ros.h>
#include <vehicle.h>

Vehicle::Vehicle(ros::NodeHandle &nh_mul, ros::NodeHandle &nh_global)
	: vehicle_info_({1, "mavros"}),
	  nh_(ros::NodeHandle(vehicle_info_.vehicle_name_)),
	  nh_mul_(nh_mul),
	  nh_global_(nh_global),
	  pos_(tf2::Vector3(0, 0, 0)),
	  sum_sp_(tf2::Vector3(0, 0, 0)),
	  err_(tf2::Vector3(0, 0, 0)),
	  setpoint_pos_(tf2::Vector3(0, 0, 0)),
	  scen_pos_(std::pair<int, int>(0, 0)),
	  setpoint_publish_flag_(false),
	  turned_yaw_(0)
{
	vehicleInit();
}

Vehicle::Vehicle(ros::NodeHandle &nh_mul, ros::NodeHandle &nh_global, const VehicleInfo &vehicle_info)
	: vehicle_info_(vehicle_info),
	  nh_(ros::NodeHandle(vehicle_info_.vehicle_name_)),
	  nh_mul_(nh_mul),
	  nh_global_(nh_global),
	  pos_(tf2::Vector3(0, 0, 0)),
	  sum_sp_(tf2::Vector3(0, 0, 0)),
	  err_(tf2::Vector3(0, 0, 0)),
	  setpoint_pos_(tf2::Vector3(0, 0, 0)),
	  scen_pos_(std::pair<int, int>(0, 0)),
	  setpoint_publish_flag_(false),
	  turned_yaw_(0)
{
	vehicleInit();
}

Vehicle::Vehicle(const Vehicle &rhs)
	: vehicle_info_(rhs.vehicle_info_),
	  nh_(ros::NodeHandle(vehicle_info_.vehicle_name_)),
	  nh_mul_(rhs.nh_mul_),
	  nh_global_(rhs.nh_global_),
	  pos_(rhs.pos_),
	  sum_sp_(rhs.sum_sp_),
	  err_(rhs.err_),
	  setpoint_pos_(rhs.setpoint_pos_),
	  scen_pos_(std::pair<int, int>(0, 0)),
	  setpoint_publish_flag_(rhs.setpoint_publish_flag_),
	  turned_yaw_(rhs.turned_yaw_)
{
	vehicleInit();
	*this = rhs;
}

const Vehicle &Vehicle::operator=(const Vehicle &rhs)
{
	if (this == &rhs)
		return *this;

	vehicle_info_ = rhs.vehicle_info_;
	setpoint_publish_flag_ = rhs.setpoint_publish_flag_;

	nh_ = ros::NodeHandle(vehicle_info_.vehicle_name_);
	nh_mul_ = rhs.nh_mul_;
	nh_global_ = rhs.nh_global_;
	pos_ = rhs.pos_;
	sum_sp_ = rhs.sum_sp_;
	err_ = rhs.err_;
	setpoint_pos_ = rhs.setpoint_pos_;
	scen_pos_ = rhs.scen_pos_;

	vehicleInit();
	return *this;
}

Vehicle::~Vehicle()
{
	release();
}

void Vehicle::vehicleInit()
{
	setpoint_global_pub_ = nh_.advertise<mavros_msgs::GlobalPositionTarget>("mavros/setpoint_position/global", 10);
	setpoint_local_pub_ = nh_.advertise<geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 10);
	setpoint_vel_pub_ = nh_.advertise<geometry_msgs::Twist>("mavros/setpoint_velocity/cmd_vel_unstamped", 10);

	state_sub_ = nh_.subscribe("mavros/state", 10, &Vehicle::stateCB, this);
	battery_sub_ = nh_.subscribe("mavros/battery", 10, &Vehicle::batteryCB, this);
	home_sub_ = nh_.subscribe("mavros/home_position/home", 10, &Vehicle::homeCB, this);
	local_pos_sub_ = nh_.subscribe("mavros/local_position/pose", 10, &Vehicle::localPositionCB, this);
	global_pos_sub_ = nh_.subscribe("mavros/global_position/global", 10, &Vehicle::globalPositionCB, this);
	turn_yaw = nh_.subscribe("/turn",10, &Vehicle::turnCB,this);

	arming_client_ = nh_.serviceClient<mavros_msgs::CommandBool>("mavros/cmd/arming");
	set_mode_client_ = nh_.serviceClient<mavros_msgs::SetMode>("mavros/set_mode");
	set_home_client_ = nh_.serviceClient<mavros_msgs::CommandHome>("mavros/cmd/set_home");
	takeoff_client_ = nh_.serviceClient<mavros_msgs::CommandTOL>("mavros/cmd/takeoff");
	land_client_ = nh_.serviceClient<mavros_msgs::CommandTOL>("mavros/cmd/land");
	multi_arming_sub_ = nh_mul_.subscribe("arming", 10, &Vehicle::multiArming, this);
	multi_set_mode_sub_ = nh_mul_.subscribe("set_mode", 10, &Vehicle::multiSetMode, this);
	multi_set_home_sub_ = nh_mul_.subscribe("set_home", 10, &Vehicle::multiSetHome, this);
	multi_takeoff_sub_ = nh_mul_.subscribe("takeoff", 10, &Vehicle::multiTakeoff, this);
	multi_land_sub_ = nh_mul_.subscribe("land", 10, &Vehicle::multiLand, this);

	ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " instance generated");
}

void Vehicle::release()
{
	setpoint_global_pub_.shutdown();
	setpoint_local_pub_.shutdown();
	setpoint_vel_pub_.shutdown();

	state_sub_.shutdown();
	battery_sub_.shutdown();
	home_sub_.shutdown();
	local_pos_sub_.shutdown();
	global_pos_sub_.shutdown();

	arming_client_.shutdown();
	set_mode_client_.shutdown();
	set_home_client_.shutdown();
	takeoff_client_.shutdown();
	land_client_.shutdown();

	multi_arming_sub_.shutdown();
	multi_set_mode_sub_.shutdown();
	multi_set_home_sub_.shutdown();
	multi_takeoff_sub_.shutdown();
	multi_land_sub_.shutdown();
}

void Vehicle::stateCB(const mavros_msgs::State::ConstPtr &msg)
{
	if (cur_state_.connected != msg->connected)
	{
		if (msg->connected == true)
			ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " is connected");
		else
			ROS_WARN_STREAM(vehicle_info_.vehicle_name_ << " is not connected");
	}
	if (cur_state_.armed != msg->armed)
	{
		if (msg->armed == true)
			ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " is armed");
		else
			ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " is disarmed");
	}
	if (cur_state_.mode != msg->mode)
	{
		ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " is " << msg->mode << " mode");
	}
	cur_state_ = *msg;
}

void Vehicle::batteryCB(const sensor_msgs::BatteryState::ConstPtr &msg)
{
	cur_battery_ = *msg;
}

void Vehicle::homeCB(const mavros_msgs::HomePosition::ConstPtr &msg)
{
	home_global_.latitude = msg->geo.latitude;
	home_global_.longitude = msg->geo.longitude;
	home_global_.altitude = msg->geo.altitude;
	home_local_.pose.position.x = msg->position.x;
	home_local_.pose.position.y = msg->position.y;
	home_local_.pose.position.z = msg->position.z;
}

void Vehicle::globalPositionCB(const sensor_msgs::NavSatFix::ConstPtr &msg)
{
	cur_global_ = *msg;
}

void Vehicle::localPositionCB(const geometry_msgs::PoseStamped::ConstPtr &msg)
{
	cur_local_ = *msg;
}

void Vehicle::turnCB(const std_msgs::Bool::ConstPtr &msg)
{
	//this->turn_flag_ = msg->data;
	turned_yaw_ -= M_PI_2l;
}

void Vehicle::multiArming(const std_msgs::Bool::ConstPtr &msg)
{
	if (arming(msg->data))
		;
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call multi arming service. ");
}

void Vehicle::multiSetMode(const std_msgs::String::ConstPtr &msg)
{
	if (setMode(msg->data))
		;
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call multi set_mode service. ");
}

void Vehicle::multiSetHome(const std_msgs::Empty::ConstPtr &trigger)
{
	if (setHomeGlobal())
		setHomeLocal();
}

void Vehicle::multiTakeoff(const std_msgs::Empty::ConstPtr &trigger)
{
	int takeoff_alt;
	nh_global_.getParam("takeoff_alt", takeoff_alt);
	takeoff(takeoff_alt);
}

void Vehicle::multiLand(const std_msgs::Empty::ConstPtr &trigger)
{
	land();
}

void Vehicle::setVehicleInfo(const VehicleInfo &new_vehicle_info)
{
	vehicle_info_.vehicle_id_ = new_vehicle_info.vehicle_id_;
	vehicle_info_.vehicle_name_ = new_vehicle_info.vehicle_name_;

	release();

	nh_ = ros::NodeHandle(vehicle_info_.vehicle_name_);

	vehicleInit();
}

VehicleInfo Vehicle::getInfo() const
{
	return vehicle_info_;
}

mavros_msgs::State Vehicle::getState() const
{
	return cur_state_;
}

sensor_msgs::BatteryState Vehicle::getBattery() const
{
	return cur_battery_;
}

bool Vehicle::arming(const bool &arm_state)
{
	mavros_msgs::CommandBool msg;
	msg.request.value = arm_state;
	tf::Quaternion q(
		cur_local_.pose.orientation.x,
		cur_local_.pose.orientation.y,
		cur_local_.pose.orientation.z,
		cur_local_.pose.orientation.w
	);
	tf::Matrix3x3 m(q);
	m.getRPY( arming_roll_, arming_pitch_, arming_yaw_ );

	std::cout << "set arming yaw: "<<arming_yaw_<<std::endl;

	if (arming_client_.call(msg) && msg.response.success)
		ROS_INFO_STREAM(msg.response.result);
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call arming service. " << msg.response.result);
	return msg.response.success;
}

bool Vehicle::setMode(const std::string &current_mode)
{
	mavros_msgs::SetMode mode;
	mode.request.custom_mode = current_mode;
	if (((current_mode == "offboard") || (current_mode == "OFFBOARD")) && (!setpoint_publish_flag_))
	{
		ROS_WARN("Please publish setpoint first");
		return false;
	}
	else
	{
		if (set_mode_client_.call(mode) && mode.response.mode_sent)
			;
		else
			ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call set_mode service. " << mode.response.mode_sent);
		return mode.response.mode_sent;
	}
}

bool Vehicle::takeoff(const double &takeoff_alt)
{
	mavros_msgs::CommandTOL msg;
	msg.request.min_pitch = 0;
	msg.request.yaw = 0;
	msg.request.latitude = cur_global_.latitude;
	msg.request.longitude = cur_global_.longitude;
	msg.request.altitude = home_global_.altitude + takeoff_alt;

	if (takeoff_client_.call(msg) && msg.response.success)
		ROS_INFO_STREAM(msg.response.result);
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call takeoff service. " << msg.response.result);
	return msg.response.success;
}

bool Vehicle::land()
{
	mavros_msgs::CommandTOL msg;
	msg.request.min_pitch = 0;
	msg.request.yaw = 0;
	msg.request.latitude = cur_global_.latitude;
	msg.request.longitude = cur_global_.longitude;
	msg.request.altitude = 0;

	if (land_client_.call(msg) && msg.response.success)
		ROS_INFO_STREAM(msg.response.result);
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " failed to call land service. " << msg.response.result);
	return msg.response.success;
}

void Vehicle::gotoGlobal(const sensor_msgs::NavSatFix &tar_global)
{
	setpoint_publish_flag_ = true;

	if ((tar_global_.latitude != tar_global.latitude) ||
		(tar_global_.longitude != tar_global.longitude) ||
		(tar_global_.altitude != tar_global.altitude))
		ROS_INFO("%s set target_global_pos(long : %lf, lati : %lf, alti : %lf)", vehicle_info_.vehicle_name_.c_str(),
				 tar_global_.longitude, tar_global_.latitude, tar_global_.altitude);

	tar_global_ = tar_global;
	tar_global_.header.seq += 1;
	tar_global_.header.stamp = ros::Time::now();
	tar_global_.header.frame_id = vehicle_info_.vehicle_name_;

	mavros_msgs::GlobalPositionTarget msg;
	msg.latitude = tar_global_.latitude;
	msg.longitude = tar_global_.longitude;
	msg.altitude = tar_global_.altitude;
	msg.header.seq += 1;
	msg.header.stamp = ros::Time::now();
	msg.header.frame_id = vehicle_info_.vehicle_name_;
	msg.coordinate_frame = mavros_msgs::GlobalPositionTarget::FRAME_GLOBAL_INT;
	msg.type_mask = mavros_msgs::GlobalPositionTarget::IGNORE_AFX |
					mavros_msgs::GlobalPositionTarget::IGNORE_AFY |
					mavros_msgs::GlobalPositionTarget::IGNORE_AFZ |
					mavros_msgs::GlobalPositionTarget::IGNORE_VX |
					mavros_msgs::GlobalPositionTarget::IGNORE_VY |
					mavros_msgs::GlobalPositionTarget::IGNORE_VZ |
					mavros_msgs::GlobalPositionTarget::IGNORE_YAW |
					mavros_msgs::GlobalPositionTarget::IGNORE_YAW_RATE;

	setpoint_global_pub_.publish(msg);
}

void Vehicle::setLocalTarget(const geometry_msgs::PoseStamped &tar_local)
{
	setpoint_publish_flag_ = true;

	if ((tar_local_.pose.position.x != tar_local.pose.position.x) ||
		(tar_local_.pose.position.y != tar_local.pose.position.y) ||
		(tar_local_.pose.position.z != tar_local.pose.position.z))
		tar_local_ = tar_local;
}

void Vehicle::gotoLocal()
{
	tar_local_.header.seq += 1;
	tar_local_.header.stamp = ros::Time::now();
	tar_local_.header.frame_id = vehicle_info_.vehicle_name_;
	//현재 로컬포지션값의 쿼터니언을 받아와서 RPY로 변환
	tf::Quaternion cur_q(
		cur_local_.pose.orientation.x,
		cur_local_.pose.orientation.y,
		cur_local_.pose.orientation.z,
		cur_local_.pose.orientation.w
	);
	tf::Matrix3x3 cur_m(cur_q);
	cur_m.getRPY( roll_, pitch_, yaw_ );
	
	//yaw값만 arming걸때 값으로 돌려주면 됨.
	tf::Matrix3x3 tar_m;
	tar_m.setEulerYPR( arming_yaw_ + turned_yaw_, pitch_, roll_ );
	//  std::cout << "arming yaw: "<<arming_yaw_*180/M_PI<<std::endl;
	//  std::cout << "cur yaw: "<<yaw_*180/M_PI<<std::endl;
	tf::Quaternion tar_q;
	tar_m.getRotation(tar_q);
	tar_local_.pose.orientation.w = tar_q.getW();
	tar_local_.pose.orientation.x = tar_q.getX();
	tar_local_.pose.orientation.y = tar_q.getY();
	tar_local_.pose.orientation.z = tar_q.getZ();

	setpoint_local_pub_.publish(tar_local_);
}

void Vehicle::setPos(const tf2::Vector3 &pos)
{
	pos_ = pos;
}

tf2::Vector3 Vehicle::getPos() const
{
	return pos_;
}

void Vehicle::setSumOfSp(const tf2::Vector3 &sum_sp)
{
	sum_sp_ = sum_sp;
}

tf2::Vector3 Vehicle::getSumOfSp() const
{
	return sum_sp_;
}

void Vehicle::setErr(const tf2::Vector3 &err)
{
	err_ = err;
}

tf2::Vector3 Vehicle::getErr() const
{
	return err_;
}

void Vehicle::setSetpointPos(const tf2::Vector3 &setpoint)
{
	setpoint_pos_.setX(cur_local_.pose.position.x + setpoint.getX());
	setpoint_pos_.setY(cur_local_.pose.position.y + setpoint.getY());
	setpoint_pos_.setZ(cur_local_.pose.position.z + setpoint.getZ());
}

tf2::Vector3 Vehicle::getSetpointPos() const
{
	return setpoint_pos_;
}

void Vehicle::setScenPos(const std::pair<int, int> &scen_pos)
{
	scen_pos_ = scen_pos;
}

std::pair<int, int> Vehicle::getScenPos() const
{
	return scen_pos_;
}

bool Vehicle::setHomeGlobal()
{
	mavros_msgs::CommandHome msg;

	msg.request.current_gps = false;
	msg.request.latitude = cur_global_.latitude;
	msg.request.longitude = cur_global_.longitude;
	msg.request.altitude = cur_global_.altitude;

	if (set_home_client_.call(msg) && msg.response.success)
	{
		home_global_ = cur_global_;

		ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " Global home position is set. " << msg.response.result);
		ROS_INFO("%s new global home is (long : %lf, lang : %lf, alti : %lf)", vehicle_info_.vehicle_name_.c_str(),
				 home_global_.longitude, home_global_.latitude, home_global_.altitude);
	}
	else
		ROS_ERROR_STREAM(vehicle_info_.vehicle_name_ << " Failed to set global home position. " << msg.response.result);

	return msg.response.success;
}

sensor_msgs::NavSatFix Vehicle::getHomeGlobal() const
{
	return home_global_;
}

sensor_msgs::NavSatFix Vehicle::getGlobalPosition() const
{
	return cur_global_;
}

sensor_msgs::NavSatFix Vehicle::getTargetGlobal() const
{
	return tar_global_;
}

void Vehicle::setHomeLocal()
{
	home_local_ = cur_local_;

	ROS_INFO_STREAM(vehicle_info_.vehicle_name_ << " Local home position is set.");
	ROS_INFO("%s new local home is (x : %lf, y : %lf, z : %lf)", vehicle_info_.vehicle_name_.c_str(),
			 home_local_.pose.position.x, home_local_.pose.position.y, home_local_.pose.position.z);
}

geometry_msgs::PoseStamped Vehicle::getHomeLocal() const
{
	return home_local_;
}

geometry_msgs::PoseStamped Vehicle::getLocalPosition() const
{
	return cur_local_;
}

geometry_msgs::PoseStamped Vehicle::getTargetLocal() const
{
	return tar_local_;
}

bool Vehicle::isPublish() const
{
	return setpoint_publish_flag_;
}

double SwarmVehicle::kp_seek_;
double SwarmVehicle::kp_sp_;
double SwarmVehicle::range_sp_;
double SwarmVehicle::vector_speed_limit_;
int SwarmVehicle::scen_num_;
std::string SwarmVehicle::scen_str_ = "";

//default value : default name = camila, num_of_vehicle = 1;
SwarmVehicle::SwarmVehicle(ros::NodeHandle &nh_global, const std::string &swarm_name, const int &num_of_vehicle)
	: swarm_name_(swarm_name),
	  num_of_vehicle_(num_of_vehicle),
	  nh_(ros::NodeHandle()),
	  nh_mul_("multi"),
	  nh_global_(nh_global),
	  swarm_target_local_(tf2::Vector3(0, 0, 0)),
	  multi_setpoint_publish_flag_(false),
	  target_changed_flag_(false)
{
	VehicleInfo vehicle_info_[num_of_vehicle_];
	camila_.reserve(num_of_vehicle_);
	offset_.reserve(num_of_vehicle_);
	scen_hex_.reserve(num_of_vehicle_);

	for (int i = 0; i < num_of_vehicle_; i++)
	{
		vehicle_info_[i].vehicle_id_ = i + 1;
		vehicle_info_[i].vehicle_name_ = swarm_name_ + std::to_string(i + 1);
		camila_.push_back(Vehicle(nh_mul_, nh_global_, vehicle_info_[i]));
	}

	swarmServiceInit();
}

SwarmVehicle::SwarmVehicle(const SwarmVehicle &rhs)
	: swarm_name_(rhs.swarm_name_),
	  num_of_vehicle_(rhs.num_of_vehicle_),
	  nh_(rhs.nh_),
	  nh_mul_(rhs.nh_mul_),
	  nh_global_(rhs.nh_global_),
	  swarm_target_local_(tf2::Vector3(0, 0, 0)),
	  multi_setpoint_publish_flag_(rhs.multi_setpoint_publish_flag_),
	  target_changed_flag_(rhs.target_changed_flag_)
{
	VehicleInfo vehicle_info_[num_of_vehicle_];
	camila_.reserve(num_of_vehicle_);
	offset_.reserve(num_of_vehicle_);
	scen_hex_.reserve(num_of_vehicle_);

	std::vector<Vehicle>::const_iterator it;
	for (it = rhs.camila_.begin(); it != rhs.camila_.end(); it++)
		camila_.push_back(*it);

	nh_ = rhs.nh_;
	nh_mul_ = rhs.nh_mul_;
	nh_global_ = rhs.nh_global_;

	swarmServiceInit();

	*this = rhs;
}

const SwarmVehicle &SwarmVehicle::operator=(const SwarmVehicle &rhs)
{
	if (this == &rhs)
		return *this;

	swarm_name_ = rhs.swarm_name_;
	num_of_vehicle_ = rhs.num_of_vehicle_;

	std::vector<Vehicle>().swap(camila_);
	camila_.reserve(num_of_vehicle_);
	offset_.reserve(num_of_vehicle_);
	scen_hex_.reserve(num_of_vehicle_);

	std::vector<Vehicle>::const_iterator it;
	for (it = rhs.camila_.begin(); it != rhs.camila_.end(); it++)
		camila_.push_back(*it);

	nh_ = rhs.nh_;
	nh_mul_ = rhs.nh_mul_;
	nh_global_ = rhs.nh_global_;

	swarmServiceInit();

	return *this;
}

void SwarmVehicle::swarmServiceInit()
{
	multi_setpoint_local_server_ = nh_.advertiseService("multi_setpoint_local", &SwarmVehicle::multiSetpointLocal, this);
	multi_setpoint_global_server_ = nh_.advertiseService("multi_setpoint_global", &SwarmVehicle::multiSetpointGlobal, this);
	goto_vehicle_server_ = nh_.advertiseService("goto_vehicle", &SwarmVehicle::gotoVehicle, this);
	trigger_sub_ = nh_.subscribe("/trigger", 10, &SwarmVehicle::triggerCB, this);

}

void SwarmVehicle::release()
{
	std::vector<Vehicle>().swap(camila_);
	std::vector<tf2::Vector3>().swap(offset_);
	std::vector<uint8_t>().swap(scen_hex_);
	multi_setpoint_global_server_.shutdown();
	multi_setpoint_local_server_.shutdown();
	goto_vehicle_server_.shutdown();
}

SwarmVehicle::~SwarmVehicle()
{
	release();
}

void SwarmVehicle::updateOffset()
{
	sensor_msgs::NavSatFix leader = camila_.front().getHomeGlobal();

	int i = 0;
	if (offset_.size() == 0)
	{
		if (num_of_vehicle_ != 1)
			angle_ = 2.0 * M_PI / (num_of_vehicle_ - 1);
		for (auto &vehicle : camila_)
		{
			sensor_msgs::NavSatFix follower = vehicle.getGlobalPosition();
			tf2::Vector3 _offset = convertGeoToENU(leader, follower);
			offset_.push_back(_offset);
			ROS_INFO_STREAM("offset_[" << i << "] = " << offset_[i].getX() << ", " << offset_[i].getY() << ", " << offset_[i].getZ());
			i++;
		}
	}
}

void SwarmVehicle::limit(tf2::Vector3 &v, const double &limit)
{
	if (v.length() > limit)
		v = v.normalize() * limit;
}

void SwarmVehicle::getVehiclePos()
{
	sensor_msgs::NavSatFix origin = camila_.front().getHomeGlobal();
	for (auto &vehicle : camila_)
	{
		tf2::Vector3 vehicle_pos = convertGeoToENU(vehicle.getGlobalPosition(), origin);
		vehicle.setPos(vehicle_pos);
	}
}

void SwarmVehicle::separate(Vehicle &vehicle)
{
	tf2::Vector3 sum(0, 0, 0);
	unsigned int cnt = 0;

	for (auto &another_vehicle : camila_)
	{
		if (&vehicle != &another_vehicle)
		{
			tf2::Vector3 diff = vehicle.getPos() - another_vehicle.getPos();
			double dist = diff.length();
			if (dist < range_sp_)
			{
				if (diff.length() != 0)
					diff = diff.normalize();
				diff *= (range_sp_ / dist);
				sum += diff;
				cnt++;
			}
		}
	}
	if (cnt > 0)
	{
		sum /= cnt;
		limit(sum, vector_speed_limit_ + 1);
		vehicle.setSumOfSp(sum);
	}
	else
	{
		sum.setZero();
		vehicle.setSumOfSp(sum);
	}
}

void SwarmVehicle::seek(Vehicle &vehicle)
{
	tf2::Vector3 err(0, 0, 0);
	geometry_msgs::PoseStamped target_pos = vehicle.getTargetLocal();
	geometry_msgs::PoseStamped current_pos = vehicle.getLocalPosition();
	err.setX(target_pos.pose.position.x - current_pos.pose.position.x);
	err.setY(target_pos.pose.position.y - current_pos.pose.position.y);
	err.setZ(target_pos.pose.position.z - current_pos.pose.position.z);

	limit(err, vector_speed_limit_);
	vehicle.setErr(err);
}

void SwarmVehicle::formationGenerator()
{
	geometry_msgs::PoseStamped msg, msg_f;
	double m_sec = ros::Time::now().toNSec() / 1000000;
	double x = 0.00006 * m_sec; // 0.2 rad/s
	double angle, spacing;
	nh_global_.getParamCached("spacing", spacing);

	int i = 0;
	for (auto &vehicle : camila_)
	{
		msg.pose.position.x = swarm_target_local_.getX()*cos(vehicle.getArmingYaw()) - swarm_target_local_.getY()*sin(vehicle.getArmingYaw());
		msg.pose.position.y = swarm_target_local_.getX()*sin(vehicle.getArmingYaw()) + swarm_target_local_.getY()*cos(vehicle.getArmingYaw());
		msg.pose.position.z = swarm_target_local_.getZ();
		i++;
	}

	// msg.pose.position.x = swarm_target_local_.getX()*sin();
	// msg.pose.position.y = swarm_target_local_.getY();
	// msg.pose.position.z = swarm_target_local_.getZ();

	if (formation_ == "POINT")
	{
		int i = 0;
		for (auto &vehicle : camila_)
		{
			if (vehicle.getInfo().vehicle_id_ != 1)
			{
				msg_f.pose.position.x = msg.pose.position.x + offset_[i].getX();
				msg_f.pose.position.y = msg.pose.position.y + offset_[i].getY();
				msg_f.pose.position.z = msg.pose.position.z;
				vehicle.setLocalTarget(msg_f);
			}
			else
				vehicle.setLocalTarget(msg);
			i++;
		}
	}
	else if (formation_ == "IDLE")
	{
		int i = 0;
		int x = 0 , y = -3;
		for (auto &vehicle : camila_)
		{
			if(y < 16)
				y += 3;
			else{
				x += 3;
				y = 0;
			}
			msg_f.pose.position.x = msg.pose.position.x + offset_[i].getX() + x;
			msg_f.pose.position.y = msg.pose.position.y + offset_[i].getY() + y;
			msg_f.pose.position.z = msg.pose.position.z;
			vehicle.setLocalTarget(msg_f);
			i++;
		}
	}
}


bool SwarmVehicle::multiSetpointLocal(drone_base::srvMultiSetpointLocal::Request &req,
									  drone_base::srvMultiSetpointLocal::Response &res)
{
	updateOffset();

	formation_ = req.formation;
	swarm_target_local_.setX(req.x);
	swarm_target_local_.setY(req.y);
	swarm_target_local_.setZ(req.z);

	target_changed_flag_ = true;

	res.success = true;
	return res.success;
}

bool SwarmVehicle::multiSetpointGlobal(drone_base::srvMultiSetpointGlobal::Request &req,
									   drone_base::srvMultiSetpointGlobal::Response &res)
{
	res.success = true;
	return res.success;
}

bool SwarmVehicle::gotoVehicle(drone_base::srvGoToVehicle::Request &req,
							   drone_base::srvGoToVehicle::Response &res)
{
	geometry_msgs::PoseStamped msg;
	iter_ = camila_.begin() + req.num_drone - 1;

	updateOffset();

	if (req.num_drone > 0 && req.num_drone <= num_of_vehicle_)
	{
		msg.header.stamp = ros::Time::now();
		msg.pose.position.x = req.x + offset_[req.num_drone - 1].getX();
		msg.pose.position.y = req.y + offset_[req.num_drone - 1].getY();
		msg.pose.position.z = req.z;
	}

	iter_->setLocalTarget(msg);

	return true;
}

tf2::Vector3 SwarmVehicle::convertGeoToENU(const sensor_msgs::NavSatFix &coord,
										   const sensor_msgs::NavSatFix &home)
{
	static const float epsilon = std::numeric_limits<double>::epsilon();

	double lat_rad = coord.latitude * M_DEG_TO_RAD;
	double lon_rad = coord.longitude * M_DEG_TO_RAD;

	double ref_lon_rad = home.longitude * M_DEG_TO_RAD;
	double ref_lat_rad = home.latitude * M_DEG_TO_RAD;

	double sin_lat = sin(lat_rad);
	double cos_lat = cos(lat_rad);
	double cos_d_lon = cos(lon_rad - ref_lon_rad);

	double ref_sin_lat = sin(ref_lat_rad);
	double ref_cos_lat = cos(ref_lat_rad);

	double c = acos(ref_sin_lat * sin_lat + ref_cos_lat * cos_lat * cos_d_lon);
	double k = (fabs(c) < epsilon) ? 1.0 : (c / sin(c));

	tf2::Vector3 offset;

	offset.setX(k * cos_lat * sin(lon_rad - ref_lon_rad) * CONSTANTS_RADIUS_OF_EARTH);
	offset.setY(k * (ref_cos_lat * sin_lat - ref_sin_lat * cos_lat * cos_d_lon) * CONSTANTS_RADIUS_OF_EARTH);
	offset.setZ(coord.altitude - home.altitude);

	return offset;
}

geographic_msgs::GeoPoint SwarmVehicle::convertENUToGeo(const geometry_msgs::PoseStamped &local,
														const sensor_msgs::NavSatFix &home_global)
{
	static const float epsilon = std::numeric_limits<double>::epsilon();

	double x_rad = local.pose.position.x / CONSTANTS_RADIUS_OF_EARTH;
	double y_rad = local.pose.position.y / CONSTANTS_RADIUS_OF_EARTH;
	double c = sqrtf(x_rad * x_rad + y_rad * y_rad);
	double sin_c = sin(c);
	double cos_c = cos(c);

	double ref_lon_rad = home_global.longitude * M_DEG_TO_RAD;
	double ref_lat_rad = home_global.latitude * M_DEG_TO_RAD;

	double ref_sin_lat = sin(ref_lat_rad);
	double ref_cos_lat = cos(ref_lat_rad);

	double lat_rad;
	double lon_rad;

	if (fabs(c) > epsilon)
	{
		lat_rad = asin(cos_c * ref_sin_lat + (y_rad * sin_c * ref_cos_lat) / c);
		lon_rad = (ref_lon_rad + atan2(x_rad * sin_c, c * ref_cos_lat * cos_c - y_rad * ref_sin_lat * sin_c));
	}
	else
	{
		lat_rad = ref_lat_rad;
		lon_rad = ref_lon_rad;
	}

	geographic_msgs::GeoPoint geo_point;

	geo_point.latitude = lat_rad * M_RAD_TO_DEG;
	geo_point.longitude = lon_rad * M_RAD_TO_DEG;
	geo_point.altitude = local.pose.position.z + home_global.altitude;

	return geo_point;
}

bool SwarmVehicle::isPublish()
{
	multi_setpoint_publish_flag_ = true;
	for (auto &vehicle : camila_)
	{
		if (vehicle.isPublish() != true)
		{
			multi_setpoint_publish_flag_ = false;
			break;
		}
	}
	return multi_setpoint_publish_flag_;
}


void SwarmVehicle::triggerCB(const std_msgs::Empty::ConstPtr &trigger)
{
	std_msgs::Bool arm;
	std_msgs::String mode;

	trigger_arm_ = nh_.advertise<std_msgs::Bool>("/multi/arming", 10);
	trigger_mode_ = nh_.advertise<std_msgs::String>("/multi/set_mode", 10);

	arm.data = true;
	// 	}
	mode.data = "offboard";

	trigger_arm_.publish(arm);
	trigger_mode_.publish(mode);

}


void SwarmVehicle::setSwarmInfo(const std::string &swarm_name, const int &num_of_vehicle)
{
	swarm_name_ = swarm_name;
	num_of_vehicle_ = num_of_vehicle;

	release(); // service 및 camila vector 초기화

	camila_.reserve(num_of_vehicle_);
	VehicleInfo vehicle_info_[num_of_vehicle_];

	for (int i = 0; i < num_of_vehicle_; i++)
	{
		vehicle_info_[i].vehicle_id_ = i + 1;
		vehicle_info_[i].vehicle_name_ = swarm_name_ + std::to_string(i + 1);
		camila_.push_back(Vehicle(nh_mul_, nh_global_, vehicle_info_[i]));
	}

	multi_setpoint_local_server_ = nh_.advertiseService("multi_setpoint_local", &SwarmVehicle::multiSetpointLocal, this);
	multi_setpoint_global_server_ = nh_.advertiseService("multi_setpoint_global", &SwarmVehicle::multiSetpointGlobal, this);
	goto_vehicle_server_ = nh_.advertiseService("goto_vehicle", &SwarmVehicle::gotoVehicle, this);
}

std::string SwarmVehicle::getSwarmInfo() const
{
	return swarm_name_;
}

void SwarmVehicle::addVehicle(const VehicleInfo &vehicle_info)
{
	camila_.push_back(Vehicle(nh_mul_, nh_global_, vehicle_info));
	num_of_vehicle_++;
}

void SwarmVehicle::deleteVehicle(const VehicleInfo &vehicle_info)
{
	for (iter_ = camila_.begin(); iter_ != camila_.end(); iter_++)
	{
		VehicleInfo temp = iter_->getInfo();
		if ((temp.vehicle_id_ == vehicle_info.vehicle_id_) && (temp.vehicle_name_ == vehicle_info.vehicle_name_))
		{
			iter_ = camila_.erase(iter_);
			ROS_INFO_STREAM("Delete vehicle : sys_id : " << temp.vehicle_id_ << ", name : " << temp.vehicle_name_);
			num_of_vehicle_--;
			break;
		}
	}
}

void SwarmVehicle::showVehicleList() const
{
	for (auto &vehicle : camila_)
	{
		VehicleInfo info = vehicle.getInfo();
		ROS_INFO_STREAM(info.vehicle_id_ << " " << info.vehicle_name_);
	}
}

void SwarmVehicle::run()
{
	if (isPublish())
	{
		bool control_method, sp;
		double final_speed_limit;
		nh_global_.getParamCached("setpoint/kp_seek", kp_seek_);
		nh_global_.getParamCached("setpoint/kp_sp", kp_sp_);
		nh_global_.getParamCached("setpoint/range_sp", range_sp_);
		nh_global_.getParamCached("setpoint/vector_speed_limit", vector_speed_limit_);
		nh_global_.getParamCached("setpoint/final_speed_limit", final_speed_limit);
		nh_global_.getParamCached("setpoint/separate", sp);
		
		getVehiclePos();
		for (auto &vehicle : camila_)
		{
			tf2::Vector3 setpoint;
			seek(vehicle);
			if (sp)
			{
				// separate(vehicle);
				setpoint = vehicle.getSumOfSp() * kp_sp_ + vehicle.getErr() * kp_seek_;
			}
			else
				setpoint = vehicle.getErr() * kp_seek_;
			limit(setpoint, final_speed_limit);
			vehicle.setSetpointPos(setpoint);

			vehicle.gotoLocal();
		}
	}
	formationGenerator();
}