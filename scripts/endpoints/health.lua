--
-- ENDPOINT:    /source/health
--
-- INPUT:       None
--
-- OUTPUT:      application health
--

local json = require("json")
return json.encode({healthy=sys.healthy()})
