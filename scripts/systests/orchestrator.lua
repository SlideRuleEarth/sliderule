local runner = require("test_executive")
local console = require("console")
local json = require("json")

netsvc.orchurl("http://127.0.0.1:8050")

--local m = netsvc.orchlock("sliderule", 1, 600)

local h = netsvc.orchhealth()
runner.check(h, "orchestrator unhealthy")

runner.report()
