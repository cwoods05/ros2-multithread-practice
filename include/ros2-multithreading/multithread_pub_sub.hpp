#ifndef ROS2_MULTITHREADING_MULTITHREAD_PUB_SUB_HPP
#define ROS2_MULTITHREADING_MULTITHREAD_PUB_SUB_HPP

#include "ros2-multithreading/constants.hpp"

// ROS2
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_srvs/srv/set_bool.hpp>

// STL
#include <string>
#include <chrono>

namespace multithread
{
  class PubNode : public rclcpp::Node
  {
  public:
    explicit PubNode();
    ~PubNode();

  private:
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub2_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr rate_service_;
    rclcpp::Logger lg_;

    int count_;
    std::chrono::milliseconds timer_period_;

    void timerCB();
    void onRateChange(const std_srvs::srv::SetBool::Request::SharedPtr req,
                      std_srvs::srv::SetBool::Response::SharedPtr res);
  };

  class Sub2Node : public rclcpp_lifecycle::LifecycleNode
  {
  public:
    explicit Sub2Node();
    ~Sub2Node();

  protected:
    /**
     * \brief Configure node
     *
     */
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_configure(const rclcpp_lifecycle::State &state) override;

    /**
     * \brief Activate node
     *
     */
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_activate(const rclcpp_lifecycle::State &state) override;

    /**
     * \brief Deactivate node
     *
     */
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &state) override;

    /**
     * \brief Cleanup node
     *
     */
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_cleanup(const rclcpp_lifecycle::State &state) override;

    /**
     * \brief Shutdown node
     *
     */
    rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn on_shutdown(const rclcpp_lifecycle::State &state) override;

  private:
    rclcpp::CallbackGroup::SharedPtr callback_group_sub1;
    rclcpp::CallbackGroup::SharedPtr callback_group_sub2;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub1_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub2_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr latency_pub_;
    rclcpp::Logger lg_;

    rclcpp::Time last_msg_time_;
    size_t received_count_ = 0;
    bool enable_metrics_ = true;

    void subCB(const std_msgs::msg::String::ConstSharedPtr msg);
  };

} // namespace multithread

#endif /* ROS2_MULTITHREADING_MULTITHREAD_PUB_SUB_HPP */
