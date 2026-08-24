/*
 * DXT1/3/5 block codec.
 *
 * Compiled by inclusion from gl_impl.c (which needs the decoder in its
 * readback path) and from the standalone t_dxt.c test harness, so the code
 * under test is byte-for-byte the code shipped in the DLL.
 *
 * The decoder supports an application that glReadPixels a surface whose
 * D3D9 storage is DXT-compressed.  That happens in id Tech 5 games, which
 * render their virtual-texture feedback (and some post targets) into
 * compressed textures and read the pages back out on the CPU.  The old code
 * refused those reads ("unsupported ... skipped"), handed the application an
 * untouched buffer, and the engine concluded no texture pages were ever
 * needed — every surface came out with the hash placeholder.  The encoder
 * covers the inverse legal GL operation: uploading ordinary RGBA/BGRA pixels
 * into storage whose requested internal format is DXT1/3/5.
 */

/* Expand a 5-bit / 6-bit DXT colour channel to 8-bit. */
static unsigned char _glsDXTExpand5(unsigned int v)
{
    return (unsigned char)((v << 3) | (v >> 2));
}
static unsigned char _glsDXTExpand6(unsigned int v)
{
    return (unsigned char)((v << 2) | (v >> 4));
}

/* Decode one 4x4 DXT1/3/5 block to RGBA8. */
static void _glsDecodeDXTBlock(const unsigned char *blk, D3DFORMAT fmt,
                               unsigned char out[16][4])
{
    unsigned short c0, c1;
    const unsigned char *cb;   /* colour block: bytes 0-7 (DXT1) or 8-15 (DXT3/5) */
    int colours[4][3];
    int alpha[16];
    int has1BitAlpha = 0;
    int i;

    cb = (fmt == D3DFMT_DXT1) ? blk : (blk + 8);

    c0 = (unsigned short)(cb[0] | (cb[1] << 8));
    c1 = (unsigned short)(cb[2] | (cb[3] << 8));

    colours[0][0] = _glsDXTExpand5(c0 & 0x1F);
    colours[0][1] = _glsDXTExpand6((c0 >> 5) & 0x3F);
    colours[0][2] = _glsDXTExpand5((c0 >> 11) & 0x1F);
    colours[1][0] = _glsDXTExpand5(c1 & 0x1F);
    colours[1][1] = _glsDXTExpand6((c1 >> 5) & 0x3F);
    colours[1][2] = _glsDXTExpand5((c1 >> 11) & 0x1F);

    if (c0 > c1) {
        /* Four-colour mode. */
        colours[2][0] = (2 * colours[0][0] + colours[1][0]) / 3;
        colours[2][1] = (2 * colours[0][1] + colours[1][1]) / 3;
        colours[2][2] = (2 * colours[0][2] + colours[1][2]) / 3;
        colours[3][0] = (colours[0][0] + 2 * colours[1][0]) / 3;
        colours[3][1] = (colours[0][1] + 2 * colours[1][1]) / 3;
        colours[3][2] = (colours[0][2] + 2 * colours[1][2]) / 3;
    } else {
        /* Three-colour mode; index 3 is transparent black in DXT1. */
        colours[2][0] = (colours[0][0] + colours[1][0]) / 2;
        colours[2][1] = (colours[0][1] + colours[1][1]) / 2;
        colours[2][2] = (colours[0][2] + colours[1][2]) / 2;
        colours[3][0] = 0;
        colours[3][1] = 0;
        colours[3][2] = 0;
        if (fmt == D3DFMT_DXT1)
            has1BitAlpha = 1;
    }

    if (fmt == D3DFMT_DXT1) {
        for (i = 0; i < 16; i++)
            alpha[i] = 0xFF;
    } else if (fmt == D3DFMT_DXT3) {
        /* 16 4-bit alphas in the first 8 bytes. */
        const unsigned char *a = blk;
        for (i = 0; i < 16; i++) {
            unsigned int nib = (i & 1) ? (a[i >> 1] >> 4) : (a[i >> 1] & 0xF);
            alpha[i] = (int)(nib * 17);
        }
    } else { /* DXT5 */
        /* Two 8-bit alpha endpoints plus 16 3-bit indices (48 bits). */
        unsigned int a0 = blk[0], a1 = blk[1];
        const unsigned char *ip = blk + 2;
        unsigned __int64 bits = (unsigned __int64)ip[0] |
                                ((unsigned __int64)ip[1] << 8) |
                                ((unsigned __int64)ip[2] << 16) |
                                ((unsigned __int64)ip[3] << 24) |
                                ((unsigned __int64)ip[4] << 32) |
                                ((unsigned __int64)ip[5] << 40);
        for (i = 0; i < 16; i++) {
            unsigned int v = (unsigned int)((bits >> (3 * i)) & 7);
            unsigned int a;
            if (v == 0)      a = a0;
            else if (v == 1) a = a1;
            else if (a0 > a1) a = ((8 - v) * a0 + (v - 1) * a1) / 7;
            else if (v == 6) a = 0;
            else if (v == 7) a = 255;
            else             a = ((8 - v) * a0 + (v - 1) * a1) / 5;
            alpha[i] = (int)a;
        }
    }

    /* Colour indices: one byte per row, 2 bits per texel. */
    for (i = 0; i < 16; i++) {
        int y = i >> 2, x = i & 3;
        int idx = (cb[4 + y] >> (2 * x)) & 3;
        out[i][0] = (unsigned char)colours[idx][0];
        out[i][1] = (unsigned char)colours[idx][1];
        out[i][2] = (unsigned char)colours[idx][2];
        out[i][3] = (unsigned char)((has1BitAlpha && idx == 3) ? 0 : alpha[i]);
    }
}

static unsigned short _glsDXTPack565(const unsigned char rgba[4])
{
    return (unsigned short)(((unsigned int)(rgba[0] >> 3) << 11) |
                            ((unsigned int)(rgba[1] >> 2) << 5) |
                             (unsigned int)(rgba[2] >> 3));
}

/* Fast, deterministic bounding-box encoder.  It is intentionally modest —
 * D3D9 needs valid blocks here, not offline-asset quality — but preserves
 * every upload and produces the exact constant colour for solid UI/video
 * blocks, which are the important runtime case. */
static void _glsEncodeDXTBlock(const unsigned char rgba[16][4], D3DFORMAT fmt,
                               unsigned char *blk)
{
    unsigned char lo[4], hi[4];
    unsigned short c0, c1;
    unsigned char *cb = (fmt == D3DFMT_DXT1) ? blk : blk + 8;
    int palette[4][3];
    int transparent = 0;
    int i, c;

    memcpy(lo, rgba[0], 4);
    memcpy(hi, rgba[0], 4);
    for (i = 1; i < 16; ++i) {
        for (c = 0; c < 4; ++c) {
            if (rgba[i][c] < lo[c]) lo[c] = rgba[i][c];
            if (rgba[i][c] > hi[c]) hi[c] = rgba[i][c];
        }
        if (rgba[i][3] < 128) transparent = 1;
    }
    if (rgba[0][3] < 128) transparent = 1;

    c0 = _glsDXTPack565(hi);
    c1 = _glsDXTPack565(lo);
    if (fmt == D3DFMT_DXT1 && transparent) {
        unsigned short t;
        if (c0 > c1) { t = c0; c0 = c1; c1 = t; }
    } else {
        unsigned short t;
        if (c0 < c1) { t = c0; c0 = c1; c1 = t; }
        if (c0 == c1) {
            if (c0 < 0xFFFFu) ++c0;
            else if (c1 > 0) --c1;
        }
    }
    cb[0] = (unsigned char)c0; cb[1] = (unsigned char)(c0 >> 8);
    cb[2] = (unsigned char)c1; cb[3] = (unsigned char)(c1 >> 8);

    palette[0][0] = _glsDXTExpand5((c0 >> 11) & 31);
    palette[0][1] = _glsDXTExpand6((c0 >> 5) & 63);
    palette[0][2] = _glsDXTExpand5(c0 & 31);
    palette[1][0] = _glsDXTExpand5((c1 >> 11) & 31);
    palette[1][1] = _glsDXTExpand6((c1 >> 5) & 63);
    palette[1][2] = _glsDXTExpand5(c1 & 31);
    if (c0 > c1) {
        for (c = 0; c < 3; ++c) {
            palette[2][c] = (2 * palette[0][c] + palette[1][c]) / 3;
            palette[3][c] = (palette[0][c] + 2 * palette[1][c]) / 3;
        }
    } else {
        for (c = 0; c < 3; ++c) {
            palette[2][c] = (palette[0][c] + palette[1][c]) / 2;
            palette[3][c] = 0;
        }
    }
    memset(cb + 4, 0, 4);
    for (i = 0; i < 16; ++i) {
        int best = 0, bestError = 0x7FFFFFFF, p, y = i >> 2, x = i & 3;
        if (fmt == D3DFMT_DXT1 && transparent && rgba[i][3] < 128) {
            best = 3;
        } else {
            int limit = (fmt == D3DFMT_DXT1 && transparent) ? 3 : 4;
            for (p = 0; p < limit; ++p) {
                int dr = (int)rgba[i][0] - palette[p][0];
                int dg = (int)rgba[i][1] - palette[p][1];
                int db = (int)rgba[i][2] - palette[p][2];
                int error = dr * dr + dg * dg + db * db;
                if (error < bestError) { bestError = error; best = p; }
            }
        }
        cb[4 + y] |= (unsigned char)(best << (2 * x));
    }

    if (fmt == D3DFMT_DXT3) {
        memset(blk, 0, 8);
        for (i = 0; i < 16; ++i)
            blk[i >> 1] |= (unsigned char)(((rgba[i][3] + 8) / 17) <<
                                           ((i & 1) ? 4 : 0));
    } else if (fmt == D3DFMT_DXT5) {
        unsigned int a[8];
        unsigned __int64 bits = 0;
        unsigned int a0 = hi[3], a1 = lo[3];
        blk[0] = (unsigned char)a0; blk[1] = (unsigned char)a1;
        a[0] = a0; a[1] = a1;
        if (a0 > a1) {
            for (i = 2; i < 8; ++i)
                a[i] = ((8 - i) * a0 + (i - 1) * a1) / 7;
        } else {
            for (i = 2; i < 6; ++i)
                a[i] = ((6 - i) * a0 + (i - 1) * a1) / 5;
            a[6] = 0; a[7] = 255;
        }
        for (i = 0; i < 16; ++i) {
            int p, best = 0, bestError = 0x7FFFFFFF;
            for (p = 0; p < 8; ++p) {
                int error = (int)rgba[i][3] - (int)a[p];
                if (error < 0) error = -error;
                if (error < bestError) { bestError = error; best = p; }
            }
            bits |= (unsigned __int64)best << (3 * i);
        }
        for (i = 0; i < 6; ++i)
            blk[2 + i] = (unsigned char)(bits >> (8 * i));
    }
}
