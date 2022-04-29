local global = require("global")
local console = require("console")
local asset = require("asset")
local json = require("json")

-- Process Arguments: JSON Configuration File
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.INFO, string.format('Reading json file: %s\n', json_input))
    local f = io.open(json_input, "r")
    if f ~= nil then
        local content = f:read("*all")
        f:close()
        cfgtbl = json.decode(content)
    end
end

-- Pull Out Parameters --
local loglvl = global.eval(cfgtbl["loglvl"]) or core.ERROR
local port = cfgtbl["port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil
local cache_root = cfgtbl["cache_root"]
local cache_size = cfgtbl["cache_size"]

-- Configure Logging --
sys.setlvl(core.LOG, loglvl)

-- Configure Assets --
assets = asset.loaddir(asset_directory)

-- Configure S3 Cache --
if __aws__ then
    if cache_root then
        aws.s3cache(cache_root, cache_size)
    end
end

-- Configure and Run Server --
server = core.httpd(port)
server:name("HttpServer")
endpoint = core.endpoint()
endpoint:name("LuaEndpoint")
server:attach(endpoint, "/source")
