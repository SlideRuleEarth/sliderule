local console = require("console")
local asset = require("asset")
local json = require("json")

-- Process Arguments: JSON Configuration File
json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s\n', json_input))
end

-- Read JSON File and Decode Content --
local cfgtbl = {}
if json_input then
    local f = io.open(json_input, "r")
    local content = f:read("*all")
    f:close()
    cfgtbl = json.decode(content)
end

-- Pull Out Parameters --
local loglvl = cfgtbl["loglvl"] or core.INFO
local port = cfgtbl["port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil
local cache_root = cfgtbl["cache_root"]
local cache_size = cfgtbl["cache_size"]

-- Configure Logging --
console.logger:config(loglvl)

-- Configure Assets --
assets = asset.load(asset_directory)

-- Configure S3 Cache --
if __aws__ then
    if cache_root then
        aws.s3cache(cache_root, cache_size)
    end
end

-- Configure and Run Server --
if __mongoose__ then
    server = mongoose.server(tostring(port), 4)
    server:name("SlideRuleMongoose")
elseif __pistache__ then
    server = pistache.server(port, 4)
    server:name("SlideRulePistache")
else
    server = core.httpd(9081)
    server:name("HttpServer")
    endpoint = core.endpoint()
    endpoint:name("LuaEndpoint")
    server:attach(endpoint, "/source")
end