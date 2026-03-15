#include "ros2-multithreading/multithread_pub_sub.hpp"

namespace multithread
{
  PubNode::PubNode()
  : rclcpp::Node(util::PUB_NODE_NAME, rclcpp::NodeOptions().use_intra_process_comms(false))
  , lg_{this->get_logger()}
  , count_{0}
  , timer_period_{std::chrono::milliseconds(1000)}
  {
    pub_ = this->create_publisher<std_msgs::msg::String>(util::TOPIC_NAME, 10);
    pub2_ = this->create_publisher<std_msgs::msg::String>(std::string(util::TOPIC_NAME) + "_backup", 10);

    rate_service_ = this->create_service<std_srvs::srv::SetBool>(
      "set_publish_rate",
      std::bind(&PubNode::onRateChange, this, std::placeholders::_1, std::placeholders::_2)
    );

    timer_ = this->create_wall_timer(timer_period_,
      [this]() { this->timerCB(); }
    );

    RCLCPP_INFO(lg_, "PubNode initialized with %ld ms timer", timer_period_.count());
  }

  PubNode::~PubNode() = default;

  void PubNode::onRateChange(const std_srvs::srv::SetBool::Request::SharedPtr req,
                             std_srvs::srv::SetBool::Response::SharedPtr res)
  {
    if (req->data) {
      timer_period_ = std::chrono::milliseconds(250);
      res->message = "Switched to fast mode (250ms)";
    } else {
      timer_period_ = std::chrono::milliseconds(1000);
      res->message = "Switched to normal mode (1000ms)";
    }
    timer_->cancel();
    timer_ = this->create_wall_timer(timer_period_, [this]() { this->timerCB(); });
    res->success = true;
    RCLCPP_INFO(lg_, "%s (period = %ldms)", res->message.c_str(), timer_period_.count());
  }

  void PubNode::timerCB()
  {
    auto msg = std_msgs::msg::String();
    msg.data = "Hello World! " + std::to_string(this->count_++);

    auto string_thread_id = std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
    RCLCPP_INFO_STREAM(lg_, "THREAD " << string_thread_id << " has spoken '" << msg.data << "'");

    this->pub_->publish(msg);

    auto msg2 = std_msgs::msg::String();
    msg2.data = "[aux] " + msg.data;
    this->pub2_->publish(msg2);
  }

  Sub2Node::Sub2Node()
  : rclcpp_lifecycle::LifecycleNode(util::SUB_NODE_NAME, rclcpp::NodeOptions().use_intra_process_comms(false))
  , lg_{this->get_logger()}
  {
    this->declare_parameter<bool>("enable_metrics", true);
  }

  Sub2Node::~Sub2Node()
  {
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn Sub2Node::on_configure(const rclcpp_lifecycle::State &state)
  {
    /* These define the callback groups
     * They don't really do much on their own, but they have to exist in order to
     * assign callbacks to them. They're also what the executor looks for when trying to run multiple threads
     */
    callback_group_sub1 = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
    callback_group_sub2 = this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

    enable_metrics_ = this->get_parameter("enable_metrics").as_bool();
    if (enable_metrics_) {
      latency_pub_ = this->create_publisher<std_msgs::msg::Float64>(
        std::string(util::TOPIC_NAME) + "_latency", 10);
    }

    RCLCPP_INFO(lg_, "Sub2Node configured (metrics %s)",
      enable_metrics_ ? "enabled" : "disabled");

    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn Sub2Node::on_activate(const rclcpp_lifecycle::State &state)
  {
    // Each of these callback groups is basically a thread
    // Everything assigned to one of them gets bundled into the same thread
    auto sub1_opt = rclcpp::SubscriptionOptions();
    sub1_opt.callback_group = callback_group_sub1;
    auto sub2_opt = rclcpp::SubscriptionOptions();
    sub2_opt.callback_group = callback_group_sub2;

    sub1_ = this->create_subscription<std_msgs::msg::String>(
        util::TOPIC_NAME,
        rclcpp::QoS(1),
        [this](const std_msgs::msg::String::ConstSharedPtr msg)
        {
          this->subCB(msg);
        },
        // This is where we set the callback group.
        // This subscription will run with callback group subscriber1
        sub1_opt
    );

    sub2_ = this->create_subscription<std_msgs::msg::String>(
        util::TOPIC_NAME,
        rclcpp::QoS(1),
        [this](const std_msgs::msg::String::ConstSharedPtr msg)
        {
          this->subCB(msg);
        },
        // This is where we set the callback group.
        // This subscription will run with callback group subscriber1
        sub2_opt
    );

    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn Sub2Node::on_deactivate(const rclcpp_lifecycle::State &state)
  {
    sub1_.reset();
    sub2_.reset();
    latency_pub_.reset();
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn Sub2Node::on_cleanup(const rclcpp_lifecycle::State &state)
  {
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }

  rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn Sub2Node::on_shutdown(const rclcpp_lifecycle::State &state)
  {
    return rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn::SUCCESS;
  }


  void Sub2Node::subCB(const std_msgs::msg::String::ConstSharedPtr msg)
  {
    auto string_thread_id = std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
    auto now = this->now();
    if (received_count_ > 0) {
      auto dt = now - last_msg_time_;
      double interval_ms = dt.seconds() * 1000.0;
      RCLCPP_INFO(lg_, "Received msg %lu in %.2f ms", received_count_ + 1, interval_ms);

      if (enable_metrics_ && latency_pub_) {
        std_msgs::msg::Float64 latency_msg;
        latency_msg.data = interval_ms;
        latency_pub_->publish(latency_msg);
      }
    } else {
      RCLCPP_INFO(lg_, "Received first message");
    }
    last_msg_time_ = now;
    received_count_++;

    RCLCPP_INFO_STREAM(lg_, "THREAD " << string_thread_id << " => Heard '" << msg->data << "'");
  }
} // namespace multithread