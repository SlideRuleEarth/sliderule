--
-- ENDPOINT:    /source/atl24gp
--

local json = require("json")
local proxy = require("proxy")
local earthdata = require("earth_data_query")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

local original_asset = parms["asset"]
parms["asset"] = "icesat2-atl09"
parms["resources09"] = earthdata.search(parms)
parms["asset"] = original_asset

proxy.proxy(resources, parms, "atl24g", "bathyrec")
