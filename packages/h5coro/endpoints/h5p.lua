-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local parms     = json.decode(arg[1])
local asset     = core.getbyname(parms["asset"], true)
local resource  = parms["resource"]
local datasets  = parms["datasets"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local f = h5coro.file(asset, resource)
    if not f then
        local userlog = msg.publish(_rqst.rspq)
        userlog:alert(core.INFO, core.RTE_FAILURE, string.format("failed to open resource: %s", resource))
        return
    end
    f:read(datasets, _rqst.rspq)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "H5Coro File Read",
    description = "Read values from an HDF5 object using an H5Coro file reader",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary"},
    schema = {
        tags = "p-series, h5coro",
        request = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "asset": {
                        "type": "string",
                        "description": "Name of the registered asset to read from"
                    },
                    "resource": {
                        "type": "string",
                        "description": "URL of HDF5 file or object"
                    },
                    "datasets": {
                        "type": "array",
                        "description": "List of datasets to read from the HDF5 file",
                        "items": {
                            "type": "object",
                            "properties": {
                                "dataset": {
                                    "type": "string",
                                    "description": "Name of the dataset"
                                },
                                "valtype": {
                                    "type": "integer",
                                    "description": "Data type enumeration (RecordObject::valType_t)"
                                },
                                "col": {
                                    "type": "integer",
                                    "description": "Column number"
                                },
                                "startrow": {
                                    "type": "integer",
                                    "description": "Starting row number"
                                },
                                "numrows": {
                                    "type": "integer",
                                    "description": "Total number of rows to read"
                                },
                                "slice": {
                                    "type": "array",
                                    "description": "Array of [start, end] pairs for each dimension",
                                    "items": {
                                        "type": "array",
                                        "items": { "type": "integer" },
                                        "minItems": 2,
                                        "maxItems": 2
                                    }
                                }
                            },
                            "required": ["dataset"]
                        }
                    }
                },
                "required": ["asset", "resource", "datasets"]
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/h5dataset.json" }
                ],
                "description": "Stream of binary-encoded values read from the hdf5 file"
            }
        } ]]
    }
}