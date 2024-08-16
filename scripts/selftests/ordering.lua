local runner = require("test_executive")
local console = require("console")

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

local ut_ordering = core.ut_ordering()
runner.check(ut_ordering:addremove())
runner.check(ut_ordering:duplicates())
runner.check(ut_ordering:sort())
runner.check(ut_ordering:iterator())
runner.check(ut_ordering:assignment())

-- Report Results --

runner.report()

