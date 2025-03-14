local runner = require("test_executive")
local console = require("console")

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

local ut_string = core.ut_string()
runner.assert(ut_string:replace())

-- Report Results --

runner.report()

