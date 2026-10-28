local runner = require("test_executive")
local json = require("json")

-- Setup --

runner.authenticate({'nsidc-cloud'})
local consoleq = msg.subscribe("consoleq") -- prevents error posting to consoleq

-- Self Test --

runner.unittest("ATL24 HDF5 Writer", function()

    -- create objects used in processing granule
    local resource      = "ATL03_20241107234251_08052501_006_01.h5"
    local timeout       = 600 * 1000
    local rqst          = {beams={"gt2r"}}
    local parms         = bathy.parms(rqst, nil, "icesat2", resource)
    local bathymask     = bathy.mask()
    local atl03h5       = h5coro.object(parms["asset"], resource)
    local granule       = icesat2.atl03granule(parms, atl03h5, "consoleq")
    local classifier    = atl24.classifier(parms)
    local refractor     = bathy.refraction(parms)
    local uncertainty   = atl24.uncertainty(parms)
    local sender        = core.framesender(parms, "rspq", resource)
    local dataframe     = core.dataframe({}, {granule=resource, request=json.encode(rqst)})
    local dataframes    = {} -- holds beam dataframes

    -- build final dataframe from beam dataframes
    dataframe:receive("rspq", "consoleq", 6, timeout)
    for _, beam in ipairs(parms["beams"]) do
        local df = bathy.dataframe(beam, parms, bathymask, atl03h5, "consoleq")
        runner.assert(df, string.format("failed to create dataframe for beam %s", beam), true)
        df:run(classifier)
        df:run(refractor)
        df:run(uncertainty)
        df:run(sender)
        df:run(core.TERMINATE)
        dataframes[beam] = df
    end

    -- wait for data to finish being read and deduplicated
    for beam, df in pairs(dataframes) do
        sys.log(core.CRITICAL, string.format("waiting for beam %s", beam))
        local status = df:finished(timeout)
        runner.assert(status, string.format("failed to finish dataframe for beam %s", beam))
    end

    -- send termination signal to final dataframe
    local rspq = msg.publish("rspq")
    rspq:sendstring("")

    -- wait for final dataframe (blocks until dataframe complete or timeout)
    runner.assert(dataframe:waiton(timeout), "failed to receive proxied dataframe", true)

    -- check dataFrame constraints
    sys.log(core.CRITICAL, string.format("constructed final dataframe with %d rows, and %d columns", dataframe:numrows(), dataframe:numcols()))
    runner.assert(dataframe:numrows() > 0 and dataframe:numcols() > 0, "produced an empty dataframe", true)

    -- write h5 file
    local atl24_file = atl24.writer(parms, dataframes, granule, "X")
    runner.assert(atl24_file:write("/tmp/atl24.h5"))

end)

-- Report Results --

runner.report()
