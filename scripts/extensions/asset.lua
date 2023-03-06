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
--------------------------------------------------------------------------------------
local function loadresource(asset, resource, attributes)

    if not asset then
        return false
    end

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
local function _loadindex(asset, file)

    -- check for no index
    if file == "nil" then -- special value representing no index
        return false
    end

    -- try to open index file
    local raw_index = csv.open(file)
    if not raw_index then
        sys.log(core.DEBUG, string.format("Attempting to find index file %s", file))
        -- look for index local to calling script (if not provided)
        local info = debug.getinfo(3,'S');
        local offset = string.find(info.source, "/[^/]*$")
        local root_path = info.source:sub(2,offset)
        local new_file = root_path..file
        raw_index = csv.open(new_file)
    end

    -- check if index file found and opened
    if not raw_index then
        sys.log(core.DEBUG, string.format("Unable to open index file %s", file))
        return false
    end

    -- load resource for each entry in index
    for _,line in ipairs(raw_index) do
        loadresource(asset, line["name"], line)
    end

end

--------------------------------------------------------------------------------------
-- loaddir
--
--  Creates Asset for each asset listed in the asset directory file.
--
--  file: name of .csv file with the following header row
--      asset,      format,     path,       index,      region,     endpoint
--------------------------------------------------------------------------------------
local function loaddir(file)

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
    local raw_directory = csv.open(file)
    if raw_directory then
        for _,line in ipairs(raw_directory) do
            directory[line["asset"]] = line
        end
    else
        sys.log(core.DEBUG, string.format("Unable to load asset directory: %s", file))
    end

    -- create asset for each entry in directory
    for k,v in pairs(directory) do
        assets[k] = core.asset(k, v["format"], v["path"], v["index"], v["region"], v["endpoint"]):name(k)
    end

    -- load index file for each asset in directory
    local offset = string.find(file, "/[^/]*$")
    local path_prefix = "./"
    if offset then -- pull out relative path of asset directory file
        path_prefix = file:sub(1, offset)
    end
    for k,v in pairs(directory) do
        sys.log(core.DEBUG, string.format("Building %s (%s) index at %s", k, v["format"], v["path"]))
        if v["index"] then
            if v["index"]:sub(1,1) == "/" then -- absolute path
                _loadindex(assets[k], v["index"])
            else -- relative path
                _loadindex(assets[k], path_prefix..v["index"])
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
