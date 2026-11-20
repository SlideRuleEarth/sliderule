-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    return json.encode({
        core = core.parms():export(),
        cre =  cre.parms():export(),
        icesat2 = __icesat2__ and icesat2.parms03():export() or nil,
        gedi = __gedi__ and gedi.parmsl4():export() or nil,
        swot = __swot__ and swot.parms():export() or nil,
        bathy = __bathy__ and bathy.parms():export() or nil,
        atl03 = __icesat2__ and icesat2.parms03():export() or nil,
        atl06 = __icesat2__ and icesat2.parms06():export() or nil,
        atl06sr = __icesat2__ and icesat2.parms06d():export() or nil,
        atl13 = __icesat2__ and icesat2.parms13():export() or nil,
        atl24 = __icesat2__ and icesat2.parms24():export() or nil
    })
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Defaults",
    description = "Dump of all system request parameters and their default values",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    inputs = nil,
    outputs = {"json"},
    schema = {
        tags = "a-series, core",
        request = nil,
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "core": { "$ref": "../components/schemas/RequestParameters.json" },
                    "cre": { "$ref": "../components/schemas/CreParameters.json" },
                    "icesat2": { "$ref": "../components/schemas/Icesat2Parameters.json" },
                    "gedi": { "$ref": "../components/schemas/GediParameters.json" },
                    "swot": { "$ref": "../components/schemas/SwotParameters.json" },
                    "bathy": { "$ref": "../components/schemas/BathyParameters.json" }
                }
            }
        } ]]
    }
}