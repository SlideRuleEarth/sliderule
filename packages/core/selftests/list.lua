local runner = require("test_executive")
local console = require("console")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut_list = core.ut_list()
runner.assert(ut_list:addremove())
runner.assert(ut_list:duplicates())
runner.assert(ut_list:sort())

-- Report Results --

runner.report()

