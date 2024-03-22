#ifndef ddimage_quantize_ddquantize__h_
#define ddimage_quantize_ddquantize__h_

#include "ddimage/ddimage.h"
#include "ddbase/ddmini_include.h"
#include <vector>

namespace NSP_DD {
class ddimage_quantize
{
public:
    static bool wu_quantize(const ddbitmap* bitmap, u16 palette_size, std::vector<ddrgb>& color_table, ddbuff& bits, const std::vector<ddrgb>& reserve_palette = {});
};
} // namespace NSP_DD
#endif // ddimage_quantize_ddquantize__h_
