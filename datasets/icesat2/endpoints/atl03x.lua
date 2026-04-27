-------------------------------------------------------
-- initialization
-------------------------------------------------------
local dataframe = require("dataframe")
local json      = require("json")
local rqst      = json.decode(arg[1])
local parms     = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local channels  = 6 -- number of dataframes per resource

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    dataframe.proxy("atl03x", parms, rqst["parms"], _rqst.rspq, channels, function(userlog)
        local dataframes = {}
        local runners = {}
        local resource = parms["resource"]

        -- custom algorithms
        if parms:stage(icesat2.FITTER) then -- surface fitter
            local fitter = icesat2.fit(parms)
            table.insert(runners, fitter)
        elseif parms:stage(icesat2.PHOREAL) then -- phoreal
            local phoreal = icesat2.phoreal(parms)
            table.insert(runners, phoreal)
        elseif parms:stage(icesat2.BLANKET) then -- surface blanket
            local blanket = icesat2.blanket(parms)
            table.insert(runners, blanket)
        end

        -- atl08
        local atl08h5 = nil
        if parms:stage(icesat2.ATL08) then
            atl08h5 = h5coro.object(parms["asset"], resource:gsub("ATL03", "ATL08"))
        end

        -- atl09
        if parms:stage(icesat2.ATL09) then
            local utils = require("icesat2_utils")
            if not utils.add_atmo_runner(runners, parms, userlog) then
                return {}, {} -- unable to process this resource
            end
        end

        -- atl24
        local atl24h5 = nil
        if parms:stage(icesat2.ATL24) then
            local atl24_filename = resource:gsub("ATL03", "ATL24"):gsub(".h5", "_002_01.h5")
            atl24h5 = h5coro.object("icesat2-atl24", atl24_filename)
        end

        -- atl03x
        local atl03h5 = h5coro.object(parms["asset"], resource)
        for _, beam in ipairs(parms["beams"]) do
            dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, atl24h5, _rqst.rspq)
            if not dataframes[beam] then
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> on %s failed to create dataframe for beam %s", _rqst.id, resource, beam))
            end
        end

        return dataframes, runners
    end)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL03 Dataframe",
    description = "Spatially and temporally subsets ATL03 photon cloud with additional filters and ancillary fields (x-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}
