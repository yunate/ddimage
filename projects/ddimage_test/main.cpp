#include "ddimage_test/stdafx.h"
#include "ddbase/ddtest_case_factory.h"
#include "ddbase/ddassert.h"
#include "ddbase/windows/ddmoudle_utils.h"
#include "ddbase/ddlocale.h"
#include <process.h>

#pragma comment(lib, "ddimage.lib")
#pragma comment(lib, "ddwin.lib")
#pragma comment(lib, "ddbase.lib")

namespace NSP_DD {
int test_main()
{
    ddmoudle_utils::set_current_directory(L"");
    DDTCF.insert_white_type("teset_case_emoj_pick_up");
    // DDTCF.insert_white_type("test_case_ddgif_render_gif");
    DDTCF.run();
    return 0;
}

} // namespace NSP_DD

void main()
{
    NSP_DD::ddlocale::set_utf8_locale_and_io_codepage();
    // ::_CrtSetBreakAlloc(1932);
    int result = NSP_DD::test_main();

#ifdef _DEBUG
    _cexit();
    DDASSERT_FMT(!::_CrtDumpMemoryLeaks(), L"Memory leak!!! Check the output to find the log.");
    ::system("pause");
    ::_exit(result);
#else
    ::system("pause");
    ::exit(result);
#endif
}

