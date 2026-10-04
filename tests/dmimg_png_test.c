#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmimg.h"
#include <errno.h>
#include <string.h>

/*
 * The PNG files in fixtures/ (made by fixtures/make_fixtures.py) decoded
 * through dmimg - which loads dmimg_png by the extension. Every image is
 * 9 x 7 of one pattern.
 */

#ifndef DMIMG_PNG_FIXTURES_DIR
#define DMIMG_PNG_FIXTURES_DIR "fixtures"
#endif
#ifndef DMIMG_PNG_TEST_DIR
#define DMIMG_PNG_TEST_DIR "."
#endif
#define FIXTURE(name)   DMIMG_PNG_FIXTURES_DIR "/" name
#define OUTPUT(name)    DMIMG_PNG_TEST_DIR "/" name

#define W   9
#define H   7

static uint32_t g_pixels[W * H];
static uint32_t g_blocks;
static uint32_t g_largest;              /* Pixels of the largest block */

static uint32_t pattern(uint32_t x, uint32_t y, bool alpha)
{
    uint32_t a = alpha ? 255u - x * 20u : 255u;
    return (a << 24) | ((x * 28u) << 16) | ((y * 36u) << 8) | ((x * y * 5u) & 0xFFu);
}

static int collect(void* ctx, const dmimg_block_t* b)
{
    (void)ctx;
    g_blocks++;
    if (b->width * b->height > g_largest)
        g_largest = b->width * b->height;
    if (b->x + b->width > W || b->y + b->height > H)
        return -100;
    for (uint32_t y = 0; y < b->height; y++)
        for (uint32_t x = 0; x < b->width; x++)
            g_pixels[(b->y + y) * W + b->x + x] = b->pixels[y * b->stride + x];
    return 0;
}

/* Decode a file; the number of pixels that differ from the pattern, or -1 */
static int decode(const char* path, bool alpha, bool palette, bool* has_alpha)
{
    dmimg_info_t info;
    int status = 0;
    dmimg_t image = dmimg_open_file(path, &info, &status);
    if (image == NULL)
    {
        Dmod_Printf("    cannot open %s: %d\n", path, status);
        return -1;
    }
    memset(g_pixels, 0, sizeof(g_pixels));
    g_blocks = 0;
    g_largest = 0;
    int ret = (info.width == W && info.height == H && info.scales == DMIMG_SCALE(0)) ? dmimg_decode(image, 0, collect, NULL) : -2;
    if (has_alpha != NULL)
        *has_alpha = info.alpha;
    dmimg_close(image);
    if (ret != 0)
    {
        Dmod_Printf("    %s: %d\n", path, ret);
        return -1;
    }
    int wrong = 0;
    for (uint32_t y = 0; y < H; y++)
        for (uint32_t x = 0; x < W; x++)
            wrong += g_pixels[y * W + x] != pattern(x, palette ? 0 : y, alpha);
    if (wrong != 0)
        Dmod_Printf("    %s: (3, 2) is 0x%08X, expected 0x%08X\n", path, (unsigned)g_pixels[2 * W + 3],
                    (unsigned)pattern(3, palette ? 0 : 2, alpha));
    return wrong;
}

DMOD_TEST_STEP(dmimg_png_decodes_rgb_and_rgba)
{
    bool alpha = true;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("rgb.png"), false, false, &alpha), 0);
    DMOD_TEST_EXPECT_FALSE(alpha);
    DMOD_TEST_EXPECT_EQ(g_blocks, (uint32_t)H);             /* A row at a time */
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("rgba.png"), true, false, &alpha), 0);
    DMOD_TEST_EXPECT_TRUE(alpha);
}

DMOD_TEST_STEP(dmimg_png_decodes_16_bits_and_palettes)
{
    bool alpha = false;
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("rgb16.png"), false, false, NULL), 0);
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("palette.png"), true, true, &alpha), 0);     /* tRNS */
    DMOD_TEST_EXPECT_TRUE(alpha);
}

DMOD_TEST_STEP(dmimg_png_decodes_interlaced_images)
{
    DMOD_TEST_EXPECT_EQ(decode(FIXTURE("adam7.png"), true, false, NULL), 0);
    DMOD_TEST_EXPECT_EQ(g_blocks, (uint32_t)(W * H));       /* Each pixel once ... */
    DMOD_TEST_EXPECT_EQ(g_largest, 8u * 7u);                /* ... the first standing for 8x8 (clipped) */
}

static bool copy_head(const char* from, const char* to, size_t size)
{
    static uint8_t data[512];
    void* f = Dmod_FileOpen(from, "rb");
    if (f == NULL)
        return false;
    size_t n = Dmod_FileRead(data, 1, sizeof(data), f);
    Dmod_FileClose(f);
    if (size > n)
        size = n;
    if ((f = Dmod_FileOpen(to, "wb")) == NULL)
        return false;
    bool ok = Dmod_FileWrite(data, 1, size, f) == size;
    Dmod_FileClose(f);
    return ok;
}

DMOD_TEST_STEP(dmimg_png_rejects_broken_images)
{
    int status = 0;

    /* Cut in the pixels: opens, fails decoding */
    DMOD_TEST_EXPECT_TRUE(copy_head(FIXTURE("rgba.png"), OUTPUT("cut.png"), 120));
    dmimg_t image = dmimg_open_file(OUTPUT("cut.png"), NULL, &status);
    DMOD_TEST_EXPECT_TRUE(image != NULL);
    if (image != NULL)
    {
        DMOD_TEST_EXPECT_EQ(dmimg_decode(image, 0, collect, NULL), -EBADMSG);
        dmimg_close(image);
    }

    /* Cut in IHDR: does not open */
    DMOD_TEST_EXPECT_TRUE(copy_head(FIXTURE("rgba.png"), OUTPUT("short.png"), 20));
    DMOD_TEST_EXPECT_TRUE(dmimg_open_file(OUTPUT("short.png"), NULL, &status) == NULL);
    DMOD_TEST_EXPECT_EQ(status, -EBADMSG);
}
