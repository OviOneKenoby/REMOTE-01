# Hardware Facts and Verification

## GPIO1 external-power circuit - current owner evidence, 2026-10-06

Exact V2 Rev1.1 owner-installed circuit:100kOhm PTH5V->GPIO1 ADC node,100kOhm node->PTHG. USB disconnected: PTH5V0.00V,GPIO1/node0.00V. USB connected: PTH5Vapproximately4.95V,GPIO1/nodeapproximately2.42V. GPIO1=ADC1_CH0 now POWER-owned, separately calibrated12dB/12bit sensing; GPIO4 battery ADC unchanged. [Implementation and first physical comparison procedure](GPIO1_EXTERNAL_POWER_SENSING.md). Wiring/physical voltages owner-reported; firmware ADC/state/icon and provisional600/1800mV thresholds await owner validation. Detect external/USB presence only; charging UNKNOWN. No change to accepted external-independent SYSTEM_SLEEP. Earlier unknown-signal/divider-planning statements are historical, superseded for this installed circuit.

Source-based facts in this record use only the Waveshare V2 repository and its V2 factory example. Physical owner observations are recorded separately below. The V1 repository and any V1/V1.1 schematic-derived values are not used to configure this project.

Inspected Waveshare V2 source snapshot: commit `1c157e6e8e68b89fd4dc400f46bf1724cb64a57e` in `waveshareteam/ESP32-S3-Touch-LCD-3.49-V2`.

## Verified from Waveshare V2 Sources

| Fact | Verified value | Official V2 source |
| --- | --- | --- |
| Board/project identity | ESP32-S3-Touch-LCD-3.49 V2 | [V2 repository](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2), `schematic/ESP32-S3-Touch-LCD-3.49 V2.pdf` |
| MCU target | ESP32-S3 | `ESP-IDF/11_FactoryProgram/sdkconfig.defaults` (`CONFIG_IDF_TARGET="esp32s3"`); `dependencies.lock` target |
| LCD logical resolution | 172 x 640 pixels | `ESP-IDF/11_FactoryProgram/main/user_config.h` (`EXAMPLE_LCD_H_RES`, `EXAMPLE_LCD_V_RES`) |
| LCD bus/controller | AXS15231B over QSPI; 16-bit color | `main/main.cpp` (`use_qspi_interface`, `LCD_BIT_PER_PIXEL`); `main/user_config.h`; `main/idf_component.yml` |
| LCD QSPI pins | CS GPIO9; PCLK GPIO10; DATA0-3 GPIO11-14 | `main/user_config.h` (`EXAMPLE_PIN_NUM_LCD_*`) |
| Touch bus | Separate I2C bus; SDA GPIO17, SCL GPIO18; address 0x3B | `main/user_config.h` (`Touch_SDA_NUM`, `Touch_SCL_NUM`, `EXAMPLE_PIN_NUM_TOUCH_ADDR`); `components/i2c_bsp/i2c_bsp.c`; `main/main.cpp` touch read callback |
| Display rotation in V2 baseline | Factory LVGL coordinates are 172 x 640; raw touch X/Y is mapped in `example_lvgl_touch_cb` | `main/main.cpp`; `components/ui_bsp/generated/setup_scr_screen.c` |
| Backlight control | LEDC PWM on GPIO42, 8-bit duty, 50 kHz; TCA9554 backlight enable uses expander bit 1 | `components/lcd_bl_pwm_bsp/lcd_bl_pwm_bsp.c`; `main/user_config.h` (`EXAMPLE_PIN_NUM_BK_LIGHT`, `EXAMPLE_EXIO_PIN_BL_EN`); `main/main.cpp` |
| LCD reset | TCA9554 output bit 5 | `main/user_config.h` (`EXAMPLE_EXIO_PIN_LCD_RST`); `main/main.cpp` reset/expander initialization |
| Factory page behavior | Generated LVGL carousel with two 172-pixel elements; main/demo followed by brightness page and a 0-255 brightness slider | `components/ui_bsp/generated/setup_scr_screen.c`; `components/lvgl/src/extra/widgets/carousel/lv_carousel.c`; `components/user_app/user_app.cpp` |
| Flash size configuration | ESP-IDF factory config selects 16 MB flash | `ESP-IDF/11_FactoryProgram/sdkconfig.defaults` (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`) |
| PSRAM configuration | Enabled, octal mode, 80 MHz | `ESP-IDF/11_FactoryProgram/sdkconfig.defaults` (`CONFIG_SPIRAM`, `CONFIG_SPIRAM_MODE_OCT`, `CONFIG_SPIRAM_SPEED_80M`) |
| V2 factory framework/dependencies | ESP-IDF lock 5.5.4; LVGL 8.3.11; `esp_lcd_axs15231b` 1.0.1; `esp_io_expander_tca9554` 2.0.3; `esp_codec_dev` 1.3.6 | `ESP-IDF/11_FactoryProgram/dependencies.lock`; `components/lvgl/lvgl.h`; component manifests |

## Owner-Validated Physical Display Qualification

**DISPLAY QUALIFICATION: hardware PASS, 2026-10-03.** The owner observed these results on the physical 172x640 LCD using the temporary native-resolution qualification page. They are physical observations, not host-render predictions or inferred results.

| Montserrat size | Physical owner observation | Intended use from qualification |
| --- | --- | --- |
| 12 px | Readable | Reserve for metadata, minor status and low-priority information |
| 14 px | Clearly readable | Secondary UI |
| 16 px | Comfortably readable | Provisional minimum for normal primary UI text |
| 20 px | Very clear | Important labels/controls |
| 28 px | Very clear | Prominent values/headlines |

Montserrat 18 and 24 px are disabled in the current build and were not physically qualified; no readability result is inferred for them.

Physical owner detail observations:

- 1 px lines and borders are clearly distinguishable.
- Fine checker/stripe patterns render distinctly.
- Grayscale progression is distinguishable.
- Color samples render clearly.
- LVGL symbols at 12 px remain readable.
- LVGL symbols at 20 px are very clear.
- Dark background + white text + `#C2FF00` accent works well on the physical panel.

The owner also physically confirmed the 180-degree LCD orientation, touch mapping, carousel snapping, natural navigation (LEFT advances 0 -> 1 -> 2 with the next page entering from the RIGHT; RIGHT returns 2 -> 1 -> 0 with the previous page entering from the LEFT), and the previous clipping fixes.

The temporary qualification content was subsequently replaced by the first static REMOTE//01 HOME screen. The owner has now physically tested that HOME screen and confirmed **HOME UI hardware PASS**. Qualification and HOME results are physical owner observations; neither establishes protocol, adapter or end-to-end universal-control-path validation. This evidence correction changes documentation only.

## Not Yet Verified

- Physical board revision 1.1: reported by the project owner; the V2 source checkout does not independently identify the revision of the physical unit mounted in REMOTE//01.
- PSRAM capacity of 8 MB: reported by the project owner. The V2 factory config verifies octal PSRAM and 80 MHz but does not state the installed capacity in the inspected config. Do not infer capacity from the mode.
- The USB connector's physical top orientation is a project mounting fact reported by the owner, not a Waveshare source fact.
- Current HOME text, artwork placeholder and control/status values are static UI samples, not detected target state or implemented control behavior. The removed qualification page was also static readability test content.

## Excluded V1-Derived Values

The upstream V2 factory [`main/user_config.h`](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2/blob/1c157e6e8e68b89fd4dc400f46bf1724cb64a57e/ESP-IDF/11_FactoryProgram/main/user_config.h) contains a comment explicitly attributing GPIO0, GPIO8, GPIO4, and GPIO16 definitions to an ESP32-S3-Touch-LCD-3.49 V1.1 schematic. Those definitions were removed from this project and are not used or documented as V2 facts. The optional factory button/battery paths depending on them are not started in this baseline. No V1 pinout or example was used to choose hardware configuration.

## Rotation Implementation

The official AXS15231B driver exposes panel mirror operations (`esp_lcd_panel_mirror`). This project mirrors X and Y at the panel after initialization, then applies the inverse transform to the factory callback's already-oriented touch point: `x' = 171 - x`, `y' = 639 - y`. This keeps the LVGL geometry at 172 x 640 and rotates display, touch targets, gestures, and the existing brightness slider together. The API is from the AXS15231B component selected by the V2 factory manifest; its implementation is in Espressif's `esp_lcd_axs15231b` source, not a replacement display driver.
