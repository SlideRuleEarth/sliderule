--
-- ENDPOINT:    /source/metric
--
-- INPUT:       arg[1] -
--              {
--                  "attr":  <metric attribute to query>
--              }
--
-- OUTPUT:      application metrics
--

local json = require("json")
local parm = json.decode(arg[1])
local attr = parm["attr"]
local metrics = sys.metric(attr)

return json.encode(metrics)
