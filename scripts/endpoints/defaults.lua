--
-- ENDPOINT:    /source/defaults
--
-- INPUT:       none
-- OUTPUT:      json string of default parameters
--
local global = require("global")

local default_parms = {}

local function insert_package_defaults(tbl, pkg)
    if pkg ~= "legacy" then
        local key = global.eval("PARMS", pkg)
        if key then
            local value = global.eval("parms({})", pkg):tojson()
            local json_str = string.format("\"%s\":%s", key, value)
            table.insert(tbl, json_str)
        end
    end
end

local version, commit, environment, launch, duration, packages = sys.version()
for _,package in ipairs(packages) do
    insert_package_defaults(default_parms, package)
end

local json_str = "{"..table.concat(default_parms, ",").."}"
return json_str

