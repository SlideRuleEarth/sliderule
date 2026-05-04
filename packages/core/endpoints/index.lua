-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- initialize variables
    local result = {resources={}}

    -- recursive function
    local function build_resource_array(index_set, union)

        -- check for operation
        if index_set["or"] then
            build_resource_array(index_set["or"], true)
            return
        elseif index_set["and"] then
            build_resource_array(index_set["and"], false)
            return
        end

        -- build set of resources
        local resource_set = {}
        for k,v in pairs(index_set) do
            local index = core.getbyname(k)
            local resources = index:query(v)
            for _,resource in pairs(resources) do
                if resource_set[resource] then
                    resource_set[resource] = resource_set[resource] + 1
                else
                    resource_set[resource] = 1
                end
            end
        end

        -- convert resource set into array
        if union then
            for k,_ in pairs(resource_set) do
                table.insert(result["resources"], k)
            end
        else -- intersection
            local num_indexes = 0
            for _ in pairs(index_set) do num_indexes = num_indexes + 1 end
            for k,v in pairs(resource_set) do
                if v == num_indexes then
                    table.insert(result["resources"], k)
                end
            end
        end

    end

    -- build resource array
    build_resource_array(parms, true)

    -- return results
    return json.encode(result)

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Geospatial Index",
    description = "Query internal geospatial indexes for resources to process",
    logging = core.INFO,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "oneOf": [
                    {
                        "type": "object",
                        "properties": {
                            "or": {
                                "type": "object",
                                "description": "Union of queries",
                                "additionalProperties": { "type": "string", "description": "Query command keyed by index name" }
                            }
                        },
                        "required": ["or"]
                    },
                    {
                        "type": "object",
                        "properties": {
                            "and": {
                                "type": "object",
                                "description": "Intersection of queries",
                                "additionalProperties": { "type": "string", "description": "Query command keyed by index name" }
                            }
                        },
                        "required": ["and"]
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
                "description": "List of resources that to process"
            }
        } ]]

    }
}