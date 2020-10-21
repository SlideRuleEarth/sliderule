--
-- ENDPOINT:    /source/index
--
-- INPUT:       arg[1]
--              {
--                  "or"|"and": 
--                  {
--                      "<index name>": { <index parameters>... } 
--                      ...
--                  }
--              }
--
-- OUTPUT:      {
--                  "resources": ["<resource name>", ...]
--              }
--
-- NOTES:       1. Both the input and the output are json objects
--              2. The index parameters are specific to the index being requested
--              3. Multiple indexes will perform the "operation" of the output from each
--
--

local json = require("json")
local parm = json.decode(arg[1])
local result = {resources={}}

local function build_resource_array(index_set, union)

    -- check for operation --
    if index_set["or"] then
        build_resource_array(index_set["or"], true)
        return
    elseif index_set["and"] then
        build_resource_array(index_set["and"], false)
        return
    end

    -- build set of resources --
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
            print(k, resource, resource_set[resource])
        end
    end

    -- convert resource set into array --
    if union then
        for k,_ in pairs(resource_set) do
            table.insert(result["resources"], k)
        end
    else -- intersection
        local num_indexes = 0
        for _ in pairs(index_set) do num_indexes = num_indexes + 1 end
        for k,v in pairs(resource_set) do
            print(k, v)
            if v == num_indexes then         
                table.insert(result["resources"], k)
            end
        end
    end

end

-- build resource array --
build_resource_array(parm, true)

-- return results --
return json.encode(result)
