--
-- ENDPOINT:    /source/gedi01bp
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "gedi01b", "gedi01b", "footprint.longitude", "footprint.latitude")
