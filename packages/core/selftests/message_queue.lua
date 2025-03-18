local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut_msgq = core.ut_msgq()
runner.assert(ut_msgq:blocking_receive())
runner.assert(ut_msgq:subscribe_unsubscribe())
runner.assert(ut_msgq:subscriber_of_opportunity())

-- Report Results --

runner.report()
