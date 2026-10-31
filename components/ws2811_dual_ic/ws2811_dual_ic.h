#pragma once

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_transformer.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "pixel_math.h"

// ESPHome PR #21577 replaces get_view_internal() with a colour accessor. It adds this file, which
// is a safer test than a version number because the PR has no confirmed release yet.
#if __has_include("esphome/components/light/esp_color_view.cpp")
#define WS2811_DUAL_IC_COLOR_ACCESSOR
#endif

namespace esphome::ws2811_dual_ic {

/// One RGB + CCT light on a strip with two WS2811 ICs per pixel.
///
/// This wraps an addressable strip that holds twice as many LEDs as there are pixels, and treats it
/// as a plain byte pipe to the ICs. Effects draw into frame_ with no correction applied. Gamma,
/// brightness and the power limit are then applied once, in float, as each frame is sent. That is
/// what keeps colours saturated when dim, and lets effects read their pixels back unchanged.
class WS2811DualICLight final : public light::AddressableLight {
  friend class CrossfadeTransformer;

 public:
  void set_strip(light::AddressableLightState *strip_state) { this->strip_state_ = strip_state; }
  void set_wire_order(uint8_t red, uint8_t green, uint8_t blue, uint8_t cold_white, uint8_t warm_white) {
    this->wire_offset_[RED] = red;
    this->wire_offset_[GREEN] = green;
    this->wire_offset_[BLUE] = blue;
    this->wire_offset_[COLD_WHITE] = cold_white;
    this->wire_offset_[WARM_WHITE] = warm_white;
  }
  void set_white_mireds(float cold, float warm) {
    this->cold_mireds_ = cold;
    this->warm_mireds_ = warm;
  }
  void set_constant_brightness(bool constant_brightness) { this->constant_brightness_ = constant_brightness; }
  void set_brightness_gamma(float gamma) { this->brightness_gamma_ = gamma; }
  void set_channel_balance(float red, float green, float blue, float white) {
    this->balance_ = {{red, green, blue, white, white}};
  }
  void set_white_leds(float cold_red, float cold_green, float cold_blue, float warm_red, float warm_green,
                      float warm_blue) {
    this->white_leds_ = {{cold_red, cold_green, cold_blue}, {warm_red, warm_green, warm_blue}};
  }

  /// Runtime calibration, safe to call before setup(). `channel` is 0 for red, 1 for green, 2 for blue.
  void set_color_balance(uint8_t channel, float balance);
  void set_white_strength(float strength);
  void set_white_mix(bool white_mix);

  /// Highest load allowed, as a share of the strip's rated power. Safe to call before setup().
  void set_power_limit(float limit);
  float get_power_limit() const { return this->power_limit_; }
  /// Share of the strip's rated power used by the last frame, assuming equal channel current.
  float get_load() const { return this->load_; }

  void setup() override;
  void dump_config() override;
  /// After every LightState (HARDWARE - 1), so the strip is ready and any restored state is known.
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.5f; }

  int32_t size() const override;
  void clear_effect_data() override;
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void update_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  std::unique_ptr<light::LightTransformer> create_default_transition() override;

 protected:
#ifdef WS2811_DUAL_IC_COLOR_ACCESSOR
  light::RawBufferColorAccessor frame_accessor_;
#else
  light::ESPColorView get_view_internal(int32_t index) const override;
  /// A default correction changes nothing, unlike correction_, which color_correct would touch.
  light::ESPColorCorrection passthrough_{};
#endif

  /// Recompute the shown colour after a calibration change.
  void refresh_();
  /// Drive for a colour at full brightness, including gamma and channel balance.
  Drive drive_for_(const light::LightColorValues &values) const;
  light::AddressableLight *strip_() const {
    return static_cast<light::AddressableLight *>(this->strip_state_->get_output());
  }
  void render_uniform_(float brightness);
  void render_effect_(float brightness);
  void send_pixel_(int32_t pixel, const uint8_t *levels);
  void send_ic_(int32_t index, const uint8_t *bytes);

  light::AddressableLightState *strip_state_{nullptr};
  uint8_t *frame_{nullptr};
  uint8_t *effect_data_{nullptr};
  float color_gamma_table_[256]{};

  uint8_t wire_offset_[NUM_CHANNELS]{0, 1, 2, 4, 3};  // "RGBWCX"
  float cold_mireds_{166.0f};
  float warm_mireds_{333.0f};
  float brightness_gamma_{2.2f};
  float power_limit_{1.0f};
  Drive balance_{{1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
  WhiteLeds white_leds_{};
  float white_strength_{1.0f};
  bool white_mix_{true};
  bool constant_brightness_{true};

  // What the light is showing now. While a transition runs, current_values lags behind these.
  Drive drive_{};
  float brightness_{0.0f};
  float load_{0.0f};
  bool frame_on_strip_{false};  // The strip shows frame_, not one uniform colour
  bool fading_frame_{false};    // A turn-off is fading out the frame an effect left behind
};

/// Fades colour in linear light and brightness along the slider's curve, so a change between RGB
/// and white is a crossfade. ESPHome's stock transition dips through black instead.
class CrossfadeTransformer final : public light::LightTransformer {
 public:
  explicit CrossfadeTransformer(WS2811DualICLight &light) : light_(light) {}

  void start() override;
  optional<light::LightColorValues> apply() override;
  void stop() override;

 protected:
  WS2811DualICLight &light_;
  Drive from_{};
  Drive to_{};
  float from_brightness_{0.0f};
  float to_brightness_{0.0f};
};

}  // namespace esphome::ws2811_dual_ic
