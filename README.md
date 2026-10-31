# AI Disclaimer
The .yaml template was created with the use of generative AI programming assistants, but has been extensively reviewed, edited, and tested by a professional software developer.

The custom light components/tests were created **primarially** with the use of generative AI programming assistants, but have been reviewed and tested by a professional software developer.

# esphome-ws2811-dual-ic-controller

An ESPHome .yaml configuration template for controlling WS2811 Dual-IC FCOB RGBCCT LED strips via ESP32 devices.

## Description

This is a ESPHome .yaml configuration template for controlling a WS2811 Dual-IC FCOB RGBCCT LED strip via an ESP32 device.

By default, GPIO pins are defined for a SMLIGHT A1-SLWF-03 device. This template should be easily adaptable to other ESP32-based controllers, especially those designed to run WLED.

## Why not use WLED?

WLED is compatible with these strips as of 16.0.1. Home Assistant's WLED integration did not handle RGBCCT strips correctly before its 2026.10 releases, which include some fixes for [issue #146123](https://github.com/home-assistant/core/issues/146123) and [issue #162142](https://github.com/home-assistant/core/issues/162142). However, several open issues remain. For example, since those fixes, Home Assistant moves so much of a color into the white channels that saturated colors set with `xy_color`, `hs_color` or `rgb_color` look whitish on these strips. See [issue #189323](https://github.com/home-assistant/core/issues/189323). See also [WLED issue #4641](https://github.com/wled/WLED/issues/4641), which is still open, and [this Home Assistant community post](https://community.home-assistant.io/t/wled-rgb-cct-led-strips-dont-work-correctly-in-home-assistant/963514/9).

## Why a custom component?

ESPHome still has no five channel strip support, as of 2026.10.1. A pull request that added it ([#12414](https://github.com/esphome/esphome/pull/12414)) was closed unmerged in May 2026, and `channel_colors` only accepts RGB or RGBW. So this repository includes `ws2811_dual_ic`, a small light platform that sits on top of ESPHome's normal strip driver.

Each physical segment of the strip has two WS2811 ICs. The component treats that pair as one pixel, so ESPHome effects run per segment and no segment list is needed.

## Features

* **One RGBCCT Light**: Colors, white and effects all live on a single light entity in Home Assistant.
* **Colors Close to Philips Hue**: Colors are decoded the way Home Assistant encodes them. The white LEDs supply any white a color contains, as they do in a Hue bulb, so near-white scenes look right and saturated colors stay pure. The light reports RGB and color temperature modes instead of RGBWW, which avoids the Home Assistant conversion behind [issue #189323](https://github.com/home-assistant/core/issues/189323).
* **Live Calibration**: Sliders in Home Assistant balance the LEDs against a reference light, with no reflash.
* **Hue Preserved When Dim**: Gamma and brightness are applied once, in floating point, as each frame is sent. A dim color stays that color instead of drifting to white.
* **Flat White**: Brightness stays the same across the color temperature range. Set `constant_brightness: false` to let the middle of the range run at twice the output.
* **Crossfades**: Changes between a color and white crossfade instead of dipping through black.
* **Power Limit**: A slider in Home Assistant caps the strip's total power, so a pure color or white can run far brighter than a mixed color at the same heat. The value survives a reboot.
* **Native ESPHome Addressable Effects**: Includes rainbow, scan, twinkle, fireworks, flicker, strobe and pulse, and optional WLED integration. Effects use the color LEDs only.
* **No Idle Traffic**: The strip is only sent data when something changes.
* **Restores After Power Loss**: The light returns to its last state after a reboot.
* **Flexible Substitutions**: Easily configurable YAML substitutions for LED counts, color temperatures, GPIO pin maps, device names, and network options.
* **Hardware Integration (SMLIGHT A1-SLWF-03 / ESP32)**:
  * **Status LEDs**: Automated network connection indication via green and blue status LEDs.
  * **Touch Button**: Capacitive TTP223 touch button binary sensor.
  * **IR Receiver**: Remote receiver integration set up for raw IR command logging and mapping.
  * **I2S Audio Microphone**: Ambient sound level monitoring with average and peak sound level sensors.
* **System Diagnostics & Management**: Real-time diagnostic sensors for strip load, internal ESP32 temperature, uptime, free memory, loop time, free heap, reset reason, and device controls (restart, safe mode, shutdown, factory reset).

## Getting Started

### Requirements

ESPHome 2026.10.0 or later is required. Keep the `components` folder next to the YAML file. If you only copy the YAML file, change `external_components` to the GitHub source shown in its comment.

### Configuration

The configuration template uses ESPHome substitutions for easy customization. You can adjust these values at the top of `ws2811-dual-ic-controller.yaml` to match your specific setup and pin mapping. Additional information is included in the configuration file.

| Substitution Option | Description | Default Value |
| --- | --- | --- |
| `ws2811_strip_num_ics` | Total number of ICs on the LED strip (double the physical segment count) | `28` |
| `ws2811_wire_order` | Byte order of one segment. See below | `RGBWCX` |
| `ws2811_cw_color_temp` | Color temperature of the cold white LEDs in Kelvin | `6000 K` |
| `ws2811_ww_color_temp` | Color temperature of the warm white LEDs in Kelvin | `3000 K` |
| `ws2811_default_power_limit` | Default Power Limit percentage | `15` |
| `ws2811_color_gamma` | Decodes colors for the LEDs. 2.2 matches how Home Assistant encodes them | `2.2` |
| `ws2811_brightness_gamma` | Curve of the brightness slider. Lower it to lift the bottom of the slider | `2.2` |
| `ws2811_default_red_balance` | Starting value of the Red Balance slider | `100` |
| `ws2811_default_green_balance` | Starting value of the Green Balance slider | `60` |
| `ws2811_default_blue_balance` | Starting value of the Blue Balance slider | `35` |
| `ws2811_default_white_strength` | Starting value of the White Strength slider | `100` |
| `controller_device_friendly_name` | Frontend friendly name of the WS2811 strip light entity | `WS2811 Dual IC RGBCCT LED Strip Controller` |
| `controller_device_id` | Backend entity ID of the custom sub-device grouping | `ws2811_controller` |
| `controller_device_led_data_pin` | GPIO pin connected to the LED strip data line | `GPIO16` |
| `controller_device_status_blue_pin` | GPIO pin connected to the blue status LED | `GPIO22` |
| `controller_device_status_green_pin` | GPIO pin connected to the green status LED | `GPIO23` |
| `controller_device_touch_button_pin` | GPIO pin connected to the TTP223 touch button | `GPIO26` |
| `controller_device_ir_receiver_pin` | GPIO pin connected to the IR receiver module | `GPIO4` |
| `controller_device_microphone_i2s_bclk_pin` | GPIO pin connected to the microphone I2S BCLK pin | `GPIO14` |
| `controller_device_microphone_i2s_lrclk_pin` | GPIO pin connected to the microphone I2S LRCLK pin | `GPIO15` |
| `controller_device_microphone_i2s_din_pin` | GPIO pin connected to the microphone I2S DIN pin | `GPIO32` |
| `node_friendly_name` | Frontend friendly name of the ESPHome node | `SMLIGHT A1-SLWF-03` |
| `node_name` | Backend hostname / name of the ESPHome node | `smlight-a1-slwf-03` |
| `log_level` | Logging verbosity level (DEBUG, INFO, WARNING, ERROR) | `INFO` |
| `static_ip` | Static IP address for manual IP configuration (optional, commented out) | `192.168.xxx.xxx` |
| `gateway` | Network gateway IP address (optional, commented out) | `192.168.xxx.xxx` |
| `subnet` | Network subnet mask (optional, commented out) | `255.255.255.xxx` |

To save resources, I recommend disabling (commenting out) the IR receiver and microphone components if you do not intend to use them in your project.

This project can be compiled using the `arduino` or `esp-idf` framework. If using the ESPHome WLED effect integration, the project must be compiled using the `arduino` framework.

### Wire order

`ws2811_wire_order` lists the six bytes of one segment in the order the ICs expect them. `R`, `G` and `B` are the color LEDs, `C` is cold white, `W` is warm white, and `X` is the unused byte. The first three letters belong to the segment's first IC.

The default, `RGBWCX`, was worked out on a BTF-LIGHTING strip. If colors or whites look swapped, set one color at a time and adjust the order. For example, `WCXRGB` means the white IC comes first. Leave `channel_colors` on the strip driver set to `RGB`.

### Power Limit

The Power Limit caps the strip's average output, where 100% means all five channels fully on. A single color or white channel at full is 20%, and white made from the color LEDs is 60%.

The brightness slider keeps its full range under the limit. When a color would exceed it, the whole color is scaled down at full brightness, so 100% on the slider means the brightest that color can be within the limit.

The limit assumes every channel draws the same current. Treat it as a guide and check the strip's temperature, especially on the spool.

### Tuning the look

The strip driver is 8-bit, so very dim settings have few steps. Below roughly 10% on the brightness slider, the strip may hold at its lowest level. This is a limit of the WS2811, and a lower `ws2811_brightness_gamma` shifts more of the slider above it.

### Matching Philips Hue

The defaults were tuned by eye on one strip, and LED output varies between strips. Set these next to a Hue bulb, with the room otherwise dark. Each slider applies immediately.

The calibration sliders and the **Use White LEDs for Colors** switch are disabled by default. Enable them first on the device's page in Home Assistant, under its disabled entities.

1. Turn off **Use White LEDs for Colors**. Set the strip and the bulb to the same white with `rgb_color: [255, 255, 255]`.
2. Lower the **Red**, **Green** and **Blue Balance** sliders until the strip stops looking tinted. Keep the strongest of the three at 100%.
3. Turn **Use White LEDs for Colors** back on. Compare a pale color on both, such as `rgb_color: [255, 180, 200]`.
4. Raise **White Strength** if the strip looks too pale, and lower it if the strip looks too saturated.

Near-white colors use the white LEDs almost alone, so they depend mostly on your strip's `ws2811_cw_color_temp` and `ws2811_ww_color_temp` being accurate. Colors warmer than the warm LEDs or cooler than the cold LEDs add a small tint from the color LEDs.

### Testing

`tests/run.sh` runs the color math tests, then runs the real component on your computer against a fake strip and checks every byte it would have sent. This needs no hardware. Set `ESPHOME` to choose which ESPHome to use.

### Installing

1. Install ESPHome using its [getting started guide](https://esphome.io/install/).
2. Clone this repository, or copy the YAML file as described under [Requirements](#requirements).
3. Copy `secrets-example.yaml` to `secrets.yaml` in the same folder and fill in every value. Generate the API encryption key with `openssl rand -base64 32`. Never commit `secrets.yaml`.
4. Adjust the substitutions at the top of `ws2811-dual-ic-controller.yaml` for your strip and board.
5. Flash the device. The first install usually needs a USB connection, and later installs can go over the air.

```sh
esphome run ws2811-dual-ic-controller.yaml
```

## Help

If you need any assistance, feel free to open an issue.

## Authors

- [Dylan Sallee](https://www.dylansallee.com)

## License

This project is licensed under the GNU General Public License - see the LICENSE file for details.

## Useful Links

* [SMLIGHT A1-SLWF-03 Manual & FAQ](https://smlight.tech/manual/slwf-03/faq-and-lifehacks/)
* [SMLIGHT A1-SLWF-03 Voice Assistant Configuration](https://github.com/smlight-dev/A1-SLWF-03/blob/main/a1-slwf-03-voice-assistant.yaml)
* [ESPHome Remote Receiver Component Documentation](https://esphome.io/components/remote_receiver/)
* [ESPHome RMT IR Devices Setup Guide](https://esphome.io/guides/setting_up_rmt_devices/#setting-up-ir-devices)
* [ESPHome I2S Audio Microphone Component Documentation](https://esphome.io/components/microphone/i2s_audio/)
* [WLED RGBCCT Strip Discussion](https://github.com/wled/WLED/issues/4641)