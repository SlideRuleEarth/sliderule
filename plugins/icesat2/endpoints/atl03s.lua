--
-- ENDPOINT:    /source/atl03s
--
-- PURPOSE:     subset and return atl03 segments
--
-- INPUT:       rqst
--              {
--                  "asset":        "<name of asset to use, defaults to atlas-local>"
--                  "resources":    ["<url of hdf5 file or object>", ...]
--                  "parms":        {<table of parameters>}
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      atl03rec
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl03rec' and 'atl03rec.photons' RecordObjects
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local resource = rqst["resource"]
local parms = rqst["parms"]
local atl03_asset = parms["asset"] or rqst["atl03-asset"] or "nsidc-s3"
parms["asset"] = atl03_asset -- backward compatibility layer
local timeout = parms["node-timeout"] or parms["timeout"] or icesat2.NODE_TIMEOUT

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.ERROR, string.format("invalid asset specified: %s", atl03_asset))
    do return end
end

-- Get Flatten Option --
local flatten = false
if parms[arrow.PARMS] then
    local output_parms = arrow.parms(parms[arrow.PARMS])
    if output_parms:isparquet() then
        flatten = true
    end
end

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> atl03 subsetting for %s ...", rspq, resource))

-- Request Parameters */
local rqst_parms = icesat2.parms(parms)

-- ATL03 Reader --
local atl03_reader = icesat2.atl03(asset, resource, rspq, rqst_parms, false, flatten)

-- Wait Until Completion --
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds
while (userlog:numsubs() > 0) and not atl03_reader:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration))
        do return end
    end
    local atl03_stats = atl03_reader:stats(false)
    userlog:sendlog(core.INFO, string.format("request <%s> read %d segments in %s (after %d seconds)", rspq, atl03_stats.read, resource, duration))
end

-- Resource Processing Complete
local atl03_stats = atl03_reader:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing for %s complete (%d/%d/%d)", rspq, resource, atl03_stats.read, atl03_stats.filtered, atl03_stats.dropped))
