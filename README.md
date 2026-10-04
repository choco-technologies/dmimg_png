# dmimg_png

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmimg_png/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmimg_png/actions/workflows/ci.yml)

The PNG decoder of [dmimg](https://github.com/choco-technologies/dmimg).

## Description

A dmimg decoder plugin: once it is enabled - or when
`dmimg_open_file()` meets a `.png` file and loads it by the name - every
program that reads images through dmimg reads PNG files.

- **Every PNG**: 1, 2, 4, 8, 16 bits; gray, RGB, palette; an alpha channel
  or tRNS; interlaced (Adam7). 16-bit channels are reduced to 8 bits.
- **Streaming**: the image is read in pieces of 512 bytes and output a row
  at a time (an interlaced one: its passes, each pixel as the block it
  stands for until the later passes fill it in). The decoder keeps the
  inflate window (32 KiB), two scanlines and one row of 0xAARRGGBB pixels -
  about 45 KiB + 8 bytes per pixel of width - never the image.
- `info.alpha` is set for an alpha channel and for palette images (which
  may have tRNS). A gray or RGB image with a tRNS color key reports no
  alpha, but its transparent pixels are output with alpha 0.
- No gamma correction (gAMA is ignored), no scaling: PNG is decoded whole.

Built on [pngle](https://github.com/kikuchan/pngle) (MIT) with miniz's
inflate - see [third_party/pngle](third_party/pngle).

## Usage

```c
#include "dmimg.h"

dmimg_info_t info;
dmimg_t image = dmimg_open_file("/flash/logo.png", &info, NULL);     /* loads dmimg_png */
if (image != NULL)
{
    dmimg_decode(image, 0, put_block, ctx);
    dmimg_close(image);
}
```

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

The tests decode the PNG files in [tests/fixtures](tests/fixtures) - made by
`tests/fixtures/make_fixtures.py` (RGB, RGBA, 16-bit, palette with tRNS,
Adam7) - through dmimg:

```bash
cd build
ctest --output-on-failure
```

## License

MIT - see [LICENSE](LICENSE); pngle: [third_party/pngle/LICENSE](third_party/pngle/LICENSE).
