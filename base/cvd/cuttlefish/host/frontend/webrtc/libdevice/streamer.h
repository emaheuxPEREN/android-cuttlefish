/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cuttlefish/host/libs/config/custom_actions.h"

#include "cuttlefish/host/frontend/webrtc/libcommon/audio_source.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/audio_sink.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/camera_controller.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/connection_observer.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/recording_manager.h"
#include "cuttlefish/host/frontend/webrtc/libdevice/video_sink.h"

namespace cuttlefish {
namespace webrtc_streaming {

class ClientHandler;

struct StreamerConfig {
  // The id with which to register with the operator server.
  std::string device_id;

  // The port on which the client files are being served
  int client_files_port;
  std::string operator_path;
  // The port ranges webrtc is allowed to use.
  // [0,0] means all ports
  std::pair<uint16_t, uint16_t> udp_port_range = {15550, 15599};
  std::pair<uint16_t, uint16_t> tcp_port_range = {15550, 15599};
  // WebRTC device id obtaining openwrt instance.
  std::string openwrt_device_id;
  // Openwrt IP address for accessing Luci interface.
  std::string openwrt_addr;
  // Adb port number of the device.
  int adb_port;
  // Path of ControlEnvProxyServer for serving Rest API in WebUI.
  std::string control_env_proxy_server_path;
  // Whether mouse is enabled.
  bool enable_mouse;
};

class OperatorObserver {
 public:
  virtual ~OperatorObserver() = default;
  // Called when the websocket connection with the operator is established.
  virtual void OnRegistered() = 0;
  // Called when the websocket connection with the operator is closed.
  virtual void OnClose() = 0;
  // Called when an error is encountered in the connection to the operator.
  virtual void OnError() = 0;
};

class Streamer {
 public:
  // The observer_factory will be used to create an observer for every new
  // client connection. Unregister() needs to be called to stop accepting
  // connections.
  static std::unique_ptr<Streamer> Create(
      const StreamerConfig& cfg, RecordingManager& recording_manager,
      std::shared_ptr<ConnectionObserverFactory> factory);
  ~Streamer() = default;

  std::shared_ptr<VideoSink> AddDisplay(const std::string& label, int width,
                                        int height, int dpi,
                                        bool touch_enabled);
  bool RemoveDisplay(const std::string& label);

  bool AddTouchpad(const std::string& label, int width, int height);

  void SetHardwareSpec(std::string key, std::string value);

  template <typename V>
  void SetHardwareSpec(std::string key, V value) {
    SetHardwareSpec(key, std::to_string(value));
  }

  std::shared_ptr<AudioSink> AddAudioStream(const std::string& label);
  // Grants access to streams originating on the client side. If there are
  // multiple streams (either because one client sends more than one or there
  // are several clients) the audio will be mixed and provided as a single
  // stream here.
  std::shared_ptr<AudioSource> GetAudioSource();

  CameraController* AddCamera(unsigned int port, unsigned int cid,
                              bool vhost_user);

  // Add a custom button to the control panel.
  void AddCustomControlPanelButton(const std::string& command,
                                   const std::string& title,
                                   const std::string& icon_name);
  void AddCustomControlPanelButtonWithShellCommand(
      const std::string& command, const std::string& title,
      const std::string& icon_name, const std::string& shell_command);
  void AddCustomControlPanelButtonWithDeviceStates(
      const std::string& command, const std::string& title,
      const std::string& icon_name,
      const std::vector<DeviceState>& device_states);

  // Register with the operator.
  void Register(std::weak_ptr<OperatorObserver> operator_observer);
  void Unregister();

 private:
  /*
   * Private Implementation idiom.
   * https://en.cppreference.com/w/cpp/language/pimpl
   */
  class Impl;

  Streamer(std::unique_ptr<Impl> impl);
  std::shared_ptr<Impl> impl_;
};

}  // namespace webrtc_streaming
}  // namespace cuttlefish
