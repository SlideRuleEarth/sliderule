csv = require("csv")

--------------------------------------------------------------------------------------
-- load  -
--------------------------------------------------------------------------------------
local function load(file)

    local directory = {}

    local full_path = file
    if full_path:sub(1,1) ~= '/' then
        local info = debug.getinfo(1,'S');
        local path_index = string.find(info.source, "/[^/]*$")
        local root_path = info.source:sub(2,path_index)
        full_path = root_path..file
    end

    local raw_directory = csv.open(full_path, {header=true})
    if raw_directory then
        for fields in raw_directory:lines() do
            directory[fields["asset"]] = fields
        end
    else
        print(string.format("Unable to load asset directory: %s", file))
    end

    return directory
end

--------------------------------------------------------------------------------------
-- index  -
--------------------------------------------------------------------------------------
local function index(asset_directory)

    local assets = {}

    for k,v in pairs(asset_directory) do
        print(string.format("Building %s (%s) index at %s", k, v["format"], v["url"]))
--        assets[k] = core.index(k, v["format"], v["url"], v["index"])
    end

    return assets
end

--------------------------------------------------------------------------------------
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    load = load,
    index = index
}

return package
