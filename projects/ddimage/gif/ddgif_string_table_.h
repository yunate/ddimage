#ifndef ddimage_gif_ddgif_string_table___h_
#define ddimage_gif_ddgif_string_table___h_

#include "ddbase/ddmini_include.h"
#include <string>

#define MAX_LZW_CODE 4096
namespace NSP_DD {

class ddgif_string_table
{
public:
    ddgif_string_table();
    ~ddgif_string_table();
    void initialize(int min_code_size);
    u8* fill_input_buff(int len);
    void compress_start(int bpp, int width);
    int compress_end(u8* buf);
    bool compress(u8* buf, int* len);
    bool decompress(u8* buf, int* len);
    void done(void);

protected:
    bool m_done = false;

    int m_min_code_size = 0;
    int m_clear_code = 0;
    int m_end_code = 0;
    int m_next_code = 0;

    //compressor information
    int m_bpp = 0;
    int m_slack = 0;

    //compressor state variable
    int m_prefix = 0;

    //compressor/decompressor state variables
    int m_code_size = 0;
    int m_code_mask = 0;
    //decompressor state variable
    int m_old_code = 0;
    //compressor/decompressor bit buffer
    int m_partial = 0;
    int m_partial_size = 0;
    // A specific flag that indicates if the first pixel
    int first_pixel_passed = 0;
    // of the whole image had already been read

    //This is what is really the "string table" data for the _decompressor
    std::string m_strings[MAX_LZW_CODE];
    int* m_strmap = nullptr;

    //input buffer
    u8* m_buffer = nullptr;
    int m_buffer_size = 0;
    int m_buffer_real_size = 0;
    int m_buffer_pos = 0;
    int m_buffer_shift = 0;

    void clear_compressor_table(void);
    void clear_decompressor_table(void);
};
} // namespace NSP_DD
#endif // ddimage_gif_ddgif_string_table___h_

