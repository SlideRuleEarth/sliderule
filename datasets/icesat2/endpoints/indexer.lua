-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json          = require("json")
local rqst          = json.decode(arg[1])
local parms         = rqst["parms"]
local atl03_asset   = rqst["asset"] or "atlas-local"
local resources     = rqst["resources"]
local timeout       = rqst["timeout"] or core.RQST_TIMEOUT

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- Create User Log --
    local userlog = msg.publish(_rqst.rspq)

    -- Post Initial Status Progress --
    userlog:alert(core.DEBUG, core.RTE_STATUS, string.format("atl03 indexing initiated on %s data...", atl03_asset))

    -- Index Asset --
    local atl03 = core.getbyname(atl03_asset)
    local name, format, url, index_filename, region, endpoint, status = atl03:info()
    local indexer = icesat2.atl03indexer(atl03, resources, _rqst.rspq)

    -- Wait Until Completion --
    local duration = 0
    local interval = 10 -- seconds
    while (userlog:numsubs() > 0) and not indexer:waiton(interval * 1000) do
        duration = duration + interval
        -- Check for Timeout --
        if timeout > 0 and duration == timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request timed-out after %d seconds", duration))
            return
        end
        -- Get Stats --
        local stats = indexer:stats()
        -- Dispay Progress --
        userlog:alert(core.DEBUG, core.RTE_STATUS, string.format("processed %d entries from %d threads", stats.processed, stats.threads))
    end

    -- Processing Complete
    userlog:alert(core.DEBUG, core.RTE_STATUS, string.format("processing complete"))
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL03 Indexer",
    description = "Generate spatial/temporal index records for a set of ATL03 granules",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}

-- INPUT
--  {
--      "resources":    ["<name of hdf5 file or object>", ...]
--      "timeout":      <seconds to wait for first response>
--  }
--
-- OUTPUT
--  Index records (atl03rec.index)
