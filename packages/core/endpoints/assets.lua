-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local earthdata = require("earth_data_query")

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    -- build directory of assets
    local directory = {}
    local assets = earthdata.load()
    for _,asset in pairs(assets) do
        local name, identity, driver, path, index, endpoint, status = asset:info()
        if status then
            directory[name] = {name=name, identity=identity, driver=driver, path=path, index=index, endpoint=endpoint}
        end
    end
    -- return response
    return json.encode({
        directory = directory,
        drivers = core.iodrivers(),
        rasters = geo.factories()
    })
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Assets",
    logging = core.DEBUG,
    description = "Lists available assets, drivers, and rasters",
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
                    "directory": {
                        "type": "object",
                        "description": "Map of data source name to its configuration",
                        "additionalProperties": {
                            "type": "object",
                            "properties": {
                                "name": { "type": "string" },
                                "driver": { "type": "string" },
                                "path": { "type": "string" },
                                "identity": { "type": "string" },
                                "endpoint": { "type": "string" },
                                "index": { "type": "string", "description": "Optional VRT or index file path" }
                            }
                        }
                    },
                    "drivers": {
                        "type": "array",
                        "description": "List of available driver types",
                        "items": { "type": "string" }
                    },
                    "rasters": {
                        "type": "array",
                        "description": "List of available raster source names",
                        "items": { "type": "string" }
                    }
                }
            }
        } ]]
    }
}