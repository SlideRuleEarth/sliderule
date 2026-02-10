local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

runner.unittest("Table Unit Test", function()
    local ut_table = core.ut_table()
    runner.assert(ut_table:addremove())
    runner.assert(ut_table:chaining())
    runner.assert(ut_table:removing())
    runner.assert(ut_table:duplicates())
    runner.assert(ut_table:fulltable())
    runner.assert(ut_table:collisions())
    runner.assert(ut_table:stress())
end)

-- Report Results --

runner.report()
