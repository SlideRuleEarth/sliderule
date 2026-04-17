-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    return json.encode({
        core = core.parms():export(),
        cre =  cre.parms():export(),
        icesat2 = __icesat2__ and icesat2.parms():export() or nil,
        gedi = __gedi__ and gedi.parms():export() or nil,
        swot = __swot__ and swot.parms():export() or nil,
        bathy = __bathy__ and bathy.parms():export() or nil
    })
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Defaults",
    description = "Dump of all system request parameters and their default values",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}