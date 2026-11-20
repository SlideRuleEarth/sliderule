-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local def = msg.definition(parms["rectype"])
    return json.encode(def)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Record Definition",
    description = "Defines the on-the-wire format and structure for a given record type",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        tags = "a-series, core",
        request = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "rectype": { "type": "string" }
                }
            }
        } ]],
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "__datasize": {
                        "type": "integer",
                        "description": "Total size of the record in bytes"
                    },
                    "offset": {
                        "type": "integer",
                        "description": "Bit offset of the field within the record"
                    },
                    "flags": {
                        "type": "string",
                        "description": "Encoding flags, e.g. endianness",
                        "enum": ["LE", "BE"]
                    },
                    "elements": {
                        "type": "integer",
                        "description": "Number of elements in the field"
                    },
                    "type": {
                        "type": "string",
                        "description": "Data type of the field",
                        "enum": ["INT8", "UINT8", "INT16", "UINT16", "INT32", "UINT32", "INT64", "UINT64", "FLOAT", "DOUBLE", "STRING"]
                    }
                }
            }
        } ]]
    }
}
