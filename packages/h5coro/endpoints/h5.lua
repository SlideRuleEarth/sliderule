-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local parms     = json.decode(arg[1])
local asset     = core.getbyname(parms["asset"], true)
local resource  = parms["resource"]
local dataset   = parms["dataset"]
local datatype  = parms["datatype"] or core.DYNAMIC
local col       = parms["col"] or 0
local startrow  = parms["startrow"] or 0
local numrows   = parms["numrows"] or h5coro.ALL_ROWS
local id        = parms["id"] or 0

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local f = h5coro.dataset(streaming.READER, asset, resource, dataset, id, false, datatype, col, startrow, numrows)
    if f:connected() then
        local r = streaming.reader(f, _rqst.rspq)
        r:waiton() -- waits until reader completes
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "H5Coro Dataset Read",
    description = "Read values from an HDF5 object using an H5Coro streaming dataset reader",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary"},
    schema = {
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
                    "dataset": {
                        "type": "string",
                        "description": "Name of the dataset within the HDF5 file"
                    },
                    "datatype": {
                        "type": "integer",
                        "description": "Data type enumeration (RecordObject::valType_t); defaults to DYNAMIC"
                    },
                    "col": {
                        "type": "integer",
                        "description": "Column to read from a multi-dimensional dataset",
                        "default": 0
                    },
                    "startrow": {
                        "type": "integer",
                        "description": "Starting row for the read",
                        "default": 0
                    },
                    "numrows": {
                        "type": "integer",
                        "description": "Number of rows to read; defaults to all rows"
                    },
                    "id": {
                        "type": "integer",
                        "description": "Integer ID to attach to the returned data",
                        "default": 0
                    }
                },
                "required": ["asset", "resource", "dataset"]
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "#/components/schemas/h5dataset" },
                ],
                "description": "Stream of binary-encoded values read from the hdf5 file"
            }
        } ]]
    }
}
