#include "ddimage/stdafx.h"
#include "ddimage/gif/ddgif_string_table_.h"
#pragma warning(disable: 6385)
namespace NSP_DD {
ddgif_string_table::ddgif_string_table()
{
    m_buffer = NULL;
    first_pixel_passed = 0; // Still no pixel read
    // Maximum number of entries in the map is MAX_LZW_CODE * 256 
    // (aka 2**12 * 2**8 => a 20 bits key)
    // This Map could be optmized to only handle MAX_LZW_CODE * 2**(m_bpp)
    m_strmap = new(std::nothrow) int[1 << 20];
}

ddgif_string_table::~ddgif_string_table()
{
    if (m_buffer != NULL) {
        delete[] m_buffer;
        m_buffer = NULL;
    }
    if (m_strmap != NULL) {
        delete[] m_strmap;
        m_strmap = NULL;
    }
}

void ddgif_string_table::initialize(int min_code_size)
{
    m_done = false;

    m_bpp = 8;
    m_min_code_size = min_code_size;
    m_clear_code = 1 << m_min_code_size;
    if (m_clear_code > MAX_LZW_CODE) {
        m_clear_code = MAX_LZW_CODE;
    }
    m_end_code = m_clear_code + 1;

    m_partial = 0;
    m_partial_size = 0;

    m_buffer_size = 0;
    clear_compressor_table();
    clear_decompressor_table();
}

u8* ddgif_string_table::fill_input_buff(int len)
{
    if (m_buffer == NULL) {
        m_buffer = new(std::nothrow) u8[len];
        m_buffer_real_size = len;
    } else if (len > m_buffer_real_size) {
        delete[] m_buffer;
        m_buffer = new(std::nothrow) u8[len];
        m_buffer_real_size = len;
    }
    m_buffer_size = len;
    m_buffer_pos = 0;
    m_buffer_shift = 8 - m_bpp;
    return m_buffer;
}

void ddgif_string_table::compress_start(int bpp, int width)
{
    m_bpp = bpp;
    m_slack = (8 - ((width * bpp) % 8)) % 8;

    m_partial |= m_clear_code << m_partial_size;
    m_partial_size += m_code_size;
    clear_compressor_table();
}

int ddgif_string_table::compress_end(u8* buf)
{
    int len = 0;

    //output code for remaining prefix
    m_partial |= m_prefix << m_partial_size;
    m_partial_size += m_code_size;
    while (m_partial_size >= 8) {
        *buf++ = (u8)m_partial;
        m_partial >>= 8;
        m_partial_size -= 8;
        len++;
    }

    //add the end of information code and flush the entire buffer out
    m_partial |= m_end_code << m_partial_size;
    m_partial_size += m_code_size;
    while (m_partial_size > 0) {
        *buf++ = (u8)m_partial;
        m_partial >>= 8;
        m_partial_size -= 8;
        len++;
    }

    //most this can be is 4 u8s.  7 bits in m_partial to start + 12 for the
    //last code + 12 for the end code = 31 bits total.
    return len;
}

bool ddgif_string_table::compress(u8* buf, int* len)
{
    if (m_buffer_size == 0 || m_done) {
        return false;
    }

    int mask = (1 << m_bpp) - 1;
    u8* bufpos = buf;
    while (m_buffer_pos < m_buffer_size) {
        //get the current pixel value
        char ch = (char)((m_buffer[m_buffer_pos] >> m_buffer_shift) & mask);

        // The next prefix is : 
        // <the previous LZW code (on 12 bits << 8)> | <the code of the current pixel (on 8 bits)>
        int nextprefix = (((m_prefix) << 8) & 0xFFF00) + (ch & 0x000FF);
        if (first_pixel_passed) {

            if (m_strmap[nextprefix] > 0) {
                m_prefix = m_strmap[nextprefix];
            } else {
                m_partial |= m_prefix << m_partial_size;
                m_partial_size += m_code_size;
                //grab full u8s for the output buffer
                while (m_partial_size >= 8 && bufpos - buf < *len) {
                    *bufpos++ = (u8)m_partial;
                    m_partial >>= 8;
                    m_partial_size -= 8;
                }

                //add the code to the "table map"
                m_strmap[nextprefix] = m_next_code;

                //increment the next highest valid code, increase the code size
                if (m_next_code == (1 << m_code_size)) {
                    m_code_size++;
                }
                m_next_code++;

                //if we're out of codes, restart the string table
                if (m_next_code == MAX_LZW_CODE) {
                    m_partial |= m_clear_code << m_partial_size;
                    m_partial_size += m_code_size;
                    clear_compressor_table();
                }

                // Only keep the 8 lowest bits (prevent problems with "negative chars")
                m_prefix = ch & 0x000FF;
            }

            //increment to the next pixel
            if (m_buffer_shift > 0 && !(m_buffer_pos + 1 == m_buffer_size && m_buffer_shift <= m_slack)) {
                m_buffer_shift -= m_bpp;
            } else {
                m_buffer_pos++;
                m_buffer_shift = 8 - m_bpp;
            }

            //jump out here if the output buffer is full
            if (bufpos - buf == *len) {
                return true;
            }

        } else {
            // Specific behavior for the first pixel of the whole image

            first_pixel_passed = 1;
            // Only keep the 8 lowest bits (prevent problems with "negative chars")
            m_prefix = ch & 0x000FF;

            //increment to the next pixel
            if (m_buffer_shift > 0 && !(m_buffer_pos + 1 == m_buffer_size && m_buffer_shift <= m_slack)) {
                m_buffer_shift -= m_bpp;
            } else {
                m_buffer_pos++;
                m_buffer_shift = 8 - m_bpp;
            }

            //jump out here if the output buffer is full
            if (bufpos - buf == *len) {
                return true;
            }
        }
    }

    m_buffer_size = 0;
    *len = (int)(bufpos - buf);

    return true;
}

bool ddgif_string_table::decompress(u8* buf, int* len)
{
    if (m_buffer_size == 0 || m_done) {
        return false;
    }

    u8* bufpos = buf;
    for (; m_buffer_pos < m_buffer_size; m_buffer_pos++) {
        m_partial |= (int)m_buffer[m_buffer_pos] << m_partial_size;
        m_partial_size += 8;
        while (m_partial_size >= m_code_size) {
            int code = m_partial & m_code_mask;
            m_partial >>= m_code_size;
            m_partial_size -= m_code_size;

            if (code > m_next_code || /*(m_next_code == MAX_LZW_CODE && code != m_clear_code) || */code == m_end_code) {
                m_done = true;
                *len = (int)(bufpos - buf);
                return true;
            }
            if (code == m_clear_code) {
                clear_decompressor_table();
                continue;
            }

            //add new string to string table, if not the first pass since a clear code
            if (m_old_code != MAX_LZW_CODE && m_next_code < MAX_LZW_CODE) {
                char c = m_strings[code][0];
                if (code == m_next_code) {
                    c = m_strings[m_old_code][0];
                }
                m_strings[m_next_code] = m_strings[m_old_code] + c;
            }

            if ((int)m_strings[code].size() > *len - (bufpos - buf)) {
                //out of space, stuff the code back in for next time
                m_partial <<= m_code_size;
                m_partial_size += m_code_size;
                m_partial |= code;
                m_buffer_pos++;
                *len = (int)(bufpos - buf);
                return true;
            }

            //output the string into the buffer
            memcpy(bufpos, m_strings[code].data(), m_strings[code].size());
            bufpos += m_strings[code].size();

            //increment the next highest valid code, add a bit to the mask if we need to increase the code size
            if (m_old_code != MAX_LZW_CODE && m_next_code < MAX_LZW_CODE) {
                if (++m_next_code < MAX_LZW_CODE) {
                    if ((m_next_code & m_code_mask) == 0) {
                        m_code_size++;
                        m_code_mask |= m_next_code;
                    }
                }
            }

            m_old_code = code;
        }
    }

    m_buffer_size = 0;
    *len = (int)(bufpos - buf);
    return true;
}

void ddgif_string_table::done(void)
{
    m_done = true;
}

void ddgif_string_table::clear_compressor_table(void)
{
    if (m_strmap) {
        memset(m_strmap, 0xFF, sizeof(unsigned int) * (1 << 20));
    }
    m_next_code = m_end_code + 1;

    m_prefix = 0;
    m_code_size = m_min_code_size + 1;
}

void ddgif_string_table::clear_decompressor_table(void)
{
    for (int i = 0; i < m_clear_code; i++) {
        m_strings[i].resize(1);
        m_strings[i][0] = (char)i;
    }
    m_next_code = m_end_code + 1;

    m_code_size = m_min_code_size + 1;
    m_code_mask = (1 << m_code_size) - 1;
    m_old_code = MAX_LZW_CODE;
}

} // namespace NSP_DD
