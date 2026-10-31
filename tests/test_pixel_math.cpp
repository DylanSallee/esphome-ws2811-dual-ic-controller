// Desktop tests for the colour math. Run tests/run.sh, or build this file with any C++17 compiler.

#include <cassert>
#include <cmath>
#include <cstdio>

#include "../components/ws2811_dual_ic/pixel_math.h"

using namespace esphome::ws2811_dual_ic;

static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

static void test_dim_colour_stays_a_colour() {
  // The drive of orange so dim that only the strongest channel can stay lit.
  const float drive[3] = {0.0015f, 0.0002f, 0.00001f};
  uint8_t levels[3];
  quantize(drive, levels, 3);
  assert(levels[0] == 1 && levels[1] == 0 && levels[2] == 0);
}

static void test_black_stays_black() {
  const float drive[5] = {};
  uint8_t levels[5] = {9, 9, 9, 9, 9};
  quantize(drive, levels, 5);
  for (uint8_t level : levels)
    assert(level == 0);
}

static void test_rounding_and_clamping() {
  const float drive[3] = {1.0f, 2.0f, 0.5f};
  uint8_t levels[3];
  quantize(drive, levels, 3);
  assert(levels[0] == 255 && levels[1] == 255 && levels[2] == 128);
}

static void test_limit_only_acts_above_the_budget() {
  assert(near(limit_scale(0.1f, 0.15f), 1.0f));
  assert(near(limit_scale(0.6f, 0.15f), 0.25f));
  assert(near(limit_scale(0.0f, 0.15f), 1.0f));
}

static void test_limit_is_exactly_the_budget() {
  Drive white;  // white made from the colour LEDs: three of five channels
  white.level[RED] = white.level[GREEN] = white.level[BLUE] = 1.0f;
  const float scale = limit_scale(white.load(), 0.15f);
  assert(near(white.load() * scale, 0.15f));
}

static void test_constant_brightness_white_sums_to_one() {
  for (float warm = 0.0f; warm <= 1.0f; warm += 0.1f) {
    float cold_level, warm_level;
    split_white(warm, true, &cold_level, &warm_level);
    assert(near(cold_level + warm_level, 1.0f));
  }
}

static void test_additive_white_doubles_in_the_middle() {
  float cold_level, warm_level;
  split_white(0.5f, false, &cold_level, &warm_level);
  assert(near(cold_level, 1.0f) && near(warm_level, 1.0f));
}

static void test_white_clamps_outside_the_range() {
  float cold_level, warm_level;
  split_white(-1.0f, true, &cold_level, &warm_level);
  assert(near(cold_level, 1.0f) && near(warm_level, 0.0f));
  split_white(2.0f, true, &cold_level, &warm_level);
  assert(near(cold_level, 0.0f) && near(warm_level, 1.0f));
}

static void test_lerp_ends() {
  Drive a, b;
  a.level[RED] = 1.0f;
  b.level[WARM_WHITE] = 1.0f;
  assert(near(lerp(a, b, 0.0f).level[RED], 1.0f));
  assert(near(lerp(a, b, 1.0f).level[WARM_WHITE], 1.0f));
  assert(near(lerp(a, b, 0.5f).load(), 0.2f));
}

// The white LEDs for 6000 K and 3000 K, as components/ws2811_dual_ic/light.py works them out.
static const WhiteLeds LEDS{{0.9655f, 1.0183f, 0.9474f}, {1.3358f, 0.9050f, 0.3326f}};
static const float NEUTRAL[3] = {1.0f, 1.0f, 1.0f};

static void test_saturated_colour_uses_no_white() {
  const float red[3] = {1.0f, 0.0f, 0.0f};
  const Drive drive = mix_color(red, NEUTRAL, LEDS, 1.0f, true);
  assert(near(drive.level[RED], 1.0f));
  assert(near(drive.level[COLD_WHITE], 0.0f) && near(drive.level[WARM_WHITE], 0.0f));
}

static void test_led_white_uses_only_that_led() {
  const Drive warm = mix_color(LEDS.warm, NEUTRAL, LEDS, 1.0f, true);
  assert(near(warm.level[WARM_WHITE], 1.0f) && near(warm.level[COLD_WHITE], 0.0f));
  assert(warm.level[RED] < 0.001f && warm.level[GREEN] < 0.001f && warm.level[BLUE] < 0.001f);
  const Drive cold = mix_color(LEDS.cold, NEUTRAL, LEDS, 1.0f, true);
  assert(near(cold.level[COLD_WHITE], 1.0f) && near(cold.level[WARM_WHITE], 0.0f));
}

static void test_mid_white_blends_both_leds() {
  float mid[3];
  for (size_t i = 0; i < 3; i++)
    mid[i] = (LEDS.cold[i] + LEDS.warm[i]) / 2.0f;
  const Drive drive = mix_color(mid, NEUTRAL, LEDS, 1.0f, true);
  assert(near(drive.level[COLD_WHITE], drive.level[WARM_WHITE]));
  assert(drive.level[RED] < 0.001f && drive.level[GREEN] < 0.001f && drive.level[BLUE] < 0.001f);
}

static void test_mix_rebuilds_the_colour() {
  const float pastel[3] = {1.0f, 0.5f, 0.6f};
  const WhiteContent white = find_white(pastel, LEDS);
  assert(white.amount > 0.0f);
  for (size_t i = 0; i < 3; i++) {
    const float from_white = white.amount * (LEDS.cold[i] + (LEDS.warm[i] - LEDS.cold[i]) * white.warm_share);
    assert(from_white <= pastel[i] + 1e-5f);
  }
}

static void test_stronger_white_leds_are_driven_less() {
  const float pastel[3] = {1.0f, 0.7f, 0.6f};
  const Drive normal = mix_color(pastel, NEUTRAL, LEDS, 1.0f, true);
  const Drive strong = mix_color(pastel, NEUTRAL, LEDS, 3.0f, true);
  const float normal_white = normal.level[COLD_WHITE] + normal.level[WARM_WHITE];
  const float strong_white = strong.level[COLD_WHITE] + strong.level[WARM_WHITE];
  assert(strong_white < normal_white);
}

static void test_white_mix_can_be_turned_off() {
  const Drive drive = mix_color(LEDS.warm, NEUTRAL, LEDS, 1.0f, false);
  assert(near(drive.level[COLD_WHITE], 0.0f) && near(drive.level[WARM_WHITE], 0.0f));
  assert(near(drive.level[RED], 1.0f));
}

static void test_balance_keeps_a_pure_colour_at_full() {
  const float blue[3] = {0.0f, 0.0f, 1.0f};
  const float balance[3] = {1.0f, 0.9f, 0.35f};
  assert(near(mix_color(blue, balance, LEDS, 1.0f, true).level[BLUE], 1.0f));
}

int main() {
  test_dim_colour_stays_a_colour();
  test_black_stays_black();
  test_rounding_and_clamping();
  test_limit_only_acts_above_the_budget();
  test_limit_is_exactly_the_budget();
  test_constant_brightness_white_sums_to_one();
  test_additive_white_doubles_in_the_middle();
  test_white_clamps_outside_the_range();
  test_lerp_ends();
  test_saturated_colour_uses_no_white();
  test_led_white_uses_only_that_led();
  test_mid_white_blends_both_leds();
  test_mix_rebuilds_the_colour();
  test_stronger_white_leds_are_driven_less();
  test_white_mix_can_be_turned_off();
  test_balance_keeps_a_pure_colour_at_full();
  puts("pixel_math: all tests passed");
  return 0;
}
