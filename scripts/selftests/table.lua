local runner = require("test_executive")
local console = require("console")

-- Check If Present --
if not core.UNITTEST then return end

-- Unit Test --

local ut_table = core.ut_table()
runner.check(ut_table:addremove())
runner.check(ut_table:chaining())
runner.check(ut_table:removing())
runner.check(ut_table:duplicates())
runner.check(ut_table:fulltable())
runner.check(ut_table:collisions())
runner.check(ut_table:stress())

-- Report Results --

runner.report()

