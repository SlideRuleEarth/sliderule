--
-- ENDPOINT:    /source/atl03sp
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "atl03s", "flat03rec", "flat03rec.photons", "photon.longitude", "photon.latitude")
