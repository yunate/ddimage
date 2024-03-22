#include "ddimage/stdafx.h"
#include "ddimage/ddimage_log_.h"
#include "ddimage/ddimage.h"
#include "ddbase/ddio.h"

#include <FreeImage.h>
#include <memory>

namespace NSP_DD {
static std::function<void(const std::string&)>& get_ddimage_logproc()
{
    static std::function<void(const std::string&)> s_ddimage_logproc;
#ifdef _DEBUG
    s_ddimage_logproc = [](const std::string& log) {
        ddcout(ddconsole_color::red) << log << "\r\n";
    };
#endif
    return s_ddimage_logproc;
}

void ddimage_set_log_proc(const std::function<void(const std::string&)>& proc)
{
    get_ddimage_logproc() = proc;
}

void ddimage_log(const std::string& log)
{
    if (get_ddimage_logproc() != nullptr) {
        get_ddimage_logproc()(log);
    }
}

void ddimage_log(const std::wstring& log)
{
    ddimage_log(ddstr::utf16_ansi(log, false));
}

static void ddimage_log_proc(FREE_IMAGE_FORMAT, const char* msg)
{
    ddimage_log(msg);
}

void init_ddimage_log()
{
    FreeImage_SetOutputMessage(ddimage_log_proc);
}
} // namespace NSP_DD
