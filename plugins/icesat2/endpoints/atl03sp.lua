--
-- ENDPOINT:    /source/atl03sp
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "atl03s", "atl03rec", "photons.longitude", "photons.latitude")
