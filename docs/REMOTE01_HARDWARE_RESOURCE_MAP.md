# REMOTE//01 hardware/resource map

2026-10-05. Current source and recorded owner evidence; no pin allocation or hardware activation. Target: owner-confirmed Waveshare ESP32-S3-Touch-LCD-3.49(B), V2/Rev1.1. Generic PlatformIO DevKit ID is not its wiring specification.

Evidence keys: **M** [main definitions/startup](../main); **P** [POWER hardware/runtime](../components/power_management) and [recorded exact-board schematic audit](STAGE_1_BATTERY_POWER_MANAGEMENT.md); **I** [I2C BSP](../components/i2c_bsp); **B** [PWM](../components/lcd_bl_pwm_bsp); **S** [dormant SD](../components/sdcard_bsp); **A** [dormant codec profiles/initialization](../components/codec_board); **C** [component exclusions](../CMakeLists.txt); **V** owner results in Stage1/2B documents. **SDK** installed pinned ESP-IDF5.5.3 `soc/esp32s3/{include/soc/spi_pins.h,adc_channel.h,soc_caps.h,uart_channel.h,register/soc/io_mux_reg.h}` and effective sdkconfig.remote01. MCU mux defaults establish reserved resources, not verified enclosure/header accessibility.

Only required status vocabulary is used below. OWNER_HARDWARE_VERIFIED means the recorded function passed; it is not an independent continuity/voltage test of every wire.

## Current target GPIO map

| Resource | GPIO/bus | Current owner/function | Evidence | Status | Conflict risk/notes |
| --- | --- | --- | --- | --- | --- |
| KEY reference |0 | No current product button owner |P; excluded button BSP |DORMANT | Schematic/factory KEY reference; not one of three verified custom buttons. |
| External/USB ADC |1 /ADC1 ch0 | POWER separate12dB calibrated binary sensing |2026-10-06 owner-installed100k/100k PTH5V/PTHG divider; node0.00V/approximately2.42V; SDK mapping |OWNER_HARDWARE_VERIFIED | Status covers wiring/physical voltage evidence only; firmware ADC/debounce/icon validation PENDING. Charging UNKNOWN; no sleep gate. [Details](GPIO1_EXTERNAL_POWER_SENSING.md). |
| Unallocated in project |2,3 | None established |M/P/resource search |UNKNOWN | ADC-capable MCU pads do not prove available header pins. |
| Battery ADC |4 /ADC1 ch3 | POWER calibrated divider measurement |P; SDK channel mapping |OWNER_HARDWARE_VERIFIED | Physical accuracy against multimeter still pending; not charging/SOC sense. |
| Unallocated in project |5 | None established |resource search |UNKNOWN | No enclosure wiring/free-pin evidence. |
| Candidate audio DIN |6 | No active audio owner |A/C |DORMANT | Candidate S3_LCD_3_49 input; no wiring acceptance. |
| Candidate audio MCLK |7 | No active audio owner |A/C |DORMANT | Not available for controls by default. |
| Shared expander IRQ |8 | POWER, falling-edge normal awake |P/V |OWNER_HARDWARE_VERIFIED | Touch/IMU/RTC inputs share interrupt; forced light-sleep level path separate. Wrong-board profiles also use8. |
| LCD CS |9 /SPI3 | main AXS15231B QSPI |M/V |OWNER_HARDWARE_VERIFIED | Korvo audio BCLK reference conflicts if enabled. |
| LCD clock |10 /SPI3 | main QSPI |M/V |OWNER_HARDWARE_VERIFIED | Korvo audio DIN conflicts; SDK SPI2 default CS is an alternative mux, not current assignment. |
| LCD data0 |11 /SPI3 | main QSPI |M/V |OWNER_HARDWARE_VERIFIED | Do not reuse SPI2 mux defaults. |
| LCD data1 |12 /SPI3 | main QSPI |M/V |OWNER_HARDWARE_VERIFIED | Same. |
| LCD data2 |13 /SPI3 | main QSPI |M/V |OWNER_HARDWARE_VERIFIED | Legacy codec SD GPIO13 power code is ESP32-only, not a live S3 conflict. |
| LCD data3 |14 /SPI3 | main QSPI |M/V |OWNER_HARDWARE_VERIFIED | Same bus ownership. |
| Candidate audio BCLK |15 | No active audio owner |A/C |DORMANT | Hardware unverified. |
| SYS_OUT reference |16 | No active custom-button input |P; excluded button BSP |DORMANT | Historical factory PWR-related signal; inaccessible PWR must not become product recovery. Korvo MCLK reference too. |
| Touch SDA |17 /I2C1 | main touch polling |M/I/V |OWNER_HARDWARE_VERIFIED | Korvo shared-I2C profile would collide. |
| Touch SCL |18 /I2C1 | main touch polling |M/I/V |OWNER_HARDWARE_VERIFIED | Korvo/C6 I2C references incompatible with live topology. |
| USB D− |19 | Native USB Serial/JTAG |SDK; UC runtime/V |RESERVED | MCU USB mux source; COM monitor validated. C6 audio MCLK reference is not this board. |
| USB D+ |20 | Native USB Serial/JTAG |SDK; UC runtime/V |RESERVED | C6 audio DIN reference incompatible. |
| LCD TE reference |21 | Not configured by main |M |VERIFIED_IN_SOURCE | Defined only; physical connection/availability not confirmed. C6 BCLK is alternative-board reference. |
| Invalid S3 GPIO numbers |22,23,24,25 | None |SDK valid-GPIO mask |RESERVED | SDK excludes these GPIO numbers. C6 profile22/23 must not be transplanted. |
| PSRAM CS default |26 /MSPI | SDK memory initialization |SDK/effective config |RESERVED | CS_IO26 configured; not header availability. |
| MSPI HD/WP/CS0 |27,28,29 /SPI0/1 | SDK memory resource reservation |SDK spi_pins.h |RESERVED | Default mux definitions; exact board/eFuse routing not remeasured. |
| PSRAM/memory clock |30 /MSPI | SDK memory initialization |SDK/effective config |RESERVED | CLK_IO30 configured. |
| MSPI MISO/MOSI |31,32 /SPI0/1 | Memory reservation |SDK |RESERVED | Do not treat as spare pads. |
| MSPI D4/D5/D6/D7/DQS |33,34,35,36,37 | Octal-memory reservation |SDK; octal80MHz config |RESERVED | Exact memory routing/package access must be verified before any proposed reuse. |
| SD D3/CD reference |38 | No active SD owner |M/S/C |DORMANT | Native1-bit driver never configures card detect; not proof of detect switch. |
| SD CMD |39 /SDMMC | Excluded BSP reference |M/S/C |DORMANT | SDK external JTAG MTCK alternative; neither SD nor external-JTAG wiring accepted. |
| SD D0 |40 /SDMMC | Excluded BSP reference |M/S/C |DORMANT | Alternative MTDO. |
| SD CLK |41 /SDMMC | Excluded BSP reference |M/S/C |DORMANT | Alternative MTDI. |
| Backlight PWM |42 /LEDC | PWM BSP; POWER gates separately |M/B/V |OWNER_HARDWARE_VERIFIED | Alternative MTMS would conflict.50kHz/8-bit source configuration. |
| Default UART0 TX/RX |43,44 | Primary console/default SDK mux |SDK/effective config |RESERVED | Default console configured; external wiring/use not independently verified. Do not allocate controls here. |
| Candidate audio DOUT |45 | No active audio owner |A/C |DORMANT | Both S3 audio profiles reference45 with different meanings. |
| Candidate audio WS |46 | No active audio owner |A/C |DORMANT | Physical function/electrical capabilities require proof. |
| System SDA |47 /I2C0 | I2C BSP, main expander, POWER IMU |M/I/P/V |OWNER_HARDWARE_VERIFIED | Dormant audio uses same bus; adopt existing handle/ownership. |
| System SCL |48 /I2C0 | Same |M/I/P/V |OWNER_HARDWARE_VERIFIED | Korvo amplifier PA48 conflicts if enabled. |
| Encoder/push/three buttons/header |UNKNOWN | No physical driver |repository search/owner requirement |UNKNOWN | No assigned pins, levels, connector exposure or verified encoder push. |

## Expander and peripheral resources

| Resource | GPIO/bus/peripheral | Current owner/function | Evidence | Status | Conflict risk/notes |
| --- | --- | --- | --- | --- | --- |
| TCA9554 |I2C0,7-bit0x20 | main creates; POWER uses existing handle |M/P |OWNER_HARDWARE_VERIFIED | One active expander; legacy codec probe is a separate dormant driver, not proof of a second expander. |
| EXIO0 |expander input | Touch interrupt reference |M/P |VERIFIED_IN_SOURCE | Shared IRQ; touch polling is active. No independent per-bit test claimed. |
| EXIO1 |expander output | POWER backlight enable |M/P/V |OWNER_HARDWARE_VERIFIED | Keep PWM brightness across idle. |
| EXIO2 |expander input | IMU INT1/WoM |M/P/V |OWNER_HARDWARE_VERIFIED | POWER owns QMI wake configuration. |
| EXIO3 |expander input | IMU INT2 reference |M/P |VERIFIED_IN_SOURCE | No separate INT2 feature/acceptance. |
| EXIO4 |expander input | RTC interrupt reference |M/P |VERIFIED_IN_SOURCE | No product clock/alarm service. |
| EXIO5 |expander output | LCD reset |M/V |OWNER_HARDWARE_VERIFIED | Not a spare bit. |
| EXIO6 |expander output | SYS_EN early unconditional HIGH |M/P/V |OWNER_HARDWARE_VERIFIED | USB cold start and battery handover verified; preserve direction/latch. |
| EXIO7 |expander audio NS_MODE reference | No active audio integration |M/C |DORMANT | No spare-bit or amplifier-control inference. |
| QMI8658 |I2C0,0x6B | POWER low-power21Hz/200mg WoM |P/V |OWNER_HARDWARE_VERIFIED | Dormant SensorLib/equipment general polling must not overwrite WoM. |
| PCF85063 |I2C0,0x51 | BSP handle; dormant equipment functions |I/C |CONFIGURED_NOT_HARDWARE_VERIFIED | Hardware RTC functionality not accepted. MCU RTC reset history is separate. |
| LCD DMA |SPI3,auto DMA channel | main22,016B internal buffer/220,160B PSRAM frame |M |VERIFIED_IN_SOURCE | Already resource-consuming; no audio/SD DMA budget granted. |
| I2C0/I2C1 |47/48 and17/18 | Existing live independent buses |M/I/V |OWNER_HARDWARE_VERIFIED | Extra `_i2c_init` bus creation is not safe wholesale reuse. |
| SPI2 |No current application allocation found | SDK mux alternatives |M/SDK |UNKNOWN | No permission to use default pins that overlap LCD/memory. |
| I2S channels/DMA |Dormant codec initialization | I2S master TX/RX references |A/C |DORMANT | SDK-default descriptor/buffer allocation would require live measurement. |
| SDMMC/FATFS |SDK support/library capability | No mounted card |S/C/effective config |VERIFIED_IN_SOURCE | Compiled SDK availability ≠ card integration. |
| External power/charge status |No trustworthy MCU sense established | POWER reports UNKNOWN |P |UNKNOWN | LED STAT/BAT voltage/serial link are not external-power evidence. |
| PWR |Enclosure inaccessible | USB required for cold boot |owner/P |OWNER_HARDWARE_VERIFIED | No daily/recovery action may require pressing it. |

## Explicit conflict/allocation matrix

| Category | Resources | Allocation conclusion |
| --- | --- | --- |
| VERIFIED USED |4,8–14,17/18,42,47/48; EXIO1/2/5/6 and source-defined shared inputs | Preserve current ownership; input additions cannot commandeer them. |
| RESERVED |19/20 USB,22–25 invalid,26–37 memory,43/44 default console | No new allocation. Exact memory/connector facts remain to verify. |
| DORMANT |0/16 factory buttons;6/7/15/45/46 audio;38–41 SD; EXIO7 | Dormant does not mean free. |
| POSSIBLY AVAILABLE |1/2/3/5 have no current project assignment found | **Not permission**: connectivity, pulls, mux/strapping and enclosure access UNKNOWN. |
| UNKNOWN / MUST VERIFY |21 TE connectivity; expansion/header; encoder push; custom controls; card detect/power; codec/amp population | OWNER HARDWARE INPUT REQUIRED. |
| CONFLICT |Conditional alternate-profile collisions: Korvo9/10/8/17/18/48; C6 8/18/19/20/21 and invalid-S3 22/23; external JTAG42 versus PWM | No active conflict diagnosed. Do not enable alternative profiles on this target. |

Encoder needs verified A/B signals and pulls/levels; push only if present. Three buttons need verified independent signals or an explicitly verified shared interface, active levels/pulls and wake capability. SD needs confirmed39/40/41 topology/pulls/card supply; GPIO38 is not verified detect. Audio needs confirmed codec/control/I2S/output/power/mute paths and bus ownership. None of these allocations is currently supported by sufficient assembled-hardware evidence. Owner must supply exact enclosure wiring/schematic/photos/continuity results and board revision; no new pins chosen.

2026-10-06: prior GPIO1 unallocated/UNKNOWN inventory superseded only by explicit owner GPIO1 divider wiring/voltage evidence. GPIO2/3 remain unverified. No other resource assignment changed.
