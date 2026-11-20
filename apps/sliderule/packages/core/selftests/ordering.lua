local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

runner.unittest("Ordering Unit Test", function()
    local ut_ordering = core.ut_ordering()
    runner.assert(ut_ordering:addremove())
    runner.assert(ut_ordering:duplicates())
    runner.assert(ut_ordering:sort())
    runner.assert(ut_ordering:iterator())
    runner.assert(ut_ordering:assignment())
end)

-- Report Results --

runner.report()
