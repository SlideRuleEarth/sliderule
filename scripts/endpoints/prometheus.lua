--
-- ENDPOINT:    /source/prometheus
--
-- INPUT:       N/A
--
-- OUTPUT:      OpentMetrics Text Format (used by prometheus)
--

local json = require("json")
local prettyprint = require("prettyprint")

local metrics = sys.metric()

prettyprint.display(metrics)

return json.encode(metrics)
