#include "ddimage/stdafx.h"
#include "ddimage/quantize/ddquantizer_wu_.h"

// Size of a 3D array : 33 x 33 x 33
#define DDQUANTIZE_WU_SIZE_3D 35937
#define DDQUANTIZE_WU__INDEX(r, g, b) ((r << 10) + (r << 6) + r + (g << 5) + g + b)
#define DDQUANTIZE_WU_MAXCOLOR 256
#define DDQUANTIZE_WU_R 2
#define DDQUANTIZE_WU_G 1
#define DDQUANTIZE_WU_B 0
namespace NSP_DD {
// Build 3-D color histogram of counts, r/g/b, c^2
void ddquantizer_wu::hist_3d(s32* vwt, s32* vmr, s32* vmg, s32* vmb, float* m2, const std::vector<ddrgb>& reserve_palette)
{
    s32 table[256];
    for (s32 i = 0; i < 256; i++) {
        table[i] = i * i;
    }

    s32 inr = 0;
    s32 ing = 0;
    s32 inb = 0;
    s32 ind = 0;
    for (u32 y = 0; y < (u32)m_h; y++) {
        u8* bits = (u8*)&m_bitmap->colors[y * m_bitmap->width];
        for (u32 x = 0; x < (u32)m_w; x++) {
            inr = (bits[DDQUANTIZE_WU_R] >> 3) + 1;
            ing = (bits[DDQUANTIZE_WU_G] >> 3) + 1;
            inb = (bits[DDQUANTIZE_WU_B] >> 3) + 1;
            ind = DDQUANTIZE_WU__INDEX(inr, ing, inb);
            m_qadd[y * m_w + x] = (u16)ind;
            // [inr][ing][inb]
            vwt[ind]++;
            vmr[ind] += bits[DDQUANTIZE_WU_R];
            vmg[ind] += bits[DDQUANTIZE_WU_G];
            vmb[ind] += bits[DDQUANTIZE_WU_B];
            m2[ind] += (float)(table[bits[DDQUANTIZE_WU_R]] + table[bits[DDQUANTIZE_WU_G]] + table[bits[DDQUANTIZE_WU_B]]);
            bits += 4;
        }
    }

    s32 reserve_size = (s32)reserve_palette.size();
    if (reserve_size > 0) {
        s32 max = 0;
        for (s32 i = 0; i < DDQUANTIZE_WU_SIZE_3D; i++) {
            if (vwt[i] > max) max = vwt[i];
        }
        max++;
        for (s32 i = 0; i < reserve_size; i++) {
            inr = (reserve_palette[i].r >> 3) + 1;
            ing = (reserve_palette[i].g >> 3) + 1;
            inb = (reserve_palette[i].b >> 3) + 1;
            ind = DDQUANTIZE_WU__INDEX(inr, ing, inb);
            m_wt[ind] = max;
            m_mr[ind] = max * reserve_palette[i].r;
            m_mg[ind] = max * reserve_palette[i].g;
            m_mb[ind] = max * reserve_palette[i].b;
            m_gm2[ind] = (float)max * (float)(table[reserve_palette[i].r] + table[reserve_palette[i].g] + table[reserve_palette[i].b]);
        }
    }
}

// At conclusion of the histogram step, we can interpret
// wt[r][g][b] = sum over voxel of P(c)
// mr[r][g][b] = sum over voxel of r*P(c)  ,  similarly for mg, mb
// m2[r][g][b] = sum over voxel of c^2*P(c)
// Actually each of these should be divided by 'ImageSize' to give the usual
// interpretation of P() as ranging from 0 to 1, but we needn't do that here.

// We now convert histogram into moments so that we can rapidly calculate
// the sums of the above quantities over any desired box.

// Compute cumulative moments
void ddquantizer_wu::m3d(s32* vwt, s32* vmr, s32* vmg, s32* vmb, float* m2)
{
    s32 area[33];
    s32 area_r[33];
    s32 area_g[33];
    s32 area_b[33];
    float area2[33];
    for (u8 r = 1; r <= 32; r++) {
        for (u8 i = 0; i <= 32; i++) {
            area2[i] = 0;
            area[i] = area_r[i] = area_g[i] = area_b[i] = 0;
        }
        for (u8 g = 1; g <= 32; g++) {
            float line2 = 0;
            s32 line = 0;
            s32 line_r = 0;
            s32 line_g = 0;
            s32 line_b = 0;
            for (u8 b = 1; b <= 32; b++) {
                // [r][g][b]
                u32 ind1 = DDQUANTIZE_WU__INDEX(r, g, b);
                line += vwt[ind1];
                line_r += vmr[ind1];
                line_g += vmg[ind1];
                line_b += vmb[ind1];
                line2 += m2[ind1];
                area[b] += line;
                area_r[b] += line_r;
                area_g[b] += line_g;
                area_b[b] += line_b;
                area2[b] += line2;
                // [r-1][g][b]
                u32 ind2 = ind1 - 1089;
                vwt[ind1] = vwt[ind2] + area[b];
                vmr[ind1] = vmr[ind2] + area_r[b];
                vmg[ind1] = vmg[ind2] + area_g[b];
                vmb[ind1] = vmb[ind2] + area_b[b];
                m2[ind1] = m2[ind2] + area2[b];
            }
        }
    }
}

// Compute sum over a box of any given statistic
s32 ddquantizer_wu::vol(ddquantizer_wu_box* cube, s32* mmt)
{
    return(mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, cube->b1)]
        - mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, cube->b0)]
        - mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b1)]
        + mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b0)]
        - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b1)]
        + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b0)]
        + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b1)]
        - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b0)]);
}

// The next two routines allow a slightly more efficient calculation
// of Vol() for a proposed subbox of a given box.  The sum of Top()
// and Bottom() is the Vol() of a subbox split in the given direction
// and with the specified new upper bound.


// Compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1
// (depending on dir)

s32 ddquantizer_wu::bottom(ddquantizer_wu_box* cube, u8 dir, s32* mmt)
{
    switch (dir) {
    case DDQUANTIZE_WU_R:
        return(-mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b1)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b0)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b1)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b0)]);
        break;
    case DDQUANTIZE_WU_G:
        return(-mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b1)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b0)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b1)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b0)]);
        break;
    case DDQUANTIZE_WU_B:
        return(-mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, cube->b0)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b0)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b0)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b0)]);
        break;
    }

    return 0;
}

// Compute remainder of Vol(cube, mmt), substituting pos for
// r1, g1, or b1 (depending on dir)

s32 ddquantizer_wu::top(ddquantizer_wu_box* cube, u8 dir, s32 pos, s32* mmt)
{
    switch (dir) {
    case DDQUANTIZE_WU_R:
        return(mmt[DDQUANTIZE_WU__INDEX(pos, cube->g1, cube->b1)]
            - mmt[DDQUANTIZE_WU__INDEX(pos, cube->g1, cube->b0)]
            - mmt[DDQUANTIZE_WU__INDEX(pos, cube->g0, cube->b1)]
            + mmt[DDQUANTIZE_WU__INDEX(pos, cube->g0, cube->b0)]);
        break;
    case DDQUANTIZE_WU_G:
        return(mmt[DDQUANTIZE_WU__INDEX(cube->r1, pos, cube->b1)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r1, pos, cube->b0)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r0, pos, cube->b1)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, pos, cube->b0)]);
        break;
    case DDQUANTIZE_WU_B:
        return(mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, pos)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, pos)]
            - mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, pos)]
            + mmt[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, pos)]);
        break;
    }

    return 0;
}

// Compute the weighted variance of a box 
// NB: as with the raw statistics, this is really the variance * ImageSize 

float ddquantizer_wu::var(ddquantizer_wu_box* cube)
{
    float dr = (float)vol(cube, m_mr);
    float dg = (float)vol(cube, m_mg);
    float db = (float)vol(cube, m_mb);
    float xx = m_gm2[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, cube->b1)]
        - m_gm2[DDQUANTIZE_WU__INDEX(cube->r1, cube->g1, cube->b0)]
        - m_gm2[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b1)]
        + m_gm2[DDQUANTIZE_WU__INDEX(cube->r1, cube->g0, cube->b0)]
        - m_gm2[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b1)]
        + m_gm2[DDQUANTIZE_WU__INDEX(cube->r0, cube->g1, cube->b0)]
        + m_gm2[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b1)]
        - m_gm2[DDQUANTIZE_WU__INDEX(cube->r0, cube->g0, cube->b0)];

    return (xx - (dr * dr + dg * dg + db * db) / (float)vol(cube, m_wt));
}

// We want to minimize the sum of the variances of two subboxes.
// The sum(c^2) terms can be ignored since their sum over both subboxes
// is the same (the sum for the whole box) no matter where we split.
// The remaining terms have a minus sign in the variance formula,
// so we drop the minus sign and MAXIMIZE the sum of the two terms.

float ddquantizer_wu::maximize(ddquantizer_wu_box* cube, u8 dir, s32 first, s32 last, s32* cut, s32 whole_r, s32 whole_g, s32 whole_b, s32 whole_w)
{
    s32 base_r = bottom(cube, dir, m_mr);
    s32 base_g = bottom(cube, dir, m_mg);
    s32 base_b = bottom(cube, dir, m_mb);
    s32 base_w = bottom(cube, dir, m_wt);
    float max = 0.0;
    *cut = -1;
    float temp = 0.0f;
    for (s32 i = first; i < last; i++) {
        s32 half_r = base_r + top(cube, dir, i, m_mr);
        s32 half_g = base_g + top(cube, dir, i, m_mg);
        s32 half_b = base_b + top(cube, dir, i, m_mb);
        s32 half_w = base_w + top(cube, dir, i, m_wt);

        // now half_x is sum over lower half of box, if split at i

        if (half_w == 0) {        // subbox could be empty of pixels!
            continue;            // never split into an empty box
        } else {
            temp = ((float)half_r * half_r + (float)half_g * half_g + (float)half_b * half_b) / half_w;
        }

        half_r = whole_r - half_r;
        half_g = whole_g - half_g;
        half_b = whole_b - half_b;
        half_w = whole_w - half_w;

        if (half_w == 0) {
            // subbox could be empty of pixels!
            // never split into an empty box
            continue;
        } else {
            temp += ((float)half_r * half_r + (float)half_g * half_g + (float)half_b * half_b) / half_w;
        }

        if (temp > max) {
            max = temp;
            *cut = i;
        }
    }

    return max;
}

bool ddquantizer_wu::cut(ddquantizer_wu_box* set1, ddquantizer_wu_box* set2)
{
    s32 whole_r = vol(set1, m_mr);
    s32 whole_g = vol(set1, m_mg);
    s32 whole_b = vol(set1, m_mb);
    s32 whole_w = vol(set1, m_wt);

    s32 cutr = 0;
    s32 cutg = 0;
    s32 cutb = 0;
    float maxr = maximize(set1, DDQUANTIZE_WU_R, set1->r0 + 1, set1->r1, &cutr, whole_r, whole_g, whole_b, whole_w);
    float maxg = maximize(set1, DDQUANTIZE_WU_G, set1->g0 + 1, set1->g1, &cutg, whole_r, whole_g, whole_b, whole_w);
    float maxb = maximize(set1, DDQUANTIZE_WU_B, set1->b0 + 1, set1->b1, &cutb, whole_r, whole_g, whole_b, whole_w);

    u8 dir = 0;
    if ((maxr >= maxg) && (maxr >= maxb)) {
        dir = DDQUANTIZE_WU_R;
        if (cutr < 0) {
            // can't split the box
            return false;
        }
    } else if ((maxg >= maxr) && (maxg >= maxb)) {
        dir = DDQUANTIZE_WU_G;
    } else {
        dir = DDQUANTIZE_WU_B;
    }

    set2->r1 = set1->r1;
    set2->g1 = set1->g1;
    set2->b1 = set1->b1;

    switch (dir) {
    case DDQUANTIZE_WU_R:
        set2->r0 = set1->r1 = cutr;
        set2->g0 = set1->g0;
        set2->b0 = set1->b0;
        break;

    case DDQUANTIZE_WU_G:
        set2->g0 = set1->g1 = cutg;
        set2->r0 = set1->r0;
        set2->b0 = set1->b0;
        break;

    case DDQUANTIZE_WU_B:
        set2->b0 = set1->b1 = cutb;
        set2->r0 = set1->r0;
        set2->g0 = set1->g0;
        break;
    }

    set1->vol = (set1->r1 - set1->r0) * (set1->g1 - set1->g0) * (set1->b1 - set1->b0);
    set2->vol = (set2->r1 - set2->r0) * (set2->g1 - set2->g0) * (set2->b1 - set2->b0);
    return true;
}

void ddquantizer_wu::mark(ddquantizer_wu_box* cube, s32 label, u8* tag) {
    for (s32 r = cube->r0 + 1; r <= cube->r1; r++) {
        for (s32 g = cube->g0 + 1; g <= cube->g1; g++) {
            for (s32 b = cube->b0 + 1; b <= cube->b1; b++) {
                tag[DDQUANTIZE_WU__INDEX(r, g, b)] = (u8)label;
            }
        }
    }
}

// Wu Quantization algorithm
bool ddquantizer_wu::quantize(const ddbitmap* bitmap, u16 palette_size, std::vector<ddrgb>& color_table, ddbuff& bits, const std::vector<ddrgb>& reserve_palette)
{
    m_bitmap = bitmap;
    m_w = bitmap->width;
    m_h = bitmap->height;

    s32 memory_size = DDQUANTIZE_WU_SIZE_3D * sizeof(float); // gm2
    memory_size += DDQUANTIZE_WU_SIZE_3D * sizeof(s32); // wt
    memory_size += DDQUANTIZE_WU_SIZE_3D * sizeof(s32); // mr
    memory_size += DDQUANTIZE_WU_SIZE_3D * sizeof(s32); // mg
    memory_size += DDQUANTIZE_WU_SIZE_3D * sizeof(s32); // mb
    memory_size += sizeof(u16) * m_w * m_h; // qadd
    memory_size += DDQUANTIZE_WU_SIZE_3D * sizeof(u8); // tag

    ddbuff buff(memory_size);
    m_gm2 = (float*)&buff[0];
    m_wt = (s32*)(m_gm2 + DDQUANTIZE_WU_SIZE_3D);
    m_mr = (s32*)(m_wt + DDQUANTIZE_WU_SIZE_3D);
    m_mg = (s32*)(m_mr + DDQUANTIZE_WU_SIZE_3D);
    m_mb = (s32*)(m_mg + DDQUANTIZE_WU_SIZE_3D);
    m_qadd = (u16*)(m_mb + DDQUANTIZE_WU_SIZE_3D);
    u8* tag = (u8*)(m_qadd + m_w * m_h);

    hist_3d(m_wt, m_mr, m_mg, m_mb, m_gm2, reserve_palette);
    m3d(m_wt, m_mr, m_mg, m_mb, m_gm2);

    s32 next = 0;
    float temp = 0.0f;
    float vv[DDQUANTIZE_WU_MAXCOLOR];
    ddquantizer_wu_box cube[DDQUANTIZE_WU_MAXCOLOR];
    cube[0].r0 = 0;
    cube[0].g0 = 0;
    cube[0].b0 = 0;
    cube[0].r1 = 32;
    cube[0].g1 = 32;
    cube[0].b1 = 32;
    for (s32 i = 1; i < palette_size; i++) {
        if (cut(&cube[next], &cube[i])) {
            // volume test ensures we won't try to cut one-cell box
            vv[next] = (cube[next].vol > 1) ? var(&cube[next]) : 0;
            vv[i] = (cube[i].vol > 1) ? var(&cube[i]) : 0;
        } else {
            // don't try to split this box again
            vv[next] = 0.0;
            // didn't create box i
            i--;
        }

        next = 0;
        temp = vv[0];
        for (s32 k = 1; k <= i; k++) {
            if (vv[k] > temp) {
                temp = vv[k];
                next = k;
            }
        }

        if (temp <= 0.0) {
            // Error: "Only got 'PaletteSize' boxes"
            palette_size = (u16)i + 1;
            break;
        }
    }

    s32 weight = 0;
    color_table.resize(palette_size);
    for (s32 k = 0; k < palette_size; k++) {
        mark(&cube[k], k, tag);
        weight = vol(&cube[k], m_wt);

        if (weight > 0) {
            color_table[k].r = (u8)(((float)vol(&cube[k], m_mr) / (float)weight) + 0.5f);
            color_table[k].g = (u8)(((float)vol(&cube[k], m_mg) / (float)weight) + 0.5f);
            color_table[k].b = (u8)(((float)vol(&cube[k], m_mb) / (float)weight) + 0.5f);
        } else {
            color_table[k].r = 0;
            color_table[k].g = 0;
            color_table[k].b = 0;
        }
    }

    bits.resize(m_h * m_w);
    for (unsigned y = 0; y < m_h; ++y) {
        for (unsigned x = 0; x < m_w; ++x) {
            bits[y * m_w + x] = tag[m_qadd[y * m_w + x]];
        }
    }

    return true;
}
} // namespace NSP_DD
