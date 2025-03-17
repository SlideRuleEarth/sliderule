local runner = require("test_executive")
local console = require("console")

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Check If Present --

if not core.UNITTEST then
    print("Skipping ordering self test")
    return
end

-- Unit Test --

local ut_ordering = core.ut_ordering()
runner.assert(ut_ordering:addremove())
runner.assert(ut_ordering:duplicates())
runner.assert(ut_ordering:sort())
runner.assert(ut_ordering:iterator())
runner.assert(ut_ordering:assignment())

-- Report Results --

runner.report()

