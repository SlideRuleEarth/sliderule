local runner = require("test_executive")

-- TimeLib Unit Test --

runner.command("NEW UT_TIMELIB ut_timelib")
runner.command("ut_timelib::CHECK_GMT_2_GPS")
runner.command("ut_timelib::CHECK_GPS_2_GMT")
runner.command("ut_timelib::CHECK_GET_COUNT")
runner.command("DELETE ut_timelib")

-- Report Results --

runner.report()

