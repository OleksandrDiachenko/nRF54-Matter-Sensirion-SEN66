# SEN66 driver

The SEN66 driver (milestone M2) implements the first two layers of the
architecture boundary — the I2C transport and the protocol/parser — and stops
short of measurement policy (M3) and Matter attributes (M4).

## Modules

All sources live in `src/sen66/` under namespace `Sen66`. The split keeps the
decode logic free of Zephyr so it is unit-tested on the host:

| File | Role | Zephyr dependency |
| --- | --- | --- |
| `sen66_crc.{h,cpp}` | Sensirion CRC-8 | none (host-testable) |
| `sen66_protocol.{h,cpp}` | commands, `Measurement`, parser | none (host-testable) |
| `sen66_driver.{h,cpp}` | I2C transport and high-level ops | `zephyr/drivers/i2c.h` |
| `sen66_shell.cpp` | `sen66` diagnostic shell command | `zephyr/shell` |

`CONFIG_APP_SEN66` gates the driver; `CONFIG_APP_SEN66_SHELL` gates the shell.

## I2C transport

The SEN66 uses standard-mode I2C (100 kHz) at address `0x6B` with **no clock
stretching**: the master writes a 16-bit big-endian command, waits the
command's execution time, then reads the response. Reads and writes are separate
transactions. The DeviceTree node `sen66@6b` on `i2c21` supplies the bus and
address (see [hardware.md](hardware.md)).

Commands used: `0x0021` start measurement, `0x0104` stop, `0x0202` get data
ready, `0x0300` read measured values, `0xD033` get serial number,
`0xD304` device reset.

## Sensirion CRC

Every 2-byte word is followed by a CRC byte: CRC-8, polynomial `0x31`, initial
value `0xFF`, MSB-first, no final XOR. A CRC mismatch means a transport error
and rejects the whole frame — retry/backoff is the measurement service's job
(M3). It is **not** treated as a per-channel "unavailable".

## Measurement decoding

`ReadMeasuredValues` (`0x0300`) returns nine words. Each channel is decoded with
the correct signedness, and its validity bit is set only when the raw word is
not the sensor's unavailable sentinel:

| Word | Channel | Type | Raw scale (raw / N) | Unavailable |
| --- | --- | --- | --- | --- |
| 0 | PM1.0 (µg/m³) | `uint16` | 10 | `0xFFFF` |
| 1 | PM2.5 (µg/m³) | `uint16` | 10 | `0xFFFF` |
| 2 | PM4.0 (µg/m³) | `uint16` | 10 | `0xFFFF` |
| 3 | PM10 (µg/m³) | `uint16` | 10 | `0xFFFF` |
| 4 | Humidity (%RH) | `int16` | 100 | `0x7FFF` |
| 5 | Temperature (°C) | `int16` | 200 | `0x7FFF` |
| 6 | VOC index | `int16` | 10 | `0x7FFF` |
| 7 | NOx index | `int16` | 10 | `0x7FFF` |
| 8 | CO2 (ppm) | `uint16` | 1 | `0xFFFF` |

`Measurement` stores these raw fixed-point values plus a `valid` bitmask (the
`Field` enum). **A cleared field must never be read as a value, least of all
zero.** Unit conversion to Matter attributes is deferred to M4. The serial
number (`0xD033`, 48 bytes) is 16 CRC-checked words of ASCII, parsed into a
NUL-terminated string of up to 32 characters.

This is the key correction over the training prototype, which decoded every word
as signed and ignored the sentinels.

## Testing

- **Unit tests** (`tests/sen66/`, host `type: unit`) cover the CRC vector,
  signedness, scaling, both sentinels, whole-frame CRC rejection, and
  serial-number parsing. Run with `west twister -T tests/sen66 -p unit_testing`.
- **Hardware smoke test**: the `sen66 serial` and `sen66 read` shell commands.

See [building.md](building.md) for the exact commands.
