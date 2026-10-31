#include "ws2811_dual_ic.h"

#include <cinttypes>
#include <cmath>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::ws2811_dual_ic {

static const char *const TAG = "ws2811_dual_ic";

static constexpr uint8_t WIRE_BYTES_PER_IC = 3;
static constexpr uint8_t ICS_PER_PIXEL = 2;
static constexpr float MIN_POWER_LIMIT = 0.01f;
static constexpr float MIN_CALIBRATION = 0.01f;

int32_t WS2811DualICLight::size() const { return this->strip_()->size() / ICS_PER_PIXEL; }

void WS2811DualICLight::setup() {
  const size_t pixels = this->size();
  if (pixels == 0) {
    ESP_LOGE(TAG, "The strip needs at least two ICs");
    this->mark_failed();
    return;
  }
  RAMAllocator<uint8_t> allocator;
  this->frame_ = allocator.allocate(pixels * 3);
  this->effect_data_ = allocator.allocate(pixels);
  if (this->frame_ == nullptr || this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate the frame buffers");
    this->frame_ = nullptr;
    this->mark_failed();
    return;
  }
  memset(this->frame_, 0, pixels * 3);
  memset(this->effect_data_, 0, pixels);
#ifdef WS2811_DUAL_IC_COLOR_ACCESSOR
  this->frame_accessor_ = light::RawBufferColorAccessor(this->frame_, this->effect_data_, light::ChannelColors(0, 1, 2));
  this->accessor_ = &this->frame_accessor_;
#endif

  // With nothing restored, the strip's own light starts a default fade to black at boot, and every
  // step of it would repaint the bytes sent below. Finish it now.
  this->strip_state_->turn_off().set_transition_length(0).set_publish(false).set_save(false).perform();

  // Send the first frame now. Until then the ICs sit in their power-on state, which is lit.
  this->write_state(this->state_parent_);
  this->strip_()->write_state(this->strip_state_);
}

void WS2811DualICLight::dump_config() {
  ESP_LOGCONFIG(TAG,
                "WS2811 dual IC light:\n"
                "  Pixels: %" PRId32 "\n"
                "  Cold white: %.0f mireds\n"
                "  Warm white: %.0f mireds\n"
                "  Constant brightness: %s\n"
                "  Brightness gamma: %.2f\n"
                "  Power limit: %.0f%%",
                this->size(), this->cold_mireds_, this->warm_mireds_, YESNO(this->constant_brightness_),
                this->brightness_gamma_, this->power_limit_ * 100.0f);
  if (this->strip_()->size() % ICS_PER_PIXEL != 0)
    ESP_LOGW(TAG, "The strip has an odd number of ICs, so the last one is not driven");
}

void WS2811DualICLight::set_power_limit(float limit) {
  this->power_limit_ = std::min(std::max(limit, MIN_POWER_LIMIT), 1.0f);
  if (this->state_parent_ != nullptr)
    this->schedule_show();
}

void WS2811DualICLight::refresh_() {
  if (this->state_parent_ == nullptr)
    return;
  if (!this->is_effect_active())
    this->drive_ = this->drive_for_(this->state_parent_->current_values);
  this->schedule_show();
}

void WS2811DualICLight::set_color_balance(uint8_t channel, float balance) {
  if (channel > BLUE)
    return;
  this->balance_.level[channel] = std::max(balance, MIN_CALIBRATION);
  this->refresh_();
}

void WS2811DualICLight::set_white_strength(float strength) {
  this->white_strength_ = std::max(strength, MIN_CALIBRATION);
  this->refresh_();
}

void WS2811DualICLight::set_white_mix(bool white_mix) {
  this->white_mix_ = white_mix;
  this->refresh_();
}

void WS2811DualICLight::clear_effect_data() {
  if (this->frame_ == nullptr)
    return;
  // An effect should start from black, not from whatever the last effect left behind.
  memset(this->frame_, 0, this->size() * 3);
  memset(this->effect_data_, 0, this->size());
}

light::LightTraits WS2811DualICLight::get_traits() {
  // Never RGB_COLD_WARM_WHITE. Home Assistant would show it as RGBWW and move colour into the white
  // LEDs through color_rgb_to_rgbww, which ignores how much brighter they are. mix_color() does this
  // here instead, using the real strength of the LEDs.
  light::LightTraits traits;
  traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::COLOR_TEMPERATURE});
  traits.set_min_mireds(this->cold_mireds_);
  traits.set_max_mireds(this->warm_mireds_);
  return traits;
}

void WS2811DualICLight::setup_state(light::LightState *state) {
  // Deliberately not the base class version, which would apply gamma as effects write each pixel.
  this->state_parent_ = state;
  for (int level = 0; level < 256; level++)
    this->color_gamma_table_[level] = state->gamma_correct_lut(level / 255.0f);
}

Drive WS2811DualICLight::drive_for_(const light::LightColorValues &values) const {
  Drive drive;
  const auto mode = values.get_color_mode();
  if (mode & light::ColorCapability::RGB) {
    const float color[3] = {this->state_parent_->gamma_correct_lut(values.get_red()),
                            this->state_parent_->gamma_correct_lut(values.get_green()),
                            this->state_parent_->gamma_correct_lut(values.get_blue())};
    drive = mix_color(color, this->balance_.level, this->white_leds_, this->white_strength_, this->white_mix_);
    const float color_brightness = this->state_parent_->gamma_correct_lut(values.get_color_brightness());
    for (float &level : drive.level)
      level *= color_brightness;
  } else if (mode & light::ColorCapability::COLOR_TEMPERATURE) {
    const float warm_fraction =
        (values.get_color_temperature() - this->cold_mireds_) / (this->warm_mireds_ - this->cold_mireds_);
    split_white(warm_fraction, this->constant_brightness_, &drive.level[COLD_WHITE], &drive.level[WARM_WHITE]);
    drive.level[COLD_WHITE] *= this->balance_.level[COLD_WHITE];
    drive.level[WARM_WHITE] *= this->balance_.level[WARM_WHITE];
  }
  return drive;
}

void WS2811DualICLight::update_state(light::LightState *state) {
  const auto &values = state->current_values;
  this->brightness_ = values.get_state() * values.get_brightness();
  this->fading_frame_ = false;
  if (!this->is_effect_active())
    this->drive_ = this->drive_for_(values);
}

void WS2811DualICLight::write_state(light::LightState *state) {
  if (this->frame_ == nullptr)
    return;
  const float brightness = this->brightness_ > 0.0f ? std::pow(this->brightness_, this->brightness_gamma_) : 0.0f;
  this->frame_on_strip_ = this->is_effect_active() || this->fading_frame_;
  if (this->frame_on_strip_) {
    this->render_effect_(brightness);
  } else {
    this->render_uniform_(brightness);
  }
  this->strip_()->schedule_show();
}

void WS2811DualICLight::render_uniform_(float brightness) {
  const float scale = brightness * limit_scale(this->drive_.load(), this->power_limit_);
  float scaled[NUM_CHANNELS];
  uint8_t levels[NUM_CHANNELS];
  for (size_t channel = 0; channel < NUM_CHANNELS; channel++)
    scaled[channel] = this->drive_.level[channel] * scale;
  quantize(scaled, levels, NUM_CHANNELS);

  const int32_t pixels = this->size();
  for (int32_t pixel = 0; pixel < pixels; pixel++)
    this->send_pixel_(pixel, levels);

  uint32_t sum = 0;
  for (uint8_t level : levels)
    sum += level;
  this->load_ = sum / (255.0f * NUM_CHANNELS);
}

void WS2811DualICLight::render_effect_(float brightness) {
  const int32_t pixels = this->size();
  float frame_load = 0.0f;
  for (int32_t pixel = 0; pixel < pixels; pixel++) {
    const uint8_t *color = this->frame_ + pixel * 3;
    for (size_t channel = RED; channel <= BLUE; channel++)
      frame_load += this->color_gamma_table_[color[channel]] * this->balance_.level[channel];
  }
  frame_load /= pixels * NUM_CHANNELS;

  const float scale = brightness * limit_scale(frame_load, this->power_limit_);
  uint32_t sum = 0;
  for (int32_t pixel = 0; pixel < pixels; pixel++) {
    const uint8_t *color = this->frame_ + pixel * 3;
    float scaled[NUM_CHANNELS]{};
    uint8_t levels[NUM_CHANNELS]{};
    for (size_t channel = RED; channel <= BLUE; channel++)
      scaled[channel] = this->color_gamma_table_[color[channel]] * this->balance_.level[channel] * scale;
    quantize(scaled, levels, BLUE + 1);
    this->send_pixel_(pixel, levels);
    sum += levels[RED] + levels[GREEN] + levels[BLUE];
  }
  this->load_ = sum / (255.0f * NUM_CHANNELS * pixels);
}

void WS2811DualICLight::send_pixel_(int32_t pixel, const uint8_t *levels) {
  uint8_t wire[ICS_PER_PIXEL * WIRE_BYTES_PER_IC]{};
  for (size_t channel = 0; channel < NUM_CHANNELS; channel++)
    wire[this->wire_offset_[channel]] = levels[channel];
  this->send_ic_(pixel * ICS_PER_PIXEL, wire);
  this->send_ic_(pixel * ICS_PER_PIXEL + 1, wire + WIRE_BYTES_PER_IC);
}

void WS2811DualICLight::send_ic_(int32_t index, const uint8_t *bytes) {
  // The strip's own brightness and gamma must not touch these bytes.
#ifdef WS2811_DUAL_IC_COLOR_ACCESSOR
  (*this->strip_())[index].set_raw(Color(bytes[0], bytes[1], bytes[2]));
#else
  light::ESPColorView view = (*this->strip_())[index];
  view.raw_set_color_correction(&this->passthrough_);
  view.set_rgb(bytes[0], bytes[1], bytes[2]);
#endif
}

#ifndef WS2811_DUAL_IC_COLOR_ACCESSOR
light::ESPColorView WS2811DualICLight::get_view_internal(int32_t index) const {
  // LightState keeps running effects after a failed setup(), so give them somewhere safe to write.
  static uint8_t scratch[3];
  if (this->frame_ == nullptr)
    return {scratch, scratch + 1, scratch + 2, nullptr, nullptr, &this->passthrough_};
  // The white pointer stays null, so an effect can never add white to a colour.
  uint8_t *color = this->frame_ + index * 3;
  return {color, color + 1, color + 2, nullptr, this->effect_data_ + index, &this->passthrough_};
}
#endif

std::unique_ptr<light::LightTransformer> WS2811DualICLight::create_default_transition() {
  return make_unique<CrossfadeTransformer>(*this);
}

void CrossfadeTransformer::start() {
  // Start from what is on the strip, because current_values is stale if a transition was interrupted.
  const auto &target = this->get_target_values();
  this->from_ = this->light_.drive_;
  this->from_brightness_ = this->light_.brightness_;
  this->to_ = target.is_on() ? this->light_.drive_for_(target) : this->from_;
  this->to_brightness_ = target.get_state() * target.get_brightness();
  // Turning on should fade the new colour up, not fade across from the colour it had when it went off.
  if (this->from_brightness_ == 0.0f)
    this->from_ = this->to_;
  // ESPHome stops an effect just before a turn-off fade, so keep fading the pixels it left behind.
  this->light_.fading_frame_ = this->light_.frame_on_strip_ && !target.is_on();
}

optional<light::LightColorValues> CrossfadeTransformer::apply() {
  const float progress = LightTransformer::smoothed_progress(this->get_progress_());
  // A running effect owns the pixels, so only its brightness and colour inputs can fade.
  if (this->light_.is_effect_active())
    return light::LightColorValues::lerp(this->get_start_values(), this->get_target_values(), progress);

  this->light_.drive_ = lerp(this->from_, this->to_, progress);
  this->light_.brightness_ = this->from_brightness_ + (this->to_brightness_ - this->from_brightness_) * progress;
  this->light_.schedule_show();
  return {};
}

void CrossfadeTransformer::stop() {
  // The last apply() can land just short of the end, so finish on the exact target.
  if (!this->light_.is_effect_active())
    this->light_.drive_ = this->to_;
  this->light_.brightness_ = this->to_brightness_;
  this->light_.fading_frame_ = false;
  this->light_.schedule_show();
}

}  // namespace esphome::ws2811_dual_ic
