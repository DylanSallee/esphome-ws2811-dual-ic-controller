#pragma once

// Pure color math for one dual-IC pixel. No ESPHome headers, so tests can run on a desktop.

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace esphome::ws2811_dual_ic {

enum Channel : uint8_t { RED = 0, GREEN = 1, BLUE = 2, COLD_WHITE = 3, WARM_WHITE = 4 };
static constexpr size_t NUM_CHANNELS = 5;

/// Linear drive of each LED channel, from 0 (off) to 1 (always on).
struct Drive {
  float level[NUM_CHANNELS]{};

  /// Share of the strip's rated power this drive uses, assuming equal current in every channel.
  float load() const {
    float sum = 0.0f;
    for (float value : this->level)
      sum += value;
    return sum / NUM_CHANNELS;
  }
};

inline Drive lerp(const Drive &from, const Drive &to, float progress) {
  Drive mixed;
  for (size_t i = 0; i < NUM_CHANNELS; i++)
    mixed.level[i] = from.level[i] + (to.level[i] - from.level[i]) * progress;
  return mixed;
}

/// Scale that brings `load` down to `limit`, and leaves anything already within it alone.
/// It is worked out at full brightness so the brightness slider keeps its whole range.
inline float limit_scale(float load, float limit) { return load > limit ? limit / load : 1.0f; }

/// Share `warm_fraction` of the light to the warm LEDs. The split is linear in mireds.
/// With constant_brightness the two channels always add up to 1. Without it the stronger channel
/// stays at 1, so the middle of the range is twice as bright as either end.
inline void split_white(float warm_fraction, bool constant_brightness, float *cold, float *warm) {
  const float warm_share = std::min(std::max(warm_fraction, 0.0f), 1.0f);
  const float cold_share = 1.0f - warm_share;
  const float divisor = constant_brightness ? 1.0f : std::max(warm_share, cold_share);
  *cold = cold_share / divisor;
  *warm = warm_share / divisor;
}

/// The light each white LED type gives per unit of luminance, as linear red, green and blue.
struct WhiteLeds {
  float cold[3]{1.0f, 1.0f, 1.0f};
  float warm[3]{1.0f, 1.0f, 1.0f};
};

/// The white light a color contains: its amount, and the share of that from the warm LEDs.
struct WhiteContent {
  float amount{0.0f};
  float warm_share{0.0f};
};

/// Find the most white light that fits inside a color without overshooting any of its channels.
/// A saturated color has none, so it stays on the color LEDs and cannot wash out. A color close
/// to a white the LEDs can make is nearly all white, which is how a Hue bulb renders its whites.
inline WhiteContent find_white(const float *color, const WhiteLeds &leds) {
  const auto amount_at = [&](float warm_share) {
    float amount = color[0] / (leds.cold[0] + (leds.warm[0] - leds.cold[0]) * warm_share);
    for (size_t i = 1; i < 3; i++)
      amount = std::min(amount, color[i] / (leds.cold[i] + (leds.warm[i] - leds.cold[i]) * warm_share));
    return amount;
  };
  WhiteContent best{amount_at(0.0f), 0.0f};
  const auto consider = [&](float warm_share) {
    const float amount = amount_at(warm_share);
    if (amount > best.amount)
      best = {amount, warm_share};
  };
  consider(1.0f);
  // The best mix is an end of the range, or a point where two channels run out together.
  for (size_t a = 0; a < 3; a++) {
    for (size_t b = a + 1; b < 3; b++) {
      const float slope_a = leds.warm[a] - leds.cold[a];
      const float slope_b = leds.warm[b] - leds.cold[b];
      const float divisor = color[a] * slope_b - color[b] * slope_a;
      if (divisor == 0.0f)
        continue;
      const float warm_share = (color[b] * leds.cold[a] - color[a] * leds.cold[b]) / divisor;
      if (warm_share > 0.0f && warm_share < 1.0f)
        consider(warm_share);
    }
  }
  return best;
}

/// Drive that shows a linear color as brightly as the LEDs allow.
/// `balance` scales the color LEDs so equal red, green and blue give neutral white. `white_strength`
/// is one white channel at full, relative to the balanced color LEDs at full.
inline Drive mix_color(const float *color, const float *balance, const WhiteLeds &leds, float white_strength,
                       bool use_white) {
  Drive drive;
  WhiteContent white;
  if (use_white)
    white = find_white(color, leds);
  for (size_t i = 0; i < 3; i++) {
    const float from_white = white.amount * (leds.cold[i] + (leds.warm[i] - leds.cold[i]) * white.warm_share);
    drive.level[i] = std::max(color[i] - from_white, 0.0f) * balance[i];
  }
  drive.level[COLD_WHITE] = white.amount * (1.0f - white.warm_share) / white_strength;
  drive.level[WARM_WHITE] = white.amount * white.warm_share / white_strength;

  float peak = 0.0f;
  for (float level : drive.level)
    peak = std::max(peak, level);
  if (peak > 0.0f) {
    for (float &level : drive.level)
      level /= peak;
  }
  return drive;
}

/// Round linear drive levels to the 8-bit values the WS2811 takes.
/// A lit pixel too dim for 8 bits keeps only its strongest channel at level 1. Raising every lit
/// channel to 1 instead would turn a dim color into dim white.
inline void quantize(const float *drive, uint8_t *levels, size_t count) {
  size_t strongest = 0;
  bool any_lit = false;
  for (size_t i = 0; i < count; i++) {
    const float rounded = drive[i] * 255.0f + 0.5f;
    levels[i] = rounded >= 255.0f ? 255 : static_cast<uint8_t>(rounded);
    any_lit = any_lit || levels[i] != 0;
    if (drive[i] > drive[strongest])
      strongest = i;
  }
  if (!any_lit && count > 0 && drive[strongest] > 0.0f)
    levels[strongest] = 1;
}

}  // namespace esphome::ws2811_dual_ic
