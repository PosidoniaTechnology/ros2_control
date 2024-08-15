// Copyright 2021 PAL Robotics S.L.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "controller_manager/controller_manager.hpp"
#include "controller_manager_test_common.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "test_chainable_controller/test_chainable_controller.hpp"
#include "test_controller/test_controller.hpp"

using ::testing::_;
using ::testing::Return;

using namespace std::chrono_literals;
class TestHardwareSpawner : public ControllerManagerFixture<controller_manager::ControllerManager>
{
public:
  TestHardwareSpawner()
  : ControllerManagerFixture<controller_manager::ControllerManager>()
  {
    cm_->set_parameter(rclcpp::Parameter("hardware_components_initial_state.unconfigured", "TestSystemHardware"));
  }

  void SetUp() override
  {
    ControllerManagerFixture::SetUp();

    update_executor_ =
    std::make_shared<rclcpp::executors::MultiThreadedExecutor>(rclcpp::ExecutorOptions(), 2);

    update_executor_->add_node(cm_);
    update_executor_spin_future_ =
    std::async(std::launch::async, [this]() -> void { update_executor_->spin(); });
    // This sleep is needed to prevent a too fast test from ending before the
    // executor has began to spin, which causes it to hang
    std::this_thread::sleep_for(50ms);
  }

  void TearDown() override { update_executor_->cancel(); }

protected:
  // Using a MultiThreadedExecutor so we can call update on a separate thread from service callbacks
  std::shared_ptr<rclcpp::Executor> update_executor_;
  std::future<void> update_executor_spin_future_;
};

int call_spawner(const std::string extra_args)
{
  std::string spawner_script = "ros2 run controller_manager hardware_spawner ";
  return std::system((spawner_script + extra_args).c_str());
}

TEST_F(TestHardwareSpawner, spawner_with_no_arguments_errors)
{
  EXPECT_NE(call_spawner(""), 0) << "Missing mandatory arguments";
}

TEST_F(TestHardwareSpawner, spawner_without_manager_errors_with_given_timeout)
{
  EXPECT_NE(call_spawner("TestSystemHardware --controller-manager-timeout 1.0"), 0)
    << "Wrong controller manager name";
}

TEST_F(TestHardwareSpawner, spawner_without_component_name_argument)
{
  EXPECT_NE(call_spawner("-c test_controller_manager"), 0) <<
  "Missing component name argument parameter";
}

TEST_F(TestHardwareSpawner, spawner_non_exising_hardware_component)
{
  EXPECT_NE(call_spawner("TestSystemHardware1 -c test_controller_manager"), 0) <<
  "Missing component name argument parameter";
}

TEST_F(TestHardwareSpawner, set_component_to_configured_state_and_back_to_activated)
{
  EXPECT_EQ(call_spawner("TestSystemHardware --configure -c test_controller_manager"), 0);

  EXPECT_EQ(call_spawner("TestSystemHardware --activate -c test_controller_manager"), 0);
}


class TestHardwareSpawnerWithoutRobotDescription
: public ControllerManagerFixture<controller_manager::ControllerManager>
{
public:
  TestHardwareSpawnerWithoutRobotDescription()
  : ControllerManagerFixture<controller_manager::ControllerManager>("")
  {
    cm_->set_parameter(rclcpp::Parameter("hardware_components_initial_state.unconfigured", "TestSystemHardware"));
  }

public:
  void SetUp() override
  {
    ControllerManagerFixture::SetUp();

    update_timer_ = cm_->create_wall_timer(
      std::chrono::milliseconds(10),
      [&]()
      {
        cm_->read(time_, PERIOD);
        cm_->update(time_, PERIOD);
        cm_->write(time_, PERIOD);
      });

    update_executor_ =
    std::make_shared<rclcpp::executors::MultiThreadedExecutor>(rclcpp::ExecutorOptions(), 2);

    update_executor_->add_node(cm_);
    update_executor_spin_future_ =
    std::async(std::launch::async, [this]() -> void { update_executor_->spin(); });
    // This sleep is needed to prevent a too fast test from ending before the
    // executor has began to spin, which causes it to hang
    std::this_thread::sleep_for(50ms);
  }

  void TearDown() override { update_executor_->cancel(); }

  rclcpp::TimerBase::SharedPtr robot_description_sending_timer_;

protected:
  rclcpp::TimerBase::SharedPtr update_timer_;

  // Using a MultiThreadedExecutor so we can call update on a separate thread from service callbacks
  std::shared_ptr<rclcpp::Executor> update_executor_;
  std::future<void> update_executor_spin_future_;
};

TEST_F(TestHardwareSpawnerWithoutRobotDescription, when_no_robot_description_spawner_times_out)
{
  EXPECT_EQ(call_spawner("TestSystemHardware --configure -c test_controller_manager --controller-manager-timeout 1.0"), 256)
    << "could not change hardware state because not robot description and not services for controller "
       "manager are active";
}

TEST_F(
  TestHardwareSpawnerWithoutRobotDescription,
  spawner_with_later_load_of_robot_description)
{
  // Delay sending robot description
  robot_description_sending_timer_ = cm_->create_wall_timer(
    std::chrono::milliseconds(2500), [&]() { pass_robot_description_to_cm_and_rm(); });

  EXPECT_EQ(call_spawner("TestSystemHardware --configure -c test_controller_manager"), 1)
    << "could not activate control because not robot description";
}

class TestHardwareSpawnerWithNamespacedCM
: public ControllerManagerFixture<controller_manager::ControllerManager>
{
public:
  TestHardwareSpawnerWithNamespacedCM()
  : ControllerManagerFixture<controller_manager::ControllerManager>(
      ros2_control_test_assets::minimal_robot_urdf, "foo_namespace")
  {
    cm_->set_parameter(rclcpp::Parameter("hardware_components_initial_state.unconfigured", "TestSystemHardware"));
  }

public:
  void SetUp() override
  {
    ControllerManagerFixture::SetUp();

    update_executor_ =
    std::make_shared<rclcpp::executors::MultiThreadedExecutor>(rclcpp::ExecutorOptions(), 2);

    update_executor_->add_node(cm_);
    update_executor_spin_future_ =
    std::async(std::launch::async, [this]() -> void { update_executor_->spin(); });
    // This sleep is needed to prevent a too fast test from ending before the
    // executor has began to spin, which causes it to hang
    std::this_thread::sleep_for(50ms);
  }

  void TearDown() override { update_executor_->cancel(); }

protected:
  // Using a MultiThreadedExecutor so we can call update on a separate thread from service callbacks
  std::shared_ptr<rclcpp::Executor> update_executor_;
  std::future<void> update_executor_spin_future_;
};

TEST_F(TestHardwareSpawnerWithNamespacedCM, set_component_to_configured_state_cm_namespace)
{
  ControllerManagerRunner cm_runner(this);
  EXPECT_EQ(
    call_spawner("TestSystemHardware --configure -c test_controller_manager --controller-manager-timeout 1.0"), 256)
    << "Should fail without defining the namespace";
  EXPECT_EQ(
    call_spawner("TestSystemHardware --configure -c test_controller_manager --ros-args -r __ns:=/foo_namespace"), 0);
}