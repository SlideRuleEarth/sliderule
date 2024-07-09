--
-- ENDPOINT:    /source/atl24v
--

local json          = require("json")

-- get inputs 
local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]

-- get asset
local asset_name    = parms["asset"] or "icesat2"
local asset         = core.getbyname(asset_name)
if not asset then
    return string.format("{\"message\":\"failed to get asset <%s>\"}", asset_name)
end

-- run bathy viewer
local bathy_parms   = icesat2.bathyparms(parms)
local reader        = icesat2.bathyviewer(asset, resource, bathy_parms)
local timeout       = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
local status        = reader:waiton(timeout * 1000)

-- return counts
if status then
    return json.encode(reader:counts())
else
    return string.format("{\"message\":\"timeout reached <%d>\"}", timeout)
end
