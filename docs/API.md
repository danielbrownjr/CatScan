# CatScan HTTP API

Base URL: `http://192.168.4.1`. Local AP only. No CORS, TLS or application login.
Responses are JSON except plain-text HTTP errors. Cache-Control is no-store.

## GET /api/config

```json
{"sample_interval_ms":1000,"gain":1,"full_scale_v":4.096,"data_rate_sps":128}
```

`gain` is a stable PGA register index, shared by all channels:

| gain | Range |
| --- | --- |
| 0 | ±6.144 V |
| 1 | ±4.096 V (default) |
| 2 | ±2.048 V |
| 3 | ±1.024 V |
| 4 | ±0.512 V |
| 5 | ±0.256 V |

## PUT /api/config

Complete mutable configuration; both keys required:

```sh
curl -X PUT http://192.168.4.1/api/config   -H 'Content-Type: application/json'   -d '{"sample_interval_ms":250,"gain":2}'
```

Body limit 256 bytes. Accepted Content-Type is `application/json` or
`application/json; charset=utf-8`. Intervals: 100, 250, 500, 1000, 2000, 5000,
10000 ms. Gain: integer 0–5. Reject unknown/duplicate/missing keys, wrong types,
fractional/unsupported values, malformed JSON and trailing content. Output-only
`full_scale_v` and `data_rate_sps` must not be sent back as mutations.

200 returns current config. 400 rejects invalid requests without mutation.
408 closes incomplete/timed-out requests; 500 indicates allocation failure.
The mutation is atomic and validated independently of the browser. It resets
sample validity, increments `config_revision`, and wakes acquisition. A concurrent
old-config scan is discarded. Queued CSV rows are not rewritten. Configuration
is volatile and returns to defaults on reboot.

## GET /api/state

```json
{
  "device":{"product":"CatScan","firmware_version":"0.1.0-dev","uptime_ms":1500},
  "adc":{"present":true,"address":72,"gain":1,"full_scale_v":4.096,
         "data_rate_sps":128,"sample_interval_ms":1000,"config_revision":1,
         "last_sample_age_ms":450,"error":"i2c_error"},
  "channels":[
    {"index":0,"raw":8000,"voltage_v":1.0,"valid":true,"error":"ok"},
    {"index":1,"raw":0,"voltage_v":0,"valid":true,"error":"ok"},
    {"index":2,"raw":null,"voltage_v":null,"valid":false,"error":"i2c_error"},
    {"index":3,"raw":16000,"voltage_v":2.0,"valid":true,"error":"ok"}
  ],
  "csv_dropped_rows":0,"scan_overruns":0,"faults":["i2c_error"]
}
```

Example mixed-channel data are illustrative. The top-level ADC error reports
the first current scan failure. `present` means the last scan's address probe ACKed, not
verified silicon identity. Before probing it is false with `not_sampled`.

`last_sample_age_ms` is time since the last **scan attempt** completed, including
failed attempts, and is null before any scan/after config mutation. Each channel's
error distinguishes successful data from an unsuccessful attempt. Raw and voltage
are null whenever invalid; there are always four indexed channels.

Fault strings: `not_sampled`, `adc_absent` (probe NACK), `i2c_error` (including bus
transaction timeout), `conversion_timeout`, `config_mismatch`, `stale`.
Stale means no complete attempt for more than `2 * sample_interval_ms + 100` ms;
previously valid values become null/invalid. Faults are current, deduplicated,
non-latching diagnostics; acquisition retries each scan.

`csv_dropped_rows` and `scan_overruns` are cumulative boot counters (uint32).
The UI polls every 500 ms without overlapping requests. Network loss blanks
measurements instead of leaving apparently live numbers onscreen.
