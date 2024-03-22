#include "ddimage/stdafx.h"
#include "ddimage/quantize/ddquantize_.h"
#include "ddimage/quantize/ddquantizer_wu_.h"

#include <memory>

namespace NSP_DD {
bool ddimage_quantize::wu_quantize(const ddbitmap* bitmap, u16 palette_size, std::vector<ddrgb>& color_table, ddbuff& bits, const std::vector<ddrgb>& reserve_palette)
{
    ddquantizer_wu wu;
    return wu.quantize(bitmap, palette_size, color_table, bits, reserve_palette);
}

} // namespace NSP_DD
