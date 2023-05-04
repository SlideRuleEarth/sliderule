--
-- EXTENSION:   georesource.lua
--
-- PURPOSE:     generate customized goelocated data products
--
-- INPUT:       rqst
--              {
--                  "resource":     "<url of hdf5 file or object>"
--                  "parms":        {<table of parameters>}
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      algorithm results
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized RecordObjects
--

local json = require("json")

--
-- Initialize Processing of Resource
--
local function initialize(resource, parms, algo, args)

    -- User Status --
    local userlog = msg.publish(rspq)

    -- Get Asset --
    local asset_name = parms["asset"] or args.default_asset
    local asset = core.getbyname(asset_name)
    if not asset then
        userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", asset_name))
        do return nil end
    end

    -- Raster Sampler --
    local sampler_disp = nil
    if parms[geo.PARMS] then
        rsps_bridge = core.bridge(args.result_q, rspq)
        sampler_disp = core.dispatcher(args.result_q, 1) -- 1 thread required because GeoRaster is not thread safe
        for key,settings in pairs(parms[geo.PARMS]) do
            local robj = geo.raster(geo.parms(settings):keyspace(args.shard))
            if robj then
                local sampler = geo.sampler(robj, key, rspq, args.result_batch, args.index_field, args.lon_field, args.lat_field)
                if sampler then
                    sampler_disp:attach(sampler, args.result_rec)
                else
                    userlog:sendlog(core.CRITICAL, string.format("request <%s> failed to create sampler %s for %s", rspq, key, resource))
                end
            else
                userlog:sendlog(core.CRITICAL, string.format("request <%s> failed to create raster %s for %s", rspq, key, resource))
            end
        end
        sampler_disp:run()
    end

    -- Dispatcher --
    local source_q = nil
    local algo_disp = nil
    if algo then
        source_q = resource .. "." .. rspq
        algo_disp = core.dispatcher(source_q)

        -- Attach Exception and Ancillary Record Forwarding --
        local except_pub = core.publish(rspq)
        algo_disp:attach(except_pub, "exceptrec") -- exception records
        algo_disp:attach(except_pub, "extrec") -- ancillary records

        -- Attach Algorithm --
        algo_disp:attach(algo, args.source_rec)

        -- Run Dispatcher --
        algo_disp:run()
    end

    -- Post Initial Status Progress --
    userlog:sendlog(core.INFO, string.format("request <%s> processing initialized on %s ...", rspq, resource))

    -- Return Needed Objects to Continue Processing Request --
    return {asset=asset, source_q=source_q, algo_disp=algo_disp, sampler_disp=sampler_disp, userlog=userlog}
end

--
-- Wait On Processing of Resource
--
local function waiton(resource, parms, algo, reader, algo_disp, sampler_disp, userlog, with_stats)

    -- Initialize Timeouts --
    local timeout = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds

    -- Wait Until Reader Completion --
    while (userlog:numsubs() > 0) and not reader:waiton(interval * 1000) do
        duration = duration + interval
        -- Check for Timeout --
        if timeout >= 0 and duration >= timeout then
            userlog:sendlog(core.ERROR, string.format("request <%s> for %s timed-out after %d seconds", rspq, resource, duration))
            do return false end
        end
        userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to read %s (after %d seconds)", rspq, resource, duration))
    end

    -- Resource Processing Complete --
    if with_stats and reader then
        local reader_stats = reader:stats(false)
        userlog:sendlog(core.INFO, string.format("request <%s> processing of %s complete (%d/%d/%d)", rspq, resource, reader_stats.read, reader_stats.filtered, reader_stats.dropped))
    end

    -- Wait Until Algorithm Dispatch Completion --
    if algo_disp then
        while (userlog:numsubs() > 0) and not algo_disp:waiton(interval * 1000) do
            duration = duration + interval
            -- Check for Timeout --
            if timeout >= 0 and duration >= timeout then
                userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
                do return false end
            end
            userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to process source records (after %d seconds)", rspq, duration))
        end
    end

    -- Wait Until Sampler Dispatch Completion --
    if sampler_disp then
        sampler_disp:aot() -- aborts on next timeout
        while (userlog:numsubs() > 0) and not sampler_disp:waiton(interval * 1000) do
            duration = duration + interval
            -- Check for Timeout --
            if timeout >= 0 and duration >= timeout then
                userlog:sendlog(core.ERROR, string.format("request <%s> timed-out after %d seconds", rspq, duration))
                do return false end
            end
            userlog:sendlog(core.INFO, string.format("request <%s> ... continuing to sample records (after %d seconds)", rspq, duration))
        end
    end

    -- Request Processing Complete --
    if with_stats and algo then
        local algo_stats = algo:stats(false)
        userlog:sendlog(core.INFO, string.format("request <%s> processing complete (%d/%d/%d/%d)", rspq, algo_stats.read, algo_stats.filtered, algo_stats.sent, algo_stats.dropped))
    end

    -- Return Success --
    return true
end

local package = {
    initialize = initialize,
    waiton = waiton
}

return package