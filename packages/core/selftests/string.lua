local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut_string = core.ut_string()
runner.assert(ut_string:replace())
runner.assert(ut_string:find())

-- Report Results --

runner.report()
