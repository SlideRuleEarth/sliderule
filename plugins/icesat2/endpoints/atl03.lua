--
-- ENDPOINT:    /source/atl03
--
-- INPUT:       rqst
--              {
--                  "asset":        "<name of asset to use, defaults to atl03-local>"
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
local userlog = core.logger(rspq, core.USER, true)

-- Request Parameters --
local rqst = json.decode(arg[1])
local asset_name = rqst["asset"] or "atl03-local"
local resources = rqst["resources"]
local timeout = rqst["timeout"] or core.PEND

-- Post Initial Status Progress --
sys.log(core.USER, string.format("atl03 indexing initiated on %s data...\n", asset_name))

-- Index Asset --  
local atl03 = core.getbyname(asset_name)
local name, format, url, index_filename, status = atl03:info()
local indexer = icesat2.atl03indexer(atl03, resources, rspq)

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not indexer:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        sys.log(core.USER, string.format("request timed-out after %d seconds\n", duration / 1000))
        return
    end
    -- Get Stats --
    local stats = indexer:stats()
    -- Dispay Progress --
    sys.log(core.USER, string.format("processed %d entries from %d threads\n", stats.processed, stats.threads))
end

-- Processing Complete
sys.log(core.USER, "...processing complete\n")
return
