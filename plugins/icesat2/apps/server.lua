local console = require("console")
local asset = require("asset")
local json = require("json")

-- Process Arguments: JSON Configuration File
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s\n', json_input))
    local f = io.open(json_input, "r")
    local content = f:read("*all")
    f:close()
    cfgtbl = json.decode(content)
end

-- Pull Out Parameters --
local loglvl = cfgtbl["loglvl"] or core.INFO
local port = cfgtbl["server_port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil
local cache_root = cfgtbl["cache_root"]
local cache_size = cfgtbl["cache_size"]

-- Configure Logging --
console.logger:config(loglvl)

-- Configure Assets --
assets = asset.loaddir(asset_directory)

timeindex = core.intervalindex(assets["atl03-cloud"], "t0", "t1")
timeindex:name("timeindex")

southindex = core.spatialindex(assets["atl03-cloud"], core.SOUTH_POLAR)
southindex:name("southindex")

northindex = core.spatialindex(assets["atl03-cloud"], core.NORTH_POLAR)
northindex:name("northindex")

cycleindex = core.pointindex(assets["atl03-cloud"], "cycle")
cycleindex:name("cycleindex")

rgtindex = core.pointindex(assets["atl03-cloud"], "rgt")
rgtindex:name("rgtindex")

-- Configure S3 Cache --
if cache_root then
    aws.s3cache(cache_root, cache_size)
end

-- Configure and Run Server --
server = core.httpd(9081)
server:name("HttpServer")
endpoint = core.endpoint()
endpoint:name("LuaEndpoint")
server:attach(endpoint, "/source")
