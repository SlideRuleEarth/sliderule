--
-- ENDPOINT:    /source/atl24gp
--

local json = require("json")
local proxy = require("proxy")
local earthdata = require("earth_data_query")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

proxy.proxy(resources, parms, "atl24g", "bathyrec")
