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

#include <vector>

#include <api/video/video_frame_buffer.h>

namespace cuttlefish {

class CvdVideoFrameBuffer : public webrtc::I420BufferInterface {
 public:
  CvdVideoFrameBuffer(int width, int height);
  ~CvdVideoFrameBuffer() override = default;

  // From VideoFrameBuffer
  int width() const override;
  int height() const override;

  // From class PlanarYuvBuffer
  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

  // From class PlanarYuv8Buffer
  const uint8_t *DataY() const override;
  const uint8_t *DataU() const override;
  const uint8_t *DataV() const override;

  uint8_t *DataY() { return y_.data(); }
  uint8_t *DataU() { return u_.data(); }
  uint8_t *DataV() { return v_.data(); }

 private:
  const int width_;
  const int height_;
  std::vector<std::uint8_t> y_;
  std::vector<std::uint8_t> u_;
  std::vector<std::uint8_t> v_;
};

}
