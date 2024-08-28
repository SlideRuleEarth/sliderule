local runner = require("test_executive")
local console = require("console")

console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- Test --

local ut = core.ut_field()
runner.check(ut:element())
runner.check(ut:array())

-- Report Results --

runner.report()

