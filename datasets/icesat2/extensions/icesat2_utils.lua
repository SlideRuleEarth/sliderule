local earthdata = require("earth_data_query")
local json = require("json")

-- Find ATL09 Granule --
local function find_atl09_granule (parms, userlog, with_time_range)
    -- get granule info
    local granule       = parms["granule"] or {}
    local rgt           = granule["rgt"] or parms["rgt"]
    local cycle         = granule["cycle"] or parms["cycle"]
    local rgt_filter    = rgt and string.format("%04d", rgt) or '????'
    local cycle_filter  = cycle and string.format("%02d", cycle) or '??'
    local name_filter   = '*_' .. rgt_filter .. cycle_filter .. '??' .. '_*'

    -- build atl09 query parameters
    local atl09_parms = {
        asset = "icesat2-atl09",
        name_filter = name_filter
    }

    -- build time range
    if with_time_range then
        local year          = granule["year"] and parms["year"]
        local month         = granule["month"] or parms["month"]
        local day           = granule["day"] or parms["day"]
        local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", year, month, day)
        local rgps          = time.gmt2gps(rdate)
        local rdelta        = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
        atl09_parms["t0"]   = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
        atl09_parms["t1"]   = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))
    end

    -- make earthdata query
    local atl09_max_retries = 3
    local atl09_attempt = 1
    local atl09_resource = nil
    while true do
        userlog:alert(core.INFO, core.RTE_STATUS, string.format("Attempt %d of %d ATL09 CMR request: %s", atl09_attempt, atl09_max_retries, json.encode(atl09_parms)))
        local rc2, rsps2 = earthdata.search(atl09_parms)
        if rc2 == earthdata.SUCCESS then
            if #rsps2 == 1 then
                atl09_resource = rsps2[1]
                break -- success
            else
                userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("Invalid number of resources for ATL09 CMR request for %s: %d", json.encode(atl09_parms), #rsps2))
                break -- failure
            end
        else
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("Failed attempt %d to make ATL09 CMR request <%d>: %s", atl09_attempt, rc2, rsps2))
            atl09_attempt = atl09_attempt + 1
            if atl09_attempt > atl09_max_retries then
                userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("Failed to make ATL09 CMR request for %s... aborting!", json.encode(atl09_parms)))
                break -- failure
            end
        end
    end

    -- return atl09 resources
    return atl09_resource
end

-- Create ATL09 Atmospheric Sampler Runner --
local function create_atmo_runner (parms, userlog, with_time_range)
    local atl09_granule = find_atl09_granule(parms, userlog)
    userlog:alert(core.INFO, core.RTE_STATUS, string.format("Processing atmospheric data from %s", atl09_granule))
    local atl09h5 = h5coro.object("icesat2-atl09", atl09_granule)
    return icesat2.atmo(parms, atl09h5, with_time_range)
end

-- Exported Package --
return {
    find_atl09_granule = find_atl09_granule,
    create_atmo_runner = create_atmo_runner
}
