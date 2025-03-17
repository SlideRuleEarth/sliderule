local runner = require("test_executive")
local console = require("console")

-- Check If Present --

if not core.UNITTEST then
    print("Skipping list self test")
    return
end

-- Unit Test --

local ut_list = core.ut_list()
runner.assert(ut_list:addremove())
runner.assert(ut_list:duplicates())
runner.assert(ut_list:sort())

-- Report Results --

runner.report()

