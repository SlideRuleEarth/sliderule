local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut_string = core.ut_string()
runner.assert(ut_string:replace())

-- Report Results --

runner.report()
