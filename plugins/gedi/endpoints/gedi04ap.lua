--
-- ENDPOINT:    /source/gedi04ap
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "gedi04a", "gedi04a", "footprint.longitude", "footprint.latitude")
