#include "../test.h"
#include <siri/version.h>


int main()
{
    test_start("version");

    _assert (siri_version_cmp("1.0.0", "2.0.0") < 0);
    _assert (siri_version_cmp("2.0.0", "1.0.0") > 0);
    _assert (siri_version_cmp("2.2.0", "2.32.0") < 0);
    _assert (siri_version_cmp("2.32.0", "2.2.0") > 0);
    _assert (siri_version_cmp("2.0.5", "2.0.22") < 0);
    _assert (siri_version_cmp("2.0.22", "2.0.5") > 0);
    _assert (siri_version_cmp("2.0", "2.0.0") < 0);
    _assert (siri_version_cmp("2.0.2", "2.0") > 0);
    _assert (siri_version_cmp("a", "") > 0);
    _assert (siri_version_cmp("", "b") < 0);
    _assert (siri_version_cmp("", "") == 0);
    _assert (siri_version_cmp("2.0.0", "2.0.0") == 0);

    return test_end();
}
