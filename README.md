# CatScan — 0.1.0-dev

A standalone, USB-powered slow analog telemetry logger for a Seeed Studio XIAO
ESP32-S3 and ADS1115. Four single-ended voltage channels, timestamped USB CSV,
a local JSON API, and a small live browser dashboard. No JumpJet dependencies
or product policy. Intended for bench characterization and general analog validation.

## Hardware and wiring

- XIAO ESP32-S3, USB-C **data** cable, Wi-Fi antenna attached.
- ADS1115 breakout that supports direct 3.3 V power and 3.3 V I²C.
- Common ground, short wiring, appropriate signal conditioning/protection.
- SDA/SCL pull-ups to 3.3 V (check the breakout before adding more).

| XIAO | ADS1115 |
| --- | --- |
| 3V3 | VDD / verified 3.3 V supply input |
| GND | GND and ADDR (address 0x48) |
| D4 / GPIO5 | SDA |
| D5 / GPIO6 | SCL |
| — | AIN0–AIN3: signals relative to GND |
| — | ALERT/RDY: unused |

**Keep every analog input within GND–VDD, nominally 0–3.3 V.** The PGA setting
is not an input voltage rating. There is no isolation or overvoltage protection
provided by this firmware. See [hardware notes](docs/HARDWARE.md).

## Build and flash

ESP-IDF **v5.3.3** is the validation baseline. Newer versions require revalidation.
Use Espressif's normal installation and exported environment, then:

```sh
idf.py set-target esp32s3
idf.py menuconfig   # CatScan: choose an AP password
idf.py build
idf.py -p /dev/ttyACM0 flash
```

On Windows, use the ESP-IDF terminal and the board's COM port instead.
If needed, hold BOOT while connecting USB / resetting, then release BOOT to
enter the ROM downloader. After flashing, reset normally.

Defaults use 8 MB flash, no PSRAM dependency, and a 1 kHz FreeRTOS tick. Application
logs go to UART0, **D6 / GPIO43 TX**, 115200 baud, using a separate 3.3 V UART
adapter and common ground. USB Serial/JTAG is reserved for CSV. Do not enable
USB console mirroring in menuconfig. Native USB uses GPIO19/20; leave them alone.

Connect to `CatScan-XXXXXX`, default password `catscan-bench`, and open
<http://192.168.4.1>. This is a local WPA2 bench AP, with no upstream network,
cloud, TLS, captive portal, or per-user authentication. Anyone with AP access
can change logger configuration. Set a private password before use on a shared bench.

## Sampling and CSV

Default: 1000 ms scan period, all four inputs, shared PGA ±4.096 V, 128 SPS,
100 kHz I²C, single-shot conversions. Intervals: 100, 250, 500, 1000, 2000,
5000, 10000 ms. All six distinct PGA ranges are selectable. Settings are RAM-only.

Single-shot operation explicitly completes one conversion before selecting the
next channel. This avoids attributing an old continuous conversion to a new MUX.
The 80 ms healthy scan budget leaves margin at the minimum 100 ms interval.
A nominal conversion takes 7.8125 ms; four conversions need 31.25 ms before
bus, polling, startup, and scheduling overhead. This is a design budget, not a
measured guarantee. Overruns are counted; there are no catch-up bursts.

```csv
timestamp_ms,ch0_raw,ch0_v,ch1_raw,ch1_v,ch2_raw,ch2_v,ch3_raw,ch3_v
```

The header is written once per boot. Each row uses monotonic milliseconds since
boot at **scan completion**, not wall time. Channels are sequential, not simultaneous.
Unavailable raw/voltage pairs are empty fields, never fabricated zeros. Voltages
use six decimal places; this is formatting, not an accuracy claim.

Use a raw serial capture tool, not an IDF monitor transcript. Open capture before
resetting the board to receive the header. ROM boot text can precede the header;
start parsing at the exact header. Application and driver logs use UART0 only.
A later reconnect does not resend the header. USB transport is not a lossless
storage device: the bounded 16-row queue drops new rows when full and increments
`csv_dropped_rows`; queued old rows may arrive after reconnect. The count covers
queue drops, not host-side USB loss. Sampling continues without a serial reader.

Config mutations take effect on a new complete scan. An in-progress scan under
an old config is discarded. Previously queued CSV rows retain their original
conversion and timestamp; CSV has no range/config metadata. For auditable runs,
record config separately and do not change it during a capture.

## Code and validation

- `catscan_adc`: register I/O, single-shot sequencing, signed counts and conversion.
- `catscan_config`: supported cadence/range policy and JSON validation.
- `catscan_model`: state, explicit faults/staleness, JSON and CSV serialization.
- `catscan_http`: three API routes and embedded plain HTML/CSS/JS.
- `main`: sampling task, synchronized state, USB writer, local network startup.

```sh
IDF_PATH=/path/to/esp-idf ./tests/run.sh
git diff --check
```

Host tests use IDF's cJSON source, GCC, AddressSanitizer and UndefinedBehaviorSanitizer.
If your environment cannot run LeakSanitizer, use `ASAN_OPTIONS=detect_leaks=0`;
this disables leak detection only. Project C sources compile with warnings as errors.
See [API](docs/API.md), [hardware verification](docs/HARDWARE.md), and
[validation results](docs/VALIDATION.md).

## Limitations and roadmap

No calibration, effective-resolution claim, open-input detection, or automatic
clipping detection. Floating or saturated analog inputs may still yield `valid:true`:
valid means a fresh successful digital acquisition, not a verified electrical signal.
A responding address is not chip identity verification; ADS1115 has no chip-ID
register used here. Noise, source impedance, grounding and supply quality matter.

Deferred: SD, MQTT, thermistor/current conversion, differential operation/UI, OTA,
DragonSniff integration, cloud, enclosures, charts, and remote control. Future
slices should start with measured board validation and calibration requirements.
