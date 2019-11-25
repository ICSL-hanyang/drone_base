#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <math.h>
#include <geometry_msgs/PoseStamped.h>




void LocalposeCallback(const geometry_msgs::PoseStamped::ConstPtr& msg){
    static tf::TransformBroadcaster br;
    tf::Transform transform;
    transform.setOrigin( tf::Vector3(0, 0, 0.2) );
    
    tf::Quaternion q;
    q.setRPY(M_PI_2l, M_PI, M_PI_2l);
    transform.setRotation(q);

    // transform.setOrigin( tf::Vector3(msg->pose.position.x, msg->pose.position.y, msg->pose.position.z) );
    br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "base_link", "odometry"));

}


int main(int argc, char** argv){
    ros::init(argc, argv, "odometry_tf_pub_node");

    ros::NodeHandle node;
    ros::Subscriber local_pose_sub = node.subscribe("/camila1/mavros/local_position/pose", 10, &LocalposeCallback);

    ros::spin();
    return 0;
};
