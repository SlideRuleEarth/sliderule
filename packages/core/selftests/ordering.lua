local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut_ordering = core.ut_ordering()
runner.assert(ut_ordering:addremove())
runner.assert(ut_ordering:duplicates())
runner.assert(ut_ordering:sort())
runner.assert(ut_ordering:iterator())
runner.assert(ut_ordering:assignment())

-- Report Results --

runner.report()
