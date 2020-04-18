![Tasmota logo](/tools/logo/TASMOTA_FullLogo_Vector.svg)
![WS2812FX library](https://raw.githubusercontent.com/kitesurfer1404/WS2812FX/master/WS2812FX_logo.png)

WS2812FX Library by @kitesurfer1404


## Description
Fork of **Tasmota 8.2.0 (master)**.
Support of **WS2812FX** library with more than 50 additional light effects.
To eliminate influence to main Tasmota processes (such as Wi-Fi and MQTT processing), the fork was compiled with **DMA support** (env:tasmota-ws2812fx-dma), this means that DI (data input) of led strip **SHOULD** be soldered on Rx (GPIO3) pin of ESP chip. If you use Rx for serial communication, you cannot apply this; pin will be OUTPUT as a compilation result. 

However, it doesn't affect programming through USB-TTL Rx-Tx adapter; in programming mode (D0 tied to GND during power-up), Rx keeps input mode. The funny is when LED strip connected during programming, addressable LEDs getting firmware binary stream and stay different colours, mostly white. 

Also because colors update of long strips (longest mine has 231 LEDs) takes too much time, and every update operation in library ws2812fx blocks interrupts (in case software send bit stream, not DMA ), this can bring system in unstable or unresponsive mess. That is why DMA is a best choice for real-time systems, it works smoothly with no affection of other system in Tasmota (I hope, no guarantee).

## Commands
This fork use most of standard embedded commands of Tasmota, although use some of them own way.

#### Scheme
Command  | Payload  | Description
-------  | -------- | -----------
Scheme   | 0..12    | Tasmota schemes
Scheme   | 13..68   | WS2812FX schemes (0..55) + offset 13

All original light schemes (0 .. 12) still work as described in Tasmota documentation.
Hereinafter will be only (13 .. 68) schemes dependent explanations.

#### ws2812fx dependent
Library animation depends of many parameters/payloads and to achieve good results have to try every scheme on various parameters, such as speed, colors, width, fade, gamma correction etc. Ws2812fx library has 3 colors entrance, these colors used in effects rendering. For not to create new settings and variables, been used standard Tasmota color settings, described in documentation about Scheme 5 (Clock).

- Some commands/parameters are standard:

Command  | Payload | Description
-------  | -------- | -----------
Pixels   | _Tasmota doc_ | LEDs count
Dimmer   | _Tasmota doc_ | Brightness
LedTable | _Tasmota doc_ | Gamma correction
Speed    | _Tasmota doc_ | Animation speed
Width    | _Tasmota doc_ | Used in animation, accepts 0..3
Color3   | _Tasmota doc_ | Used in animation
Color4   | _Tasmota doc_ | Used in animation
Color5   | _Tasmota doc_ | Used in animation

- Some commands deviate behavior

Command  | Payload | Description
-------  | -------- | -----------
Rotation | any int, use bit[0]  | Animation direction

- Moreover, 2 commands added, due to inability use standard Fade and to show additional state message

Command  | Payload | Description
-------  | -------- | -----------
XFade    | 0..7  | used in animation
XState   |       | report JSON state of every of this

Result of XState command:
```
{"Time":"2020-04-18T09:53:33",
"POWER2":"ON",
"Dimmer":48,
"Color":"7A6500",
"HSBColor":"50,100,48",
"Channel":[48,40,0],
"Scheme":32,
"Width":1,
"Fade":"OFF",
"Speed":33,
"LedTable":"ON",
"Pixels":231,
"Color3":"ff00ff",
"Color4":"00ff00",
"Color5":"ff0000",
"XFade":1,
"Name":"Twinkle"}
```
- Library ws2812fx need Speed in values 2..65535, so inside there is a converter who does job about conversion of standard Tasmota Speed command:

![Speed conversion image](/tools/logo/speed-conversion.jpg)

Dont't want compile yourself ? Here is precompiled version

https://github.com/a-roz/Tasmota/raw/ws2812fx/firmware/firmware.bin

I have tried this using OTA update. 
1. download and install "tasmota-minimal.bin"
2. download and install this "firmware.bin"

That is my template modified, took from @blakadder https://templates.blakadder.com/QS-QIFI-S03.html 
![Template](/tools/logo/template.jpeg)

Good luck

-----------------------------------------------------------------------------
Tasmota continue
-----------------------------------------------------------------------------

Alternative firmware for [ESP8266](https://en.wikipedia.org/wiki/ESP8266) based devices with **easy configuration using webUI, OTA updates, automation using timers or rules, expandability and entirely local control over MQTT, HTTP, Serial or KNX**.
_Written for Arduino IDE and PlatformIO._

[![GitHub version](https://img.shields.io/github/release/arendst/Tasmota.svg)](https://github.com/arendst/Tasmota/releases/latest)
[![GitHub download](https://img.shields.io/github/downloads/arendst/Tasmota/total.svg)](https://github.com/arendst/Tasmota/releases/latest)
[![License](https://img.shields.io/github/license/arendst/Tasmota.svg)](LICENSE.txt)
[![Chat](https://img.shields.io/discord/479389167382691863.svg)](https://discord.gg/Ks2Kzd4)

If you like **Tasmota**, give it a star, or fork it and contribute!

[![GitHub stars](https://img.shields.io/github/stars/arendst/Tasmota.svg?style=social&label=Star)](https://github.com/arendst/Tasmota/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/arendst/Tasmota.svg?style=social&label=Fork)](https://github.com/arendst/Tasmota/network)
[![donate](https://img.shields.io/badge/donate-PayPal-blue.svg)](https://paypal.me/tasmota)

See [RELEASENOTES.md](RELEASENOTES.md) for release information.

In addition to the [release webpage](https://github.com/arendst/Tasmota/releases/latest) the binaries can also be downloaded from http://thehackbox.org/tasmota/release/

## Development

[![Dev Version](https://img.shields.io/badge/development%20version-v8.2.x.x-blue.svg)](https://github.com/arendst/Tasmota)
[![Download Dev](https://img.shields.io/badge/download-development-yellow.svg)](http://thehackbox.org/tasmota/)
[![Build Status](https://img.shields.io/travis/arendst/Tasmota.svg)](https://travis-ci.org/arendst/Tasmota)

See [tasmota/CHANGELOG.md](tasmota/CHANGELOG.md) for detailed change information.

Unless your Tasmota powered device exhibits a problem or you need to make use of a feature that is not available in the Tasmota version currently installed on your device, leave your device alone - it works so don't make unnecessary changes! If the release version (i.e., the master branch) exhibits unexpected behaviour for your device and configuration, you should upgrade to the latest development version instead to see if your problem is resolved as some bugs in previous releases or development builds may already have been resolved.

The Tasmota development codebase is checked every 1-2 hours for changes. If new commits have been merged and they compile successfuly, new binary files for every variant will be posted at http://thehackbox.org/tasmota/ (this web address can be used for OTA updates too). The last compiled commit number is also indicated on the same page. It is important to note that these binaries are based on the current development codebase. These commits are tested as much as is possible and are typically quite stable. However, it is infeasible to test on the hundreds of different types of devices with all the available configuration options permitted.

Note that there is a chance, as with any upgrade, that the device may not function as expected. You must always account for the possibility that you may need to flash the device via the serial programming interface if the OTA upgrade fails. Even with the master release, you should always attempt to test the device or a similar prototype before upgrading a device which is in production or is hard to reach. And, as always, make a backup of the device configuration before beginning any firmware update.

## Disclaimer

:warning: **DANGER OF ELECTROCUTION** :warning:

If your device connects to mains electricity (AC power) there is danger of electrocution if not installed properly. If you don't know how to install it, please call an electrician (***Beware:*** certain countries prohibit installation without a licensed electrician present). Remember: _**SAFETY FIRST**_. It is not worth the risk to yourself, your family and your home if you don't know exactly what you are doing. Never tinker or try to flash a device using the serial programming interface while it is connected to MAINS ELECTRICITY (AC power).

We don't take any responsibility nor liability for using this software nor for the installation or any tips, advice, videos, etc. given by any member of this site or any related site.

## Note

Please do not ask to add new devices unless it requires additional code for new features. If the device is not listed as a module, try using [Templates](https://tasmota.github.io/docs/Templates) first. If it is not listed in the [Tasmota Device Templates Repository](http://blakadder.github.io/templates) create your own [Template](https://tasmota.github.io/docs/Templates#creating-your-template).

## Quick Install
Download one of the released binaries from https://github.com/arendst/Tasmota/releases and flash it to your hardware [using our installation guide](https://tasmota.github.io/docs/Getting-Started).

## Important User Compilation Information
If you want to compile Tasmota yourself keep in mind the following:

- Only Flash Mode **DOUT** is supported. Do not use Flash Mode DIO / QIO / QOUT as it might seem to brick your device.
- Tasmota uses a 1M linker script WITHOUT spiffs **1M (no SPIFFS)** for optimal code space.
- To make compile time changes to Tasmota use the `user_config_override.h` file. It assures keeping your custom settings when you download and compile a new version. You have to make a copy from the provided `user_config_override_sample.h` file and add your setting overrides. To enable the override you have to set a compile define as documented in the `user_config_override_sample.h` file.

## Configuration Information

Please refer to the installation and configuration articles in our [documentation](https://tasmota.github.io/docs).

## Migration Information

See [wiki migration path](https://tasmota.github.io/docs/Upgrading#migration-path) for instructions how to migrate to a major version. Pay attention to the following version breaks due to dynamic settings updates:

1. Migrate to **Sonoff-Tasmota 3.9.x**
2. Migrate to **Sonoff-Tasmota 4.x**
3. Migrate to **Sonoff-Tasmota 5.14**
4. Migrate to **Sonoff-Tasmota 6.x**
5. Migrate to **Tasmota 7.x**

--- Major change in parameter storage layout ---

6. Migrate to **Tasmota 8.1**
7. Migrate to **Tasmota 8.x**

## Support Information

<img src="https://user-images.githubusercontent.com/5904370/68332933-e6e5a600-00d7-11ea-885d-50395f7239a1.png" width=150 align="right" />

For a database of supported devices see [Tasmota Device Templates Repository](https://templates.blakadder.com)

If you're looking for support on **Tasmota** there are some options available:

### Documentation

* [Documentation Site](https://tasmota.github.io/docs): For information on how to flash Tasmota, configure, use and expand it
* [FAQ and Troubleshooting](https://tasmota.github.io/docs/FAQ/): For information on common problems and solutions.
* [Commands Information](https://tasmota.github.io/docs/Commands): For information on all the commands supported by Tasmota.

### Support's Community

* [Tasmota Forum](https://groups.google.com/d/forum/sonoffusers): For usage and discussions.
* [Tasmota Support Chat](https://discord.gg/Ks2Kzd4): For support, troubleshooting and general questions. You have better chances to get fast answers from members of the Tasmota Community.
* [Search in Issues](https://github.com/arendst/Tasmota/issues): You might find an answer to your question by searching current or closed issues.

### Developers' Community

* [Bug Report](https://github.com/arendst/Tasmota/issues/new?template=Bug_report.md): For reporting Bugs of Tasmota Software.
* [Feature Request](https://github.com/arendst/Tasmota/issues/new?template=Feature_request.md): For requesting features/functions to Tasmota Software.
* [Troubleshooting](https://github.com/arendst/Tasmota/issues/new?template=Custom.md): As a last resort, you can open new *Troubleshooting* issue on GitHub if the solution could not be found using the other channels. Just remember: the more info you provide the more chances you'll have to get an accurate answer.
* [Issue a question](https://github.com/arendst/Tasmota/issues/new/choose): As a last resort, you can open a new *Question* issue on GitHub if the answer could not be found using the other channels. Just remember: the more info you provide the more chances you'll have to get an accurate answer.

## Contribute

You can contribute to Tasmota by
- providing Pull Requests (Features, Proof of Concepts, Language files or Fixes)
- testing new released features and report issues
- donating to acquire hardware for testing and implementing or out of gratitude
- contributing missing [documentation](https://tasmota.github.io/docs) for features and devices

[![donate](https://img.shields.io/badge/donate-PayPal-blue.svg)](https://paypal.me/tasmota)

## Credits

People helping to keep the show on the road:
- David Lang providing initial issue resolution and code optimizations
- Heiko Krupp for his IRSend, HTU21, SI70xx and Wemo/Hue emulation drivers
- Wiktor Schmidt for Travis CI implementation
- Thom Dietrich for PlatformIO optimizations
- Marinus van den Broek for his EspEasy groundwork
- Pete Ba for more user friendly energy monitor calibration
- Lobradov providing compile optimization tips
- Flexiti for his initial timer implementation
- reloxx13 for his [TasmoAdmin](https://github.com/reloxx13/TasmoAdmin) management tool
- Joachim Banzhaf for his TSL2561 library and driver
- Gijs Noorlander for his MHZ19, SenseAir and updated PubSubClient drivers
- Erik Montnemery for his HomeAssistant Discovery concept and many code tuning tips
- Federico Leoni for continued HomeAssistant Discovery support
- Aidan Mountford for his HSB support
- Daniel Ztolnai for his Serial Bridge implementation
- Gerhard Mutz for multiple sensor & display drivers, Sunrise/Sunset, and scripting
- Nuno Ferreira for his HC-SR04 driver
- Adrian Scillato for his (security)fixes and implementing and maintaining KNX
- Gennaro Tortone for implementing and maintaining Eastron drivers
- Raymond Mouthaan for managing Wemos Wiki information
- Norbert Richter for his [decode-config.py](https://github.com/tasmota/decode-config) tool
- Andre Thomas for providing [thehackbox](http://thehackbox.org/tasmota/) OTA support and daily development builds
- Joel Stein, digiblur and Shantur Rathore for their Tuya research and driver
- Frogmore42 for providing many issue answers
- Jason2866 for platformio support and providing many issue answers
- Blakadder for managing the new document site and providing template management
- Stephan Hadinger for refactoring light driver, enhancing HueEmulation and Zigbee support
- tmo for designing the official Tasmota logo
- Stefan Bode for his Shutter and Deep sleep drivers
- Jacek Ziółkowski for his [TDM](https://github.com/jziolkowski/tdm) management tool and [Tasmotizer](https://github.com/tasmota/tasmotizer) flashing tool
- Christian Staars for NRF24L01 and HM-10 Bluetooth sensor support
- Pail Diem for UDP Group communication support
- Many more providing Tips, Wips, Pocs, PRs and Donations

## License

This program is licensed under GPL-3.0
