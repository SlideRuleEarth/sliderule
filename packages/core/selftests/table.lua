local runner = require("test_executive")
local console = require("console")

-- Check If Present --

if not core.UNITTEST then
    print("Skipping table self test")
    return
end

-- Unit Test --

local ut_table = core.ut_table()
runner.assert(ut_table:addremove())
runner.assert(ut_table:chaining())
runner.assert(ut_table:removing())
runner.assert(ut_table:duplicates())
runner.assert(ut_table:fulltable())
runner.assert(ut_table:collisions())
runner.assert(ut_table:stress())

-- Report Results --

runner.report()

