/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <boost/bind.hpp>

#include <OgreManualObject.h>
#include <OgreMaterialManager.h>
#include <OgreRectangle2D.h>
#include <OgreRenderSystem.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreTextureManager.h>
#include <OgreViewport.h>
#include <OgreTechnique.h>
#include <OgreCamera.h>

#include <tf/transform_listener.h>

#include "rviz/display_context.h"
#include "rviz/frame_manager.h"
#include "rviz/render_panel.h"
#include "rviz/validate_floats.h"

#include <sensor_msgs/image_encodings.h>

#include "color_depth_image_display.h"

namespace smart_car
{

ColorDepthImageDisplay::ColorDepthImageDisplay()
  : ::rviz::ImageDisplayBase()
  , texture_()
{
  normalize_property_ = new ::rviz::BoolProperty( "Normalize Range", true,
                                          "If set to true, will try to estimate the range of possible values from the received images.",
                                          this, SLOT( updateNormalizeOptions() ));

  min_property_ = new ::rviz::FloatProperty( "Min Value", 0.0, "Value which will be displayed as black.", this, SLOT( updateNormalizeOptions() ));

  max_property_ = new ::rviz::FloatProperty( "Max Value", 1.0, "Value which will be displayed as white.", this, SLOT( updateNormalizeOptions() ));

  median_buffer_size_property_ = new ::rviz::IntProperty( "Median window", 5, "Window size for median filter used for computin min/max.",
                                                  this, SLOT( updateNormalizeOptions() ) );

  color_depth_switch_property_ = new ::rviz::BoolProperty( "Color Depth", true, "Switch between enable and disable colorized depth data.",
                                                  this, SLOT( updateNormalizeOptions() ) );

  got_float_image_ = false;

  is_coloried_ = true;
}

void ColorDepthImageDisplay::onInitialize()
{
  ImageDisplayBase::onInitialize();
  {
    static uint32_t count = 0;
    std::stringstream ss;
    ss << "ColorDepthImageDisplay" << count++;
    img_scene_manager_ = Ogre::Root::getSingleton().createSceneManager(Ogre::ST_GENERIC, ss.str());
  }

  img_scene_node_ = img_scene_manager_->getRootSceneNode()->createChildSceneNode();

  {
    static int count = 0;
    std::stringstream ss;
    ss << "ImageDisplayObject" << count++;

    screen_rect_ = new Ogre::Rectangle2D(true);
    screen_rect_->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
    screen_rect_->setCorners(-1.0f, 1.0f, 1.0f, -1.0f);

    ss << "Material";
    material_ = Ogre::MaterialManager::getSingleton().create( ss.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );
    material_->setSceneBlending( Ogre::SBT_REPLACE );
    material_->setDepthWriteEnabled(false);
    material_->setReceiveShadows(false);
    material_->setDepthCheckEnabled(false);

    material_->getTechnique(0)->setLightingEnabled(false);
    Ogre::TextureUnitState* tu = material_->getTechnique(0)->getPass(0)->createTextureUnitState();
    tu->setTextureName(texture_.getTexture()->getName());
    tu->setTextureFiltering( Ogre::TFO_NONE );

    material_->setCullingMode(Ogre::CULL_NONE);
    Ogre::AxisAlignedBox aabInf;
    aabInf.setInfinite();
    screen_rect_->setBoundingBox(aabInf);
    screen_rect_->setMaterial(material_->getName());
    img_scene_node_->attachObject(screen_rect_);
  }

  render_panel_ = new ::rviz::RenderPanel();
  render_panel_->getRenderWindow()->setAutoUpdated(false);
  render_panel_->getRenderWindow()->setActive( false );

  render_panel_->resize( 640, 480 );
  render_panel_->initialize(img_scene_manager_, context_);

  setAssociatedWidget( render_panel_ );

  render_panel_->setAutoRender(false);
  render_panel_->setOverlaysEnabled(false);
  render_panel_->getCamera()->setNearClipDistance( 0.01f );

  updateNormalizeOptions();
}

ColorDepthImageDisplay::~ColorDepthImageDisplay()
{
  if ( initialized() )
  {
    delete render_panel_;
    delete screen_rect_;
    img_scene_node_->getParentSceneNode()->removeAndDestroyChild( img_scene_node_->getName() );
  }
}

void ColorDepthImageDisplay::onEnable()
{
  ImageDisplayBase::subscribe();
  render_panel_->getRenderWindow()->setActive(true);
}

void ColorDepthImageDisplay::onDisable()
{
  render_panel_->getRenderWindow()->setActive(false);
  ImageDisplayBase::unsubscribe();
  clear();
}

void ColorDepthImageDisplay::updateNormalizeOptions()
{
  if (got_float_image_)
  {
    bool normalize = normalize_property_->getBool();

    normalize_property_->setHidden(false);
    min_property_->setHidden(normalize);
    max_property_->setHidden(normalize);
    median_buffer_size_property_->setHidden(!normalize);

    texture_.setNormalizeFloatImage( normalize, min_property_->getFloat(), max_property_->getFloat());
    texture_.setMedianFrames( median_buffer_size_property_->getInt() );
  }
  else
  {
    normalize_property_->setHidden(true);
    min_property_->setHidden(true);
    max_property_->setHidden(true);
    median_buffer_size_property_->setHidden(true);
  }

  is_coloried_ = color_depth_switch_property_->getBool();
}

void ColorDepthImageDisplay::clear()
{
  texture_.clear();

  if( render_panel_->getCamera() )
  {
    render_panel_->getCamera()->setPosition(Ogre::Vector3(999999, 999999, 999999));
  }
}

void ColorDepthImageDisplay::update( float wall_dt, float ros_dt )
{
  try
  {
    texture_.update();

    //make sure the aspect ratio of the image is preserved
    float win_width = render_panel_->width();
    float win_height = render_panel_->height();

    float img_width = texture_.getWidth();
    float img_height = texture_.getHeight();

    if ( img_width != 0 && img_height != 0 && win_width !=0 && win_height != 0 )
    {
      float img_aspect = img_width / img_height;
      float win_aspect = win_width / win_height;

      if ( img_aspect > win_aspect )
      {
        screen_rect_->setCorners(-1.0f, 1.0f * win_aspect/img_aspect, 1.0f, -1.0f * win_aspect/img_aspect, false);
      }
      else
      {
        screen_rect_->setCorners(-1.0f * img_aspect/win_aspect, 1.0f, 1.0f * img_aspect/win_aspect, -1.0f, false);
      }
    }

    render_panel_->getRenderWindow()->update();
  }
  catch( ::rviz::UnsupportedImageEncoding& e )
  {
    setStatus(::rviz::StatusProperty::Error, "Image", e.what());
  }
}

void ColorDepthImageDisplay::reset()
{
  ImageDisplayBase::reset();
  clear();
}

/* This is called by incomingMessage(). */
void ColorDepthImageDisplay::processMessage(const sensor_msgs::Image::ConstPtr& msg)
{
  bool got_float_image = msg->encoding == sensor_msgs::image_encodings::TYPE_32FC1 ||
      msg->encoding == sensor_msgs::image_encodings::TYPE_16UC1 ||
      msg->encoding == sensor_msgs::image_encodings::TYPE_16SC1 ||
      msg->encoding == sensor_msgs::image_encodings::MONO16;

  if ( got_float_image != got_float_image_ )
  {
    got_float_image_ = got_float_image;
    updateNormalizeOptions();
  }

  if(is_coloried_)
  {
    colorizeDepthImage(const_cast<sensor_msgs::Image*>(msg.get()));
  }

  texture_.addMessage(msg);
}

void ColorDepthImageDisplay::colorizeDepthImage(sensor_msgs::Image* img)
{
  // keep header, height, width, is_bigendian unchanged

  auto pixel_step = img->step / img->width;

  // encoding
  img->encoding = ::sensor_msgs::image_encodings::RGB8;
  
  // step
  img->step = img->width * 3; // 1 pixel(R,G,B) = 3Bytes

  // data
  colorizer_.process_frame(img->data, pixel_step); 
}

} // namespace rviz

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS( smart_car::ColorDepthImageDisplay, rviz::Display )
