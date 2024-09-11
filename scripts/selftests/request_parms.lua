local runner = require("test_executive")
local console = require("console")
local prettyprint = require("prettyprint")
local json = require("json")

-- Setup

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

-- (1) Check Defaults

local parms = core.rqstparms()
local ptable = parms:export()

runner.check(ptable["rqst_timeout"] == core.RQST_TIMEOUT)
runner.check(ptable["node_timeout"] == core.NODE_TIMEOUT)
runner.check(ptable["read_timeout"] == core.READ_TIMEOUT)
runner.check(ptable["cluster_size_hint"] == 0)
runner.check(ptable["points_in_polygon"] == 0)
runner.check(ptable["region_mask"]["rows"] == 0)
runner.check(ptable["region_mask"]["cols"] == 0)

prettyprint.display(ptable)

-- (2) Set Timeouts

parms = core.rqstparms({timeout=400})
ptable = parms:export()

runner.check(ptable["rqst_timeout"] == 400)
runner.check(ptable["node_timeout"] == 400)
runner.check(ptable["read_timeout"] == 400)

parms = core.rqstparms({rqst_timeout=400})
ptable = parms:export()

runner.check(ptable["rqst_timeout"] == 400)
runner.check(ptable["node_timeout"] == core.NODE_TIMEOUT)
runner.check(ptable["read_timeout"] == core.READ_TIMEOUT)

-- Clean Up --

-- Report Results --

runner.report()
