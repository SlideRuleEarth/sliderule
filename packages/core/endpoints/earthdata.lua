-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local earthdata = require("earth_data_query")
local parms     = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local status,rsps = earthdata.search(parms)
    if status == earthdata.SUCCESS then
        return json.encode(rsps), true
    else
        local err_rsps = json.encode({error=rsps, code=status})
        sys.log(core.CRITICAL, "Failed earthdata request: " .. err_rsps)
        return err_rsps, false
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Earthdata",
    description = "Query earth data assets for resources to process",
    logging = core.INFO,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "allOf": [
                    {
                        "$ref": "../components/schemas/CoreParameters.json"
                    },
                    {
                        "type": "object",
                        "properties": {
                            "short_name": { "type": "string", "description": "CMR short name of the dataset" },
                            "version": { "type": "string", "description": "Version of the dataset" },
                            "collection": { "type": "string", "description": "Collection name of the dataset" },
                            "name_filter": { "type": "string", "description": "Regular expression to match granule name" },
                            "rgt": { "type": "integer", "description": "Reference ground track" },
                            "cycle": { "type": "integer", "description": "Orbit cycle number" },
                            "region": { "type": "integer", "description": "Global region" }
                        }
                    }
                ]
            }
        } ]],
        response = [[ "application/json": {
            "schema": {
                "type": "array",
                "items": {
                    "type": "string"
                },
                "description": "List of resources to be processed"
            }
        } ]]
    }
}
