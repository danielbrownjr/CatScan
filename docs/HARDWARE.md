# Hardware decisions and verification

## CONFIRMED

Documentation-confirmed, not bench-tested:

- Seeed's standard XIAO ESP32-S3 pin map assigns D4 to GPIO5/SDA and D5 to GPIO6/SCL.
  These avoid ESP32-S3 strapping pins GPIO0/3/45/46 and USB GPIO19/20.
- TI ADS1115 provides six distinct PGA ranges: ±6.144, ±4.096, ±2.048, ±1.024,
  ±0.512 and ±0.256 V. Results are signed 16-bit two's complement.
- Voltage = signed count × configured positive full-scale / 32768.
  Single-ended inputs use the positive half of the range; tiny negative codes
  near ground are preserved, not clamped.
- Conversion rates are 8/16/32/64/128/250/475/860 SPS. The implementation fixes 128.
- Normal analog inputs must remain between GND and VDD. Absolute maximum is
  GND−0.3 V to VDD+0.3 V; that is a damage limit, **not an operating allowance**.
  Large PGA ranges do not authorize above-supply inputs. A 16-bit result does
  not promise 16 effective noise-free bits.

Sources:
[Seeed pin documentation](https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/),
[TI ADS1115 datasheet, SBAS444E](https://www.ti.com/lit/ds/symlink/ads1115.pdf),
[Espressif ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf).

## ASSUMED

Implementation choices awaiting physical verification:

- Standard XIAO ESP32-S3, not an unspecified pin-compatible clone; 8 MB flash.
- Breakout is an actual ADS1115, accepts 3.3 V on its chosen supply input,
  ADDR is tied to GND, address 0x48; common signal ground.
- SDA/SCL have suitable external pull-ups to **3.3 V**, not 5 V. Internal pull-ups
  are deliberately disabled. Around 4.7 kΩ is a starting point for a short bench
  bus, subject to existing breakout resistors and measured rise time.
- I²C0 at 100 kHz on GPIO5/6; ALERT/RDY unused. Comparator disabled.
- CH0–CH3 are AIN0–AIN3 relative to GND. Default gain index 1, ±4.096 V.
- Single-shot mode, OS polled with bounded waits; no interrupt wiring needed.
  Code verifies the MUX/PGA/mode/rate/comparator readback before accepting data.
- At each channel start the driver requests power-down and waits for idle,
  then starts a fresh conversion. This makes recovery after interrupted scans
  explicit. The driver uses 10 ms I²C transaction timeouts and 20 ms conversion
  deadlines. Failure paths can exceed the healthy scan budget and are reported.

## UNVERIFIED

Before connecting valuable equipment:

1. Inspect exact board/breakout markings and schematic. Confirm supply input,
   pull-up rail, address strap, decoupling, connector labeling and no 5 V logic.
2. Measure VDD, bus idle voltages and SDA/SCL rise time; check ACK at 0x48.
3. Start with ground, then known low DC voltages below both supply and selected
   positive full-scale. Compare all channels and gains against a trusted meter.
4. Test channel isolation and ordering with four different safe inputs. Check
   source impedance, settling, noise and clipping near range limits.
5. Unplug/reconnect ADC; induce a safe bus fault; confirm invalid/null data and
   automatic recovery, without stale values being presented as current.
6. Measure complete scan duration at 100 ms under Wi-Fi traffic and config
   changes. Check overruns, real conversion timeout behavior and task stacks.
7. Capture USB while connecting/disconnecting the reader; verify header, nine
   columns, timestamp spacing, partial-write handling, queue drop reporting,
   and no debug prose after the header during normal operation.
8. Verify Wi-Fi/API/UI on target and mobile browser; test malformed requests,
   missing ADC, reboot/default restoration, and disconnected-browser displays.

No input isolation, attenuation, protection, shunt, sensor excitation or power-rail
interface is supplied by this design. Engineer those separately for each experiment.
USB ground connects to the host: assess ground potential before attaching a DUT.
