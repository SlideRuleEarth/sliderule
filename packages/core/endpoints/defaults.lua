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
    parms = nil,
    name = "Defaults",
    description = "Dump of all system request parameters and their default values",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    inputs = nil,
    outputs = {"json"},
    schema = {
        request = nil,
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "core": { "$ref": "#/components/schemas/CoreParameters" },
                    "cre": { "$ref": "#/components/schemas/CreParameters" },
                    "icesat2": { "$ref": "#/components/schemas/Icesat2Parameters" },
                    "gedi": { "$ref": "#/components/schemas/GediParameters" },
                    "swot": { "$ref": "#/components/schemas/SwotParameters" },
                    "bathy": { "$ref": "#/components/schemas/BathyParameters" }
                }
            }
        } ]]
    }
}