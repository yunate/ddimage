#ifndef ddimage_ddimage_convert__h_
#define ddimage_ddimage_convert__h_

#include "ddbase/ddmini_include.h"
#include <FreeImagePlus.h>
#include <vector>

namespace NSP_DD {
typedef FIBITMAP*(*FREEIMAGE_CONVERT_PROC)(FIBITMAP*);
void init_ddimage_default_convert_procs_map();
FREEIMAGE_CONVERT_PROC get_default_convert_proc(FREE_IMAGE_FORMAT fif);
FIBITMAP* convert_for_ico(FIBITMAP* dib, s32 width = 48, s32 height = 48);
FIBITMAP* convert_for_wbmp(FIBITMAP* dib, u8 bright = 100);
} // namespace NSP_DD
#endif // ddimage_ddimage_convert__h_
