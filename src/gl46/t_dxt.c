#include <stdio.h>
#include <string.h>
#include <windows.h>

#define STRICT
#include <d3d9.h>

/* Make the decoder's static functions externally linkable in this TU. */
#define static
#include "dxt_decode_impl.c"
#undef static

/* Independent reference implementation straight from the S3TC spec, so the
 * code under test is checked against the standard, not against itself. */
static void reference_dxt1(const unsigned char *blk, unsigned char out[16][4])
{
    unsigned short c0 = (unsigned short)(blk[0] | (blk[1] << 8));
    unsigned short c1 = (unsigned short)(blk[2] | (blk[3] << 8));
    int col[4][3];
    int i;
    col[0][0] = _glsDXTExpand5(c0 & 0x1F); col[0][1] = _glsDXTExpand6((c0 >> 5) & 0x3F); col[0][2] = _glsDXTExpand5((c0 >> 11) & 0x1F);
    col[1][0] = _glsDXTExpand5(c1 & 0x1F); col[1][1] = _glsDXTExpand6((c1 >> 5) & 0x3F); col[1][2] = _glsDXTExpand5((c1 >> 11) & 0x1F);
    if (c0 > c1) {
        col[2][0] = (2*col[0][0]+col[1][0])/3; col[2][1] = (2*col[0][1]+col[1][1])/3; col[2][2] = (2*col[0][2]+col[1][2])/3;
        col[3][0] = (col[0][0]+2*col[1][0])/3; col[3][1] = (col[0][1]+2*col[1][1])/3; col[3][2] = (col[0][2]+2*col[1][2])/3;
    } else {
        col[2][0] = (col[0][0]+col[1][0])/2; col[2][1] = (col[0][1]+col[1][1])/2; col[2][2] = (col[0][2]+col[1][2])/2;
        col[3][0] = 0; col[3][1] = 0; col[3][2] = 0;
    }
    for (i = 0; i < 16; i++) {
        int y = i >> 2, x = i & 3;
        int idx = (blk[4 + y] >> (2 * x)) & 3;
        int transparent = (c0 <= c1 && idx == 3);
        out[i][0] = (unsigned char)col[idx][0];
        out[i][1] = (unsigned char)col[idx][1];
        out[i][2] = (unsigned char)col[idx][2];
        out[i][3] = transparent ? 0 : 255;
    }
}

static void reference_dxt5(const unsigned char *blk, unsigned char out[16][4])
{
    unsigned short c0 = (unsigned short)(blk[8] | (blk[9] << 8));
    unsigned short c1 = (unsigned short)(blk[10] | (blk[11] << 8));
    int col[4][3];
    int i;
    unsigned int a0 = blk[0], a1 = blk[1];
    col[0][0] = _glsDXTExpand5(c0 & 0x1F); col[0][1] = _glsDXTExpand6((c0 >> 5) & 0x3F); col[0][2] = _glsDXTExpand5((c0 >> 11) & 0x1F);
    col[1][0] = _glsDXTExpand5(c1 & 0x1F); col[1][1] = _glsDXTExpand6((c1 >> 5) & 0x3F); col[1][2] = _glsDXTExpand5((c1 >> 11) & 0x1F);
    col[2][0] = (2*col[0][0]+col[1][0])/3; col[2][1] = (2*col[0][1]+col[1][1])/3; col[2][2] = (2*col[0][2]+col[1][2])/3;
    col[3][0] = (col[0][0]+2*col[1][0])/3; col[3][1] = (col[0][1]+2*col[1][1])/3; col[3][2] = (col[0][2]+2*col[1][2])/3;
    for (i = 0; i < 16; i++) {
        int y = i >> 2, x = i & 3;
        int idx = (blk[12 + y] >> (2 * x)) & 3;
        out[i][0] = (unsigned char)col[idx][0];
        out[i][1] = (unsigned char)col[idx][1];
        out[i][2] = (unsigned char)col[idx][2];
    }
    {
        unsigned __int64 abits = (unsigned __int64)blk[2] |
                                 ((unsigned __int64)blk[3] << 8) |
                                 ((unsigned __int64)blk[4] << 16) |
                                 ((unsigned __int64)blk[5] << 24) |
                                 ((unsigned __int64)blk[6] << 32) |
                                 ((unsigned __int64)blk[7] << 40);
        for (i = 0; i < 16; i++) {
            unsigned int v = (unsigned int)((abits >> (3 * i)) & 7);
            unsigned int a;
            if (v == 0) a = a0;
            else if (v == 1) a = a1;
            else if (a0 > a1) a = ((8 - v) * a0 + (v - 1) * a1) / 7;
            else if (v == 6) a = 0;
            else if (v == 7) a = 255;
            else a = ((8 - v) * a0 + (v - 1) * a1) / 5;
            out[i][3] = (unsigned char)a;
        }
    }
}

static int fails = 0;

static void check(const char *name, const unsigned char got[16][4], const unsigned char want[16][4])
{
    int i;
    for (i = 0; i < 16; i++) {
        if (memcmp(got[i], want[i], 4) != 0) {
            printf("FAIL %s texel %d: got %d,%d,%d,%d want %d,%d,%d,%d\n",
                   name, i, got[i][0], got[i][1], got[i][2], got[i][3],
                   want[i][0], want[i][1], want[i][2], want[i][3]);
            fails++;
            return;
        }
    }
    printf("ok   %s\n", name);
}

int main(void)
{
    unsigned char got[16][4], want[16][4];

    /* DXT1: white endpoint, black endpoint, all texels index 0. */
    {
        unsigned char b[8] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        _glsDecodeDXTBlock(b, D3DFMT_DXT1, got);
        reference_dxt1(b, want);
        check("DXT1 all-white", got, want);
    }

    /* DXT1: c0 <= c1 -> three-colour mode, texel with index 3 transparent. */
    {
        unsigned char b[8] = { 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        _glsDecodeDXTBlock(b, D3DFMT_DXT1, got);
        reference_dxt1(b, want);
        check("DXT1 3-colour mode", got, want);
    }

    /* DXT1: gradient endpoints with mixed indices. */
    {
        unsigned char b[8] = { 0xE0, 0x07, 0x1F, 0x00, 0xE4, 0x91, 0xA4, 0x51 };
        _glsDecodeDXTBlock(b, D3DFMT_DXT1, got);
        reference_dxt1(b, want);
        check("DXT1 mixed", got, want);
    }

    /* DXT5: alpha 0x00->0xFF with index0 (all a0) -> alpha 0; colour white. */
    {
        unsigned char b[16] = {
            0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        _glsDecodeDXTBlock(b, D3DFMT_DXT5, got);
        reference_dxt5(b, want);
        check("DXT5 a0 alpha", got, want);
    }

    /* DXT5: alpha index 1 -> a1; colour gradient. */
    {
        unsigned char b[16] = {
            0x10, 0x20, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24,
            0xE0, 0x07, 0x1F, 0x00, 0xE4, 0x91, 0xA4, 0x51
        };
        _glsDecodeDXTBlock(b, D3DFMT_DXT5, got);
        reference_dxt5(b, want);
        check("DXT5 mixed", got, want);
    }

    /* DXT5: alpha interpolation where a0 > a1 (smooth ramp). */
    {
        unsigned char b[16] = {
            0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        _glsDecodeDXTBlock(b, D3DFMT_DXT5, got);
        reference_dxt5(b, want);
        check("DXT5 ramp a0>a1", got, want);
    }

    /* DXT3: 4-bit alpha, every texel 0xF -> 255. */
    {
        unsigned char b[16] = {
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        int i;
        _glsDecodeDXTBlock(b, D3DFMT_DXT3, got);
        reference_dxt1(b + 8, want);
        for (i = 0; i < 16; i++) want[i][3] = 255;
        check("DXT3 alpha", got, want);
    }

    /* 4x4 DXT3 with a mixed nibble alpha ramp in bytes 0-7. */
    {
        unsigned char b[16] = {
            0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
            0xE0, 0x07, 0x1F, 0x00, 0xE4, 0x91, 0xA4, 0x51
        };
        int i;
        _glsDecodeDXTBlock(b, D3DFMT_DXT3, got);
        reference_dxt1(b + 8, want);
        for (i = 0; i < 16; i++) {
            unsigned int nib = (i & 1) ? (b[i >> 1] >> 4) : (b[i >> 1] & 0xF);
            want[i][3] = (unsigned char)(nib * 17);
        }
        check("DXT3 mixed alpha", got, want);
    }

    printf(fails ? "FAILURES: %d\n" : "all DXT block tests passed\n", fails);
    return fails;
}