local runner = require("test_executive")
local console = require("console")
local json = require("json")

netsvc.orchurl("http://127.0.0.1:8050")

--m = netsvc.orchlock("sliderule", 1, 600)
k = netsvc.orchhealth()
print(k)

runner.report()
