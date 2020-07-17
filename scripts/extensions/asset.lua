csv = require("csv")

--------------------------------------------------------------------------------------
-- load  -
--------------------------------------------------------------------------------------
local function load(file)

    local directory = {}

    local info = debug.getinfo(1,'S');
    local path_index = string.find(info.source, "/[^/]*$")
    local path = info.source:sub(2,path_index)

    local raw_directory = csv.open(path..file, {header=true})
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
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    load = load
}

return package
