--
-- ENDPOINT:    /source/atl06p
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "atl06", "atl06rec", "atl06rec.elevation", "longitude", "latitude")
