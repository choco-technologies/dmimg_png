#define DMOD_ENABLE_REGISTRATION    ON
#include "dmimg.h"
#include "pngle.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

/*
 * dmimg_png - the PNG decoder of dmimg, on pngle (third_party/pngle, MIT):
 * a streaming decoder with miniz's inflate. It keeps the inflate window
 * (32 KiB), two scanlines and this module's row of 0xAARRGGBB pixels -
 * never the image. Every PNG: 1 ... 16 bits, gray, RGB, palette, with
 * alpha or tRNS, interlaced (Adam7).
 *
 * An image that is not interlaced is output a row at a time. An interlaced
 * one comes in passes: each pixel of a pass as the block it stands for
 * until the later passes fill it in (up to 8x8, then smaller).
 */

#define READ_CHUNK      512u
#define HEAD_SIZE       33u             /* The signature and the IHDR chunk */
#define MAX_BLOCK       64u             /* An Adam7 block: 8x8 */

static const uint8_t g_signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

struct dmimg_decoder
{
    const dmimg_input_t*    input;
    pngle_t*                png;
    uint32_t                width;
    uint32_t                height;
    bool                    interlaced;
    bool                    done;       /* IEND */
    int                     stopped;    /* What the output returned to stop */
    dmimg_output_fn         output;
    void*                   ctx;
    uint32_t*               row;        /* Not interlaced: the row being decoded */
};

/* ---- What pngle takes from the C library (calloc / free / abs in pngle.c,
 *      see CMakeLists.txt) ---- */

int dmimg_png_abs(int v)
{
    return (v < 0) ? -v : v;
}


void* dmimg_png_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size)
        return NULL;
    void* p = Dmod_Malloc(count * size);
    if (p != NULL)
        memset(p, 0, count * size);
    return p;
}

void dmimg_png_free(void* p)
{
    if (p != NULL)
        Dmod_Free(p);
}

/* ---- pngle's callbacks (static: their addresses are handed out - a global
 *      function's address would be taken through the GOT, which the dmod
 *      loader does not relocate) ---- */

static void on_draw(pngle_t* png, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t rgba[4])
{
    struct dmimg_decoder* d = pngle_get_user_data(png);
    if (d->stopped != 0 || d->output == NULL)
        return;
    uint32_t color = ((uint32_t)rgba[3] << 24) | ((uint32_t)rgba[0] << 16) | ((uint32_t)rgba[1] << 8) | rgba[2];
    dmimg_block_t block;

    if (!d->interlaced)
    {
        d->row[x] = color;
        if (x + 1U != d->width)
            return;
        block.x = 0;
        block.y = y;
        block.width = d->width;
        block.height = 1;
        block.stride = d->width;
        block.pixels = d->row;
        d->stopped = d->output(d->ctx, &block);
        return;
    }

    uint32_t pixels[MAX_BLOCK];
    if (w * h > MAX_BLOCK)
        return;                         /* Never: Adam7 blocks are at most 8x8 */
    for (uint32_t i = 0; i < w * h; i++)
        pixels[i] = color;
    block.x = x;
    block.y = y;
    block.width = w;
    block.height = h;
    block.stride = w;
    block.pixels = pixels;
    d->stopped = d->output(d->ctx, &block);
}

static void on_done(pngle_t* png)
{
    struct dmimg_decoder* d = pngle_get_user_data(png);
    d->done = true;
}

/* Feed `size` bytes; returns how many pngle took (it may want more first), -1 on error */
static int feed(struct dmimg_decoder* d, const uint8_t* data, size_t size)
{
    size_t taken = 0;
    while (taken < size && !d->done && d->stopped == 0)
    {
        int n = pngle_feed(d->png, data + taken, size - taken);
        if (n < 0)
            return -1;
        if (n == 0)
            break;
        taken += (size_t)n;
    }
    return (int)taken;
}

static void free_decoder(struct dmimg_decoder* d)
{
    if (d->png != NULL)
        pngle_destroy(d->png);
    if (d->row != NULL)
        Dmod_Free(d->row);
    Dmod_Free(d);
}

/* ---- DIF ---- */

dmod_dmimg_dif_api_declaration(1.0, dmimg_png, bool, _probe, ( const uint8_t* head, size_t size ))
{
    return size >= sizeof(g_signature) && memcmp(head, g_signature, sizeof(g_signature)) == 0;
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_png, dmimg_decoder_t, _open,
                               ( const dmimg_input_t* input, dmimg_info_t* info, int* status ))
{
    uint8_t head[HEAD_SIZE];
    struct dmimg_decoder* d = Dmod_Malloc(sizeof(*d));
    if (d == NULL)
    {
        *status = -ENOMEM;
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->input = input;
    if ((d->png = pngle_new()) == NULL)
    {
        free_decoder(d);
        *status = -ENOMEM;
        return NULL;
    }
    pngle_set_user_data(d->png, d);
    pngle_set_draw_callback(d->png, on_draw);
    pngle_set_done_callback(d->png, on_done);

    /* The signature and IHDR: the size and the kind of pixels, nothing drawn yet */
    int32_t n = input->read(input->ctx, head, sizeof(head));
    const pngle_ihdr_t* ihdr = NULL;
    if (n < 0 || n != (int32_t)sizeof(head) || feed(d, head, sizeof(head)) != (int)sizeof(head) ||
        (ihdr = pngle_get_ihdr(d->png)) == NULL || ihdr->width == 0 || ihdr->height == 0)
    {
        free_decoder(d);
        *status = (n < 0) ? -EIO : -EBADMSG;
        return NULL;
    }
    d->width = ihdr->width;
    d->height = ihdr->height;
    d->interlaced = ihdr->interlace != 0;
    if (!d->interlaced && (d->row = Dmod_Malloc(d->width * sizeof(uint32_t))) == NULL)
    {
        free_decoder(d);
        *status = -ENOMEM;
        return NULL;
    }

    info->width = d->width;
    info->height = d->height;
    info->alpha = (ihdr->color_type & 4u) != 0 || ihdr->color_type == 3;     /* An alpha channel, or a palette's tRNS */
    info->scales = DMIMG_SCALE(0);
    return d;
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_png, int, _decode,
                               ( dmimg_decoder_t d, uint8_t scale, dmimg_output_fn output, void* ctx ))
{
    uint8_t buffer[READ_CHUNK];
    size_t have = 0;
    if (scale != 0 || output == NULL)
        return -EINVAL;
    d->output = output;
    d->ctx = ctx;

    while (!d->done && d->stopped == 0)
    {
        int32_t n = d->input->read(d->input->ctx, buffer + have, sizeof(buffer) - have);
        if (n < 0)
            return -EIO;
        have += (size_t)n;
        int taken = feed(d, buffer, have);
        if (taken < 0)
            return -EBADMSG;
        if (n == 0 && taken == 0)
            break;                      /* The end, and pngle wants more */
        memmove(buffer, buffer + taken, have - (size_t)taken);
        have -= (size_t)taken;
    }
    if (d->stopped != 0)
        return d->stopped;
    return d->done ? 0 : -EBADMSG;      /* Truncated */
}

dmod_dmimg_dif_api_declaration(1.0, dmimg_png, void, _close, ( dmimg_decoder_t d ))
{
    free_decoder(d);
}

/* ---- Module ---- */

int dmod_init(const Dmod_Config_t* Config)
{
    (void)Config;
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
