local runner = require("test_executive")
local console = require("console")

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

local ut_string = core.ut_string()
runner.check(ut_string:replace())

-- Report Results --

runner.report()

