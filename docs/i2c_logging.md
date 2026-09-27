# I2C Transaction Logging

`vc_mipi_core` can record every I2C register read/write it performs (module chip and sensor
chip) into a dedicated trace, independent of the general `debug` verbosity level. It is off by
default and adds no overhead unless enabled.

Each entry shows exactly when a register was accessed, the I2C device involved, and the value:

```text
[1790070578.943075] vc_mod_setup(): READ  dev=10-0010 addr=0x10 reg=0x1000 value=0x6d
[1790070580.059069] vc_sen_write_mode(): WRITE dev=10-0036 addr=0x36 reg=0x0100 value=0x01
```

- `[<seconds>.<microseconds>]` &mdash; wall-clock timestamp of the transaction.
- `<function>()` &mdash; the driver function that issued the read/write.
- `READ` / `WRITE` &mdash; transaction direction.
- `dev=<bus>-<addr>` &mdash; the I2C device, as `<bus number>-<7-bit address>` (matches the name
  shown by `i2cdetect`/`dmesg`); the module chip and the sensor chip normally sit at different
  addresses (e.g. `0x10` vs. `0x1a`), so this tells you which chip the transaction was for.
- `addr=0x<addr>` &mdash; the same I2C device address, repeated in hex.
- `reg=0x<addr>` &mdash; the sensor/module register address.
- `value=0x<val>` &mdash; the byte read or written.
- A failed transfer is marked with a trailing `FAILED` instead of a value guarantee.

## Enabling it

The log is controlled by a single on/off switch, reachable three ways &mdash; all of them read
and write the same underlying flag, so whichever you use, the others reflect it immediately.

### At runtime (module already loaded)

```sh
echo 1 | sudo tee /sys/kernel/debug/vc_mipi_i2c/enable   # turn on
echo 0 | sudo tee /sys/kernel/debug/vc_mipi_i2c/enable   # turn off
```

or equivalently:

```sh
echo 1 | sudo tee /sys/module/vc_mipi_core/parameters/i2c_log
```

Use this when you want to trace whatever happens *after* you enable it (e.g. a specific
`v4l2-ctl` call or a stream start/stop) &mdash; register accesses that already happened before
you switched it on are not retroactively captured.

### At module load (`modprobe`)

To capture everything from the very first I2C transaction &mdash; including the large register
table `vc_mod_setup()` writes during probe, before you'd otherwise get a chance to enable
anything &mdash; pass the parameter when inserting the module:

```sh
sudo modprobe vc_mipi_core i2c_log=1
```

### At boot, automatically

Create `/etc/modprobe.d/vc_mipi_core.conf`:

```text
options vc_mipi_core i2c_log=1
```

This is read by `modprobe`/`udev` whenever the module auto-loads at boot, so logging is active
from the first probe onward on every boot, with no manual step. Remove the file (or set
`i2c_log=0`) to go back to the default off state.

## Reading and clearing the trace

```sh
sudo cat /sys/kernel/debug/vc_mipi_i2c/log      # dump the captured trace
echo 1 | sudo tee /sys/kernel/debug/vc_mipi_i2c/clear   # reset it
```

The trace is kept in a fixed-size 64&nbsp;KB in-memory ring buffer; once full it wraps around
and starts overwriting from the beginning, so clear it before a specific test if you want a
clean trace for just that run.

## Files

| Path | Access | Purpose |
| --- | --- | --- |
| `/sys/kernel/debug/vc_mipi_i2c/enable` | read/write | `1`/`0` (or `Y`/`N`) to turn logging on/off |
| `/sys/kernel/debug/vc_mipi_i2c/log` | read-only | dump the captured trace |
| `/sys/kernel/debug/vc_mipi_i2c/clear` | write-only | write anything to reset the trace |
| `/sys/module/vc_mipi_core/parameters/i2c_log` | read/write | same on/off switch, also settable via `modprobe vc_mipi_core i2c_log=1` |

`/sys/kernel/debug` must be mounted (it is by default on Raspberry Pi OS); if
`/sys/kernel/debug/vc_mipi_i2c/` is missing, check that `vc_mipi_core` is actually the module
being loaded and not a stale build shadowing it (see [Troubleshooting](./troubleshooting.md) for
the DKMS-vs-manual-build case).
