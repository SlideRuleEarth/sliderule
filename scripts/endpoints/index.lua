--
-- ENDPOINT:    /source/index
--
-- INPUT:       arg[1]
--              {
--                  "<index name>": { <index parameters>... } 
--              }
--
-- OUTPUT:      {
--                  "resources": ["<resource name>", ...]
--              }
--
-- NOTES:       1. Both the input and the output are json objects
--              2. The index parameters are specific to the index being requested
--
--

local json = require("json")
local parm = json.decode(arg[1])

-- build set of resources --
local resource_set = {}
for k,v in pairs(parm) do
    local index = core.getbyname(k)
    local resources = index:query(v)
    for _,resource in pairs(resources) do
        resource_set[resource] = true
    end
end

-- convert resource set into array --
local result = {resources={}}
for k,_ in pairs(resource_set) do
    table.insert(result["resources"], k)
end

print(json.encode(result))
return json.encode(result)
