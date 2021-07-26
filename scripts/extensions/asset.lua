csv = require("csv")

--------------------------------------------------------------------------------------
-- loadresource
--
--  Populates a resource in an Asset
--
--  name: name of asset
--
--  resource: name of resource
--
--  attributes: table of resource attributes
--      "t0": start time
--      "t1": stop time
--      "lat0": southern latitude (N: 90, S: -90)
--      "lat1": northern latitude (N: 90, S: -90)
--      "lon0": western longitutde (E: 180, W: -180)
--      "lon1": eastern longitutde (E: 180, W: -180)
--
--  quiet: boolean whether to print a message for each resource loaded [optional]
--------------------------------------------------------------------------------------
local function loadresource(asset, resource, attributes, quiet)

    if not asset then
        return false
    end

--    if not attributes["t0"]     then attributes["t0"]   = 0.0 end
--    if not attributes["t1"]     then attributes["t1"]   = 0.0 end
--    if not attributes["lat0"]   then attributes["lat0"] = 0.0 end
--    if not attributes["lon0"]   then attributes["lon0"] = 0.0 end
--    if not attributes["lat1"]   then attributes["lat1"] = 0.0 end
--    if not attributes["lon1"]   then attributes["lon1"] = 0.0 end
--
--    if not quiet then
--        print(string.format("Loading resource %s in asset %s: time <%s, %s>, spatial <%s, %s, %s, %s>",
--            resource, asset:name(), attributes["t0"], attributes["t1"],
--            attributes["lat0"], attributes["lon0"], attributes["lat1"], attributes["lon1"]))
--    end

    return asset:load(resource, attributes)

end

--------------------------------------------------------------------------------------
-- loadindex
--
--  Loads resource for each resource listed in the asset index file.
--
--  file: name of .csv file with the following header row
--      resource,   t0, t1,     lat0, lat1, lon0, lon1,     [attr1, attr2, ..., attrN]
--
--  quiet: boolean whether to print a message for each asset loaded [optional]
--------------------------------------------------------------------------------------
local function _loadindex(asset, file, quiet)

    -- try to open index file
    local raw_index = csv.open(file, {header=true})
    if not raw_index then
        if not quiet then
            print(string.format("Attempting to find index file %s", file))
        end
        -- look for index local to calling script (if not provided)
        local info = debug.getinfo(3,'S');
        local offset = string.find(info.source, "/[^/]*$")
        local root_path = info.source:sub(2,offset)
        local new_file = root_path..file
        raw_index = csv.open(new_file, {header=true})
    end

    -- check if index file found and opened
    if not raw_index then
        if not quiet then
            print(string.format("Unable to open index file %s", file))
        end
        return false
    end

    -- load resource for each entry in index
    for fields in raw_index:lines() do
        loadresource(asset, fields["name"], fields, quiet)
    end

end

--------------------------------------------------------------------------------------
-- loaddir
--
--  Creates Asset for each asset listed in the asset directory file.
--
--  file: name of .csv file with the following header row
--      asset,      format,     path,       index,      region,     endpoint
--
--  quiet: boolean whether to print a message for each asset loaded [optional]
--------------------------------------------------------------------------------------
local function loaddir(file, quiet)

    local assets = {}

    -- if not provided then look for asset directory local to calling script
    if not file then
        local info = debug.getinfo(2,'S')
        local offset = string.find(info.source, "/[^/]*$")
        local root_path = info.source:sub(2,offset)
        file = root_path.."asset_directory.csv" -- by convention
    -- if not an absolute path then look for file in the configuration directory
    elseif file:find("/") ~= 1 then
        file = __confdir.."/"..file
    end

    -- build directory from csv file
    local directory = {}
    local raw_directory = csv.open(file, {header=true})
    if raw_directory then
        for fields in raw_directory:lines() do
            directory[fields["asset"]] = fields
        end
    else
        print(string.format("Unable to load asset directory: %s", file))
    end

    -- create asset for each entry in directory
    for k,v in pairs(directory) do
        assets[k] = core.asset(k, v["format"], v["path"], v["index"], v["region"], v["endpoint"])
        assets[k]:name(k)
    end

    -- load index file for each asset in directory
    local offset = string.find(file, "/[^/]*$")
    local path_prefix = "./"
    if offset then -- pull out relative path of asset directory file
        path_prefix = file:sub(1, offset)
    end
    for k,v in pairs(directory) do
        if(not quiet) then
            print(string.format("Building %s (%s) index at %s", k, v["format"], v["path"]))
        end
        if v["index"] then
            if v["index"]:sub(1,1) == "/" then -- absolute path
                _loadindex(assets[k], v["index"], quiet)
            else -- relative path
                _loadindex(assets[k], path_prefix..v["index"], quiet)
            end
        end
    end

    return assets
end

--------------------------------------------------------------------------------------
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    loadresource = loadresource,
    loaddir = loaddir
}

return package
