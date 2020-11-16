--
-- ENDPOINT:    /source/subset
--
-- INPUT:       rqst
--              {
--                  "asset":        "<name of asset to use, defaults to atl03-cloud>"
--                  "poly":         <spatial region>
--                  "resource":     "<url of hdf5 file or object>"
--                  "datasets":     [<dataset name>, <dataset name>, ...]
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Table of subsetting datasets
--

local json = require("json")
local asset = require("asset")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local data_asset = rqst["asset"] or "atl03-cloud"
local datasets = rqst["datasets"]
local resource = rqst["resource"]
local poly = rqst["poly"]
local timeout = rqst["timeout"] or core.PEND

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("subsetter initiated on %s ...\n", resource))

-- ATL03 Reader --
local resource_url = asset.buildurl(data_asset, resource)
reader = icesat2.atl03(resource_url, rspq, {poly=poly}, 0)
reader:name("atl03_reader")

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not reader:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request for %s timed-out after %d seconds\n", resource, duration / 1000))
        return
    end
    local stats = reader:stats(false)
    userlog:sendlog(core.INFO, string.format("read %d segments in %s (after %d seconds)\n", stats.read, resource, duration / 1000))
end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("subsetting of %s complete\n", resource))
return
