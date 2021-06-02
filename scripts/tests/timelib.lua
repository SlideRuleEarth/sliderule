local runner = require("test_executive")

-- TimeLib Unit Test --

runner.command("NEW UT_TIMELIB ut_timelib")
runner.command("ut_timelib::CHECK_GMT_2_GPS")
runner.command("ut_timelib::CHECK_GPS_2_GMT")
runner.command("ut_timelib::CHECK_GET_COUNT")
runner.command("DELETE ut_timelib")

-- Lua Time Unit Tests --

timestamp = "2021:06:02T16:42:51.743Z"
gps = time.gmt2gps(timestamp)
year, month, day, hour, minute, second, millisecond = time.gps2date(gps)
runner.check(year == 2021.0, "incorrect year: " .. tostring(year))
runner.check(month == 6.0, "incorrect month: " .. tostring(month))
runner.check(day == 2.0, "incorrect day: " .. tostring(day))
runner.check(hour == 16.0, "incorrect hour: " .. tostring(hour))
runner.check(minute == 42.0, "incorrect minute: " .. tostring(minute))
runner.check(second == 51.0, "incorrect second: " .. tostring(second))
runner.check(millisecond == 743.0, "incorrect millisecond: " .. tostring(ms))

-- Report Results --

runner.report()

