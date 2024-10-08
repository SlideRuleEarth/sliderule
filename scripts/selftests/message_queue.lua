local runner = require("test_executive")

-- Check If Present --
if not core.UNITTEST then return end

-- Unit Test --
local ut_msgq = core.ut_msgq()
runner.check(ut_msgq:blocking_receive())
runner.check(ut_msgq:subscribe_unsubscribe())
runner.check(ut_msgq:subscriber_of_opportunity())

-- Report Results --
runner.report()


