--
-- ENDPOINT:    /source/atl03s
--
-- PURPOSE:     subset and return atl03 segments
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resources":    ["<url of hdf5 file or object>", ...]
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
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
local atl03_asset = rqst["atl03-asset"] or "nsidc-s3"
local resource = rqst["resource"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Get Asset --
local asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.ERROR, string.format("invalid asset specified: %s", atl03_asset))
    return
end

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> atl03 subsetting for %s ...", rspq, resource))

-- ATL03 Reader --
local atl03_reader = icesat2.atl03(asset, resource, rspq, parms, false)

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while __alive() and not atl03_reader:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration / 1000))
        return
    end
    local atl03_stats = atl03_reader:stats(false)
    userlog:sendlog(core.INFO, string.format("request <%s> read %d segments in %s (after %d seconds)", rspq, atl03_stats.read, resource, duration / 1000))
end

-- Resource Processing Complete
local atl03_stats = atl03_reader:stats(false)
userlog:sendlog(core.INFO, string.format("request <%s> processing for %s complete (%d/%d/%d)", rspq, resource, atl03_stats.read, atl03_stats.filtered, atl03_stats.dropped))

return
