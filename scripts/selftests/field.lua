local runner = require("test_executive")
local console = require("console")

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- Unit Test --

local ut = core.ut_field()
runner.check(ut:element())
runner.check(ut:array())
runner.check(ut:column())
runner.check(ut:dictionary())

-- Report Results --

runner.report()

