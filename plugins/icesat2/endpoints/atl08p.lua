--
-- ENDPOINT:    /source/atl08p
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "atl08", "atl08rec", "vegetation.longitude", "vegetation.latitude")
