--
-- ENDPOINT:    /source/gedi02ap
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "gedi02a", "gedi02a", "footprint.longitude", "footprint.latitude")
