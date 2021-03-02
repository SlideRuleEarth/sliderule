--
-- ENDPOINT:    /source/indexer
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resources":    ["<name of hdf5 file or object>", ...]
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      Index records (atl03rec.index)
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl06rec' and 'atl06rec.elevation' RecordObjects
--

local json = require("json")
local asset = require("asset")

-- Create User Log --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "atlas-local"
local resources = rqst["resources"]
local timeout = rqst["timeout"] or core.PEND

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("atl03 indexing initiated on %s data...\n", atl03_asset))

-- Index Asset --  
local atl03 = core.getbyname(atl03_asset)
local name, format, url, index_filename, status = atl03:info()
local indexer = icesat2.atl03indexer(atl03, resources, rspq)

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not indexer:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request timed-out after %d seconds\n", duration / 1000))
        return
    end
    -- Get Stats --
    local stats = indexer:stats()
    -- Dispay Progress --
    userlog:sendlog(core.INFO, string.format("processed %d entries from %d threads\n", stats.processed, stats.threads))
end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("processing complete\n"))
return
