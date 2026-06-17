# Firmware images (.msbl) — not included

Pulse Express boards ship **pre-flashed at the ProtoCentral factory**. You do not
need a firmware image for normal use.

The MAX32664D application firmware (`.msbl`) is **Maxim/Analog Devices intellectual
property and is not redistributed with this library.** No `.msbl` is, or should ever
be, committed to this repository — `*.msbl` and `*.bin` are gitignored.

## For factory programming / recovery only

If you are re-flashing a board (factory or recovery), place your licensed image in
this directory, e.g.:

```
extras/firmware/MAX32664D_BPT_<version>.msbl   <-- gitignored, never committed
```

Then flash it with the host tool, which drives the
[`11.FirmwareFlash`](../../examples/11.FirmwareFlash) sketch over USB-Serial:

```
python3 ../flash_tool/flash_msbl.py --port <PORT> MAX32664D_BPT_<version>.msbl
```

The flashing code is implemented from Maxim/ADI User Guide 6806 (Table 9) and
**must be validated on hardware before production use**. A failed or interrupted
flash leaves the hub in bootloader mode; simply re-run the flash to recover.
