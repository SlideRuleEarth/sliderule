local runner = require("test_executive")
local console = require("console")
local prettyprint = require("prettyprint")
local json = require("json")

-- Setup

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

-- (1) Check Defaults

local p = core.rqstparms({})

--prettyprint.display(parms.export())

-- Clean Up --

-- Report Results --

runner.report()
