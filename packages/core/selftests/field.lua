local runner = require("test_executive")
local console = require("console")

-- Check If Present --
if not core.UNITTEST then return end

-- Setup --
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- Unit Test --

local ut = core.ut_field()
runner.assert(ut:element())
runner.assert(ut:array())
runner.assert(ut:enumeration())
runner.assert(ut:list())
runner.assert(ut:column())
runner.assert(ut:dictionary())

-- Report Results --

runner.report()

