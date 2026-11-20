local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

runner.unittest("List Unit Test", function()
    local ut_list = core.ut_list()
    runner.assert(ut_list:addremove())
    runner.assert(ut_list:duplicates())
    runner.assert(ut_list:sort())
end)

-- Report Results --

runner.report()

