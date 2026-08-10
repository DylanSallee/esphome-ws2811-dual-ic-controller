# esphome-ws2811-dual-ic-controller

An ESPHome .yaml configuration template for controlling WS2811 Dual-IC FCOB RGBCCT LED strips via ESP32 devices.

## Description

This is a ESPHome .yaml configuration template for controlling a WS2811 Dual-IC FCOB RGBCCT LED strip via an ESP32 device.

By default, GPIO pins are defined for a SMLIGHT A1-SLWF-03 device. This template should be easily adaptable to other ESP32-based controllers, especially those designed to run WLED.

While WLED is compatible with these strips as of 16.0.1, the integration with Home Assistant does not handle RGBCCT strips correctly at this time (2026-08-08). See [issue #146123](https://github.com/home-assistant/core/issues/146123), [issue #162142](https://github.com/home-assistant/core/issues/162142) and [issue #4641](https://github.com/wled/WLED/issues/4641), along with [this home assistant community post](https://community.home-assistant.io/t/wled-rgb-cct-led-strips-dont-work-correctly-in-home-assistant/963514/9).

## Features

* **WS2811 Dual-IC RGBCCT Support**: Native control for WS2811 Dual-IC FCOB RGBCCT LED strips using an interleaved IC layout (even ICs for RGB, odd ICs for CCT).
* **Dual Virtual Light Entities**: Exposes two lights in Home Assistant: one with full RGBCCT color/temperature control and one for RGB effects.
* **Native ESPHome Addressable Effects**: Includes native ESPHome addressable effects (rainbow, scan, twinkle, fireworks, flicker, strobe, pulse) and optional WLED integration.
* **Configurable Maximum Brightness**: Configurable maximum brightness limit slider in Home Assistant to protect LED strips from overheating.
* **Flexible Substitutions**: Easily configurable YAML substitutions for LED counts, color temperatures, GPIO pin maps, device names, and network options.
* **Hardware Integration (SMLIGHT A1-SLWF-03 / ESP32)**:
  * **Status LEDs**: Automated network connection indication via green and blue status LEDs.
  * **Touch Button**: Capacitive TTP223 touch button binary sensor.
  * **IR Receiver**: Remote receiver integration set up for raw IR command logging and mapping.
  * **I2S Audio Microphone**: Ambient sound level monitoring with average and peak sound level sensors.
* **System Diagnostics & Management**: Real-time diagnostic sensors for internal ESP32 temperature, uptime, free memory, loop time, free heap, reset reason, and device controls (restart, safe mode, shutdown, factory reset).

## Getting Started

### Configuration

The configuration template uses ESPHome substitutions for easy customization. You can adjust these values at the top of `ws2811-dual-ic-controller.yaml` to match your specific setup and pin mapping. Additional information is included in the configuration file.

**Important:** If you adjust the number of leds (`ws2811_strip_num_leds`), you must also update the `segments` array under `light` -> `partition` -> `partition_effects`.

| Substitution Option | Description | Default Value |
| --- | --- | --- |
| `ws2811_strip_num_ics` | Total number of ICs on the LED strip (should be double the physical segment count) | `28` |
| `ws2811_cw_color_temp` | Color temperature of the cold white LEDs in Kelvin | `6000 K` |
| `ws2811_ww_color_temp` | Color temperature of the warm white LEDs in Kelvin | `3000 K` |
| `ws2811_default_max_brightness` | Default maximum brightness percentage limit to prevent overheating | `25` |
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
| `log_level` | Logging verbosity level (DEBUG, INFO, WARNING, ERROR) | `DEBUG` |
| `static_ip` | Static IP address for manual IP configuration (optional, commented out) | `192.168.xxx.xxx` |
| `gateway` | Network gateway IP address (optional, commented out) | `192.168.xxx.xxx` |
| `subnet` | Network subnet mask (optional, commented out) | `255.255.255.xxx` |

To save resources, I recommend disabling (commenting out) the IR receiver and microphone components if you do not intend to use them in your project.

This project can be compiled using the `arduino` or `esp-idf` framework. If using the ESPHome WLED effect integration, the project must be compiled using the `arduino` framework.

### Installing

* Install using ESPHome's excellent [getting started guide](https://esphome.io/install/).

## Help

If you need any assistance, feel free to open an issue.

## Authors

- [Dylan Sallee](https://www.dylansallee.com)

## License

This project is licensed under the GNU General Public License - see the LICENSE.md file for details.

## Useful Links

* [SMLIGHT A1-SLWF-03 Manual & FAQ](https://smlight.tech/manual/slwf-03/faq-and-lifehacks/)
* [SMLIGHT A1-SLWF-03 Voice Assistant Configuration](https://github.com/smlight-dev/A1-SLWF-03/blob/main/a1-slwf-03-voice-assistant.yaml)
* [ESPHome Remote Receiver Component Documentation](https://esphome.io/components/remote_receiver/)
* [ESPHome RMT IR Devices Setup Guide](https://esphome.io/guides/setting_up_rmt_devices/#setting-up-ir-devices)
* [ESPHome I2S Audio Microphone Component Documentation](https://esphome.io/components/microphone/i2s_audio/)

## AI Disclaimer
This template was created with the assistance of generative AI programming tools. However, it was primarially written, edited, extensively reviewed, and tested by a professional software developer.