# pngle

[pngle](https://github.com/kikuchan/pngle) by kikuchan - MIT, see
[LICENSE](LICENSE). With it, miniz's inflate (MIT).

Unmodified sources of commit `b1c68193f1d3f8642b3e0e095d457a828038e6fb`
(`src/pngle.c`, `pngle.h`, `miniz.c`, `miniz.h`). dmimg_png builds them with
(CMakeLists.txt, Makefile):

- `PNGLE_NO_GAMMA_CORRECTION` - no libm;
- `MINIZ_NO_MALLOC` - inflate uses pngle's buffers only;
- `calloc`, `free`, `abs` renamed to `dmimg_png_calloc`, `dmimg_png_free`,
  `dmimg_png_abs` (src/dmimg_png.c) - allocations through dmod.
