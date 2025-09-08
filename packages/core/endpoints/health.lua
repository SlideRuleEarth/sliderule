--
-- ENDPOINT:    /source/health
--
-- INPUT:       None
--
-- OUTPUT:      application health
--

local json = require("json")
local healthy = sys.healthy()
return json.encode({healthy=healthy}), healthy
