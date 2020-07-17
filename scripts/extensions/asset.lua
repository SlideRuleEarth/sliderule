csv = require("csv")

--------------------------------------------------------------------------------------
-- load  -
--------------------------------------------------------------------------------------
local function load(file, quiet)

    local assets = {}

    if not file then -- look for it local to calling script
        local info = debug.getinfo(1,'S');
        local path_index = string.find(info.source, "/[^/]*$")
        local root_path = info.source:sub(2,path_index)
        file = root_path.."asset_directory.csv" -- by convention
    end

    local directory = {}
    local raw_directory = csv.open(file, {header=true})
    if raw_directory then
        for fields in raw_directory:lines() do
            directory[fields["asset"]] = fields
        end
    else
        print(string.format("Unable to load asset directory: %s", full_path))
    end

    for k,v in pairs(directory) do
        if(not quiet) then
            print(string.format("Building %s (%s) index at %s", k, v["format"], v["url"]))
        end
        assets[k] = core.asset(k, v["format"], v["url"], v["index"])
    end

    return assets
end

--------------------------------------------------------------------------------------
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    load = load,
}

return package
