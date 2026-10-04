# dmimg_png API Reference

dmimg_png has no API of its own: it implements the dmimg DIF, and programs
use it through dmimg (`dmimg_open()`, `dmimg_open_file()` - see dmimg's
api-reference.md).

| DIF function | Behavior |
|--------------|----------|
| `_probe` | The 8-byte PNG signature |
| `_open` | Reads the signature and IHDR (33 bytes): width, height, `alpha` (an alpha channel, or a palette - tRNS), `scales` = `DMIMG_SCALE(0)`. `-EBADMSG` for anything else, `-EIO`, `-ENOMEM` |
| `_decode` | Scale 0 only (`-EINVAL` otherwise). Not interlaced: one block per row, in order. Adam7: one block per pixel of each pass, sized as the area it stands for (8x8 for pass 1, ...; clipped to the image), the later passes over the earlier ones. `-EBADMSG` for a damaged or truncated image (CRC, inflate, filters), `-EIO` |
| `_close` | Releases the decoder |

## Memory

- the decoder (pngle): about 45 KiB - the inflate window (32 KiB) and the
  inflate state;
- two scanlines of the image;
- one row of 0xAARRGGBB pixels (4 bytes per pixel of width) when the image
  is not interlaced.

Nothing depends on the image's height.
