local runner = require("test_executive")
local console = require("console")
local json = require("json")

netsvc.orchurl("http://127.0.0.1:8050")

--
-- API: health
--
--local h = netsvc.orchhealth()
--runner.check(h, "orchestrator unhealthy")


--
-- API: <register>
--
rsps = netsvc.orchreg({'service':'test', 'lifetime':2, 'name':'bob'})

--
-- API: lock
--
local m = netsvc.orchlock("sliderule", 1, 600)
for k,v in pairs(m) do
    print(k, v)
end

runner.report()
