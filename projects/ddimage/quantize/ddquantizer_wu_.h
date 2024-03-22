#ifndef ddimage_quantize_ddquantizer_wu__h_
#define ddimage_quantize_ddquantizer_wu__h_

#include "ddbase/ddmini_include.h"
#include "ddbase/ddcolor.h"
#include "ddbase/ddbitmap.h"

namespace NSP_DD {
class ddquantizer_wu
{
public:
#pragma pack(push, 1) 
    struct ddquantizer_wu_box
    {
        s32 r0; // min value, exclusive
        s32 r1; // max value, inclusive
        s32 g0;
        s32 g1;
        s32 b0;
        s32 b1;
        s32 vol;
    };
#pragma pack(pop)

public:
    bool quantize(const ddbitmap* bitmap, u16 palette_size, std::vector<ddrgb>& color_table, ddbuff& bits, const std::vector<ddrgb>& reserve_palette);

protected:
    void hist_3d(s32* vwt, s32* vmr, s32* vmg, s32* vmb, float* m2, const std::vector<ddrgb>& reserve_palette);
    void m3d(s32* vwt, s32* vmr, s32* vmg, s32* vmb, float* m2);
    s32 vol(ddquantizer_wu_box* cube, s32* mmt);
    s32 bottom(ddquantizer_wu_box* cube, u8 dir, s32* mmt);
    s32 top(ddquantizer_wu_box* cube, u8 dir, s32 pos, s32* mmt);
    float var(ddquantizer_wu_box* cube);
    float maximize(ddquantizer_wu_box* cube, u8 dir, s32 first, s32 last, s32* cut,
        s32 whole_r, s32 whole_g, s32 whole_b, s32 whole_w);
    bool cut(ddquantizer_wu_box* set1, ddquantizer_wu_box* set2);
    void mark(ddquantizer_wu_box* cube, s32 label, u8* tag);

protected:
    const ddbitmap* m_bitmap = nullptr;
    float* m_gm2 = nullptr;
    s32* m_wt = nullptr;
    s32* m_mr = nullptr;
    s32* m_mg = nullptr;
    s32* m_mb = nullptr;
    u16* m_qadd = nullptr;

    u32 m_w = 0;
    u32 m_h = 0;
};
} // namespace NSP_DD
#endif // ddimage_quantize_ddquantizer_wu__h_
