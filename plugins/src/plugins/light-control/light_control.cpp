/***************************************************************************
 *  light_control.cpp - Plugin to control the light signals on an MPS
 *
 *  Created: Sat Feb 21 19:11:46 2015
 *  Copyright  2015  Frederik Zwilling
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include <math.h>

#include <utils/misc/gazebo_api_wrappers.h>

#include "light_control.h"

using namespace gazebo;

// Register this plugin to make it available in the simulator
GZ_REGISTER_MODEL_PLUGIN(LightControl)

///Constructor
LightControl::LightControl()
{
}
///Destructor
LightControl::~LightControl()
{
  printf("Destructing LightControl Plugin!\n");
}

/** on loading of the plugin
 * @param _parent Parent Model
 */
void LightControl::Load(physics::ModelPtr _parent, sdf::ElementPtr /*_sdf*/) 
{
  // Store the pointer to the model
  this->model_ = _parent;

  //get the model-name
  this->name_ = model_->GetName();
  printf("Loading LightControl Plugin of model %s\n", name_.c_str());

  //get name of the machine containing the light signal
  physics::ModelPtr machine = model_->GetParentModel();
  if(!machine)
  {
    printf("Error: There is a light signal that is not part of a mps! Light signal disabled!\n");
    return;
  }
  machine_name_ = machine->GetName();
  
  printf("MachSignal: parent machine: %s\n", machine_name_.c_str());

  // Listen to the update event. This event is broadcast every
  // simulation iteration.
  this->update_connection_ = event::Events::ConnectWorldUpdateBegin(boost::bind(&LightControl::OnUpdate, this, _1));

  //Create the communication Node for communication
  this->node_ = transport::NodePtr(new transport::Node());
  //the namespace is set to the world name!
  this->node_->Init(model_->GetWorld()->GZWRAP_NAME());

  //Create publisher to set visual properties
  visPub_ = this->node_->Advertise<msgs::Visual>("~/visual",
						 /*number of lights*/ 3*12);

  //subscribe for light status msgs
  light_msg_sub_ = node_->Subscribe(std::string(TOPIC_INSTRUCT_MACHINE), &LightControl::on_light_msg, this);

  world_ = model_->GetWorld();
  last_sent_time_ = world_->GZWRAP_SIM_TIME().Double();

  //initially turn lights off
  prev_state_red_ = prev_state_yellow_ = prev_state_green_ = llsf_msgs::ON;
  state_red_ = state_yellow_ = state_green_ = llsf_msgs::OFF;
}

/** Called by the world update start event
 */
void LightControl::OnUpdate(const common::UpdateInfo & /*_info*/)
{
  double time = world_->GZWRAP_SIM_TIME().Double();
  //wait until the world is completly loaded, otherwise the lights will spawn at (0,0)
  if(time < 20)
  {
    return;
  }  
  //update lights only twice a second
  if(time - last_sent_time_ < 0.5)
  {
    return;
  }
  last_sent_time_ = time;

  
  change_light(machine_name_, RED, state_red_, prev_state_red_);
  change_light(machine_name_, YELLOW, state_yellow_, prev_state_yellow_);
  change_light(machine_name_, GREEN, state_green_, prev_state_green_);
}

/** on Gazebo reset
 */
void LightControl::Reset()
{
}


/** Functions for recieving a light signal status msg
 * @param msg message
 */ 
void LightControl::on_light_msg(ConstInstructMachinePtr &msg)
{

        std::string machine_name = msg->machine();

    if (msg->set() != llsf_msgs::INSTRUCT_MACHINE_SET_SIGNAL_LIGHT &&
        machine_name != machine_name_){
        return;
    }

          state_red_ = msg->light_state().red();
          state_yellow_ = msg->light_state().yellow();
          state_green_ = msg->light_state().green();
}


//creates all needed visual messages
void LightControl::change_light(std::string machine_name, Color color, llsf_msgs::LightState &state, llsf_msgs::LightState &prev_state)
{
  //is there a change?
  if(state != llsf_msgs::BLINK && state == prev_state)
  {
    return;
  }
  prev_state = state;
  
  //create message to send
  msgs::Visual msg;

  //resolve BLINK (Machines Blink at 2Hz)
  if(state == llsf_msgs::BLINK)
  {
    double time = world_->GZWRAP_SIM_TIME().Double();
    if(fmod(time,1) >= 0.5)
    {
      state = llsf_msgs::OFF;
    }
    else
    {
      state = llsf_msgs::ON;
    }
  }
  
  //common parameters
  msgs::Geometry *geomMsg = msg.mutable_geometry();
  geomMsg->set_type(msgs::Geometry::CYLINDER);
  geomMsg->mutable_cylinder()->set_radius(0.02);
  geomMsg->mutable_cylinder()->set_length(0.034);
  msg.set_cast_shadows(false);
  
  //construct full path to link containing the light visual
  std::string parent_link = machine_name + "::light_signals::link";
  msg.set_parent_name(parent_link.c_str());

  //parameters dependent of color and state
  switch(color)
  {
  case RED:
    {
      msg.set_name((parent_link + "::redon").c_str());
#if GAZEBO_MAJOR_VERSION > 5
      msgs::Set(msg.mutable_pose(), ignition::math::Pose3d(0, 0, 0.085, 0, 0, 0));
#else
      msgs::Set(msg.mutable_pose(), math::Pose(0, 0, 0.085, 0, 0, 0));
#endif
      break;
    }
  case YELLOW:
    {
      msg.set_name((parent_link + "::yellowon").c_str());
#if GAZEBO_MAJOR_VERSION > 5
      msgs::Set(msg.mutable_pose(), ignition::math::Pose3d(0, 0, 0.051, 0, 0, 0));
#else
      msgs::Set(msg.mutable_pose(), math::Pose(0, 0, 0.051, 0, 0, 0));
#endif
      break;
    }
  case GREEN:
    {
      msg.set_name((parent_link + "::greenon").c_str());
#if GAZEBO_MAJOR_VERSION > 5
      msgs::Set(msg.mutable_pose(), ignition::math::Pose3d(0, 0, 0.017, 0, 0, 0));
#else
      msgs::Set(msg.mutable_pose(), math::Pose(0, 0, 0.017, 0, 0, 0));
#endif
      break;
    }
  }


  if(state == llsf_msgs::ON)
  {
    msg.set_visible(true);
    switch(color)
      {
    case RED:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(.8f, 0, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(1.0, .3f, .3f, 1.0));
          break;
      }
    case YELLOW:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(.9f, .7f, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(1.0, .9f, .3f, 1.0));
          break;
      }
    case GREEN:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(0, .8f, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(.3f, 1.0, .3f, 1.0));
          break;
      }
    }
  }
  else
  {
    msg.set_visible(false);
    switch(color)
      {
    case RED:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(.8f, 0, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(0.0, 0.0, 0.0, 0.0));
          msgs::Set(msg.mutable_material()->mutable_ambient(), gzwrap::Color(.8f, 0.0, 0.0, .8f));
          break;
      }
    case YELLOW:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(.9f, .7f, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(0.0, 0.0, 0.0, 0.0));
          msgs::Set(msg.mutable_material()->mutable_ambient(), gzwrap::Color(.9f, .7f, 0.0, .8f));
          break;
      }
    case GREEN:
      {
          msgs::Set(msg.mutable_material()->mutable_diffuse(), gzwrap::Color(0, .8f, 0, .8f));
          msgs::Set(msg.mutable_material()->mutable_emissive(), gzwrap::Color(0.0, 0.0, 0.0, 0.0));
          msgs::Set(msg.mutable_material()->mutable_ambient(), gzwrap::Color(0, .8f, 0, .8f));
          break;
      }
    }
  }

  visPub_->Publish(msg);
}
