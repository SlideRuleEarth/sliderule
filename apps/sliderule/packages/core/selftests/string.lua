local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

runner.unittest("String Unit Test", function()
    local ut_string = core.ut_string()
    runner.assert(ut_string:replace())
    runner.assert(ut_string:find())
end)

-- Report Results --

runner.report()
