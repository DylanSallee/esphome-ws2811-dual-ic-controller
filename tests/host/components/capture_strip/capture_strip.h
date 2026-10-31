#pragma once

#include <cstdio>
#include <vector>

#include "esphome/components/light/addressable_light.h"

#if __has_include("esphome/components/light/esp_color_view.cpp")
#define CAPTURE_STRIP_COLOR_ACCESSOR
#endif

namespace esphome::capture_strip {

class CaptureStrip final : public light::AddressableLight {
 public:
  void set_num_leds(int32_t num_leds) { this->bytes_.resize(num_leds * 3), this->effect_.resize(num_leds); }
  int32_t size() const override { return this->effect_.size(); }
  void clear_effect_data() override { std::fill(this->effect_.begin(), this->effect_.end(), 0); }
  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void setup() override {
#ifdef CAPTURE_STRIP_COLOR_ACCESSOR
    this->accessor_impl_ =
        light::CorrectedBufferColorAccessor(this->bytes_.data(), this->effect_.data(), {0, 1, 2}, &this->correction_);
    this->accessor_ = &this->accessor_impl_;
#endif
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// One line per frame, so a test can follow exactly what the strip would have received.
  void write_state(light::LightState *state) override {
    printf("FRAME %u", static_cast<unsigned>(++this->frames_));
    for (uint8_t byte : this->bytes_)
      printf(" %u", byte);
    printf("\n");
    fflush(stdout);
  }

 protected:
#ifdef CAPTURE_STRIP_COLOR_ACCESSOR
  light::CorrectedBufferColorAccessor accessor_impl_;
#else
  light::ESPColorView get_view_internal(int32_t index) const override {
    auto *bytes = const_cast<uint8_t *>(this->bytes_.data()) + index * 3;
    auto *effect = const_cast<uint8_t *>(this->effect_.data()) + index;
    return {bytes, bytes + 1, bytes + 2, nullptr, effect, &this->correction_};
  }
#endif
  std::vector<uint8_t> bytes_;
  std::vector<uint8_t> effect_;
  uint32_t frames_{0};
};

}  // namespace esphome::capture_strip
