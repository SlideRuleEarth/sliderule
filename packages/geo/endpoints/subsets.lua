-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local parms     = json.decode(arg[1])
local extents   = parms["extents"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- Get Samples --
    local dem = geo.raster(geo.parms(parms[geo.PARMS]))

    -- Build Table --
    local subsets = {}
    local errors = {}
    if dem then
        for _, position in ipairs(extents) do
            local xmin = position[1]
            local ymin = position[2]
            local xmax = position[3]
            local ymax = position[4]
            local subset, error = dem:subset(xmin, ymin, xmax, ymax)
            table.insert(subsets, subset)
            table.insert(errors, error)
        end
    end

    -- Return Response
    return json.encode({subsets=subsets, errors=errors})

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Raster Subsetter",
    description = "Return a subset from a raster dataset given an extent",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "allOf": [
                    { "$ref": "#/components/schemas/GeoParameters" }
                ],
                "type": "object",
                "properties": {
                    "extents": {
                        "type": "array",
                        "description": "Array of [<xmin>, <ymin>, <xmax>, <ymax>] extents to subset",
                        "items": {
                            "type": "array",
                            "items": { "type": "number" },
                            "minItems": 4,
                            "maxItems": 4
                        }
                    }
                },
                "required": ["samples", "extents"]
            }
        } ]],
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "subsets": {
                        "type": "array",
                        "description": "Array of subset objects for each extent",
                        "items": {
                            "type": "object",
                            "properties": {
                                "file": {
                                    "type": "string",
                                    "description": "Name of the raster file that was subset"
                                },
                                "size": {
                                    "type": "integer",
                                    "description": "Size of the subset data in bytes"
                                },
                                "poolsize": {
                                    "type": "integer",
                                    "description": "Current memory pool size in bytes"
                                }
                            },
                            "required": ["file", "size", "poolsize"]
                        }
                    },
                    "errors": {
                        "type": "array",
                        "description": "Error codes for each coordinate (0 indicates no error)",
                        "items": { "type": "integer" }
                    }
                }
            }
        } ]]
    }
}