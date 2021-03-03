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

-- Configure Logging --
console.logger:config(loglvl)

-- Configure Assets --
assets = asset.loaddir(asset_directory, true)

-- Configure and Run Server --
server = core.httpd(9081)
server:name("HttpServer")
endpoint = core.endpoint()
endpoint:name("LuaEndpoint")
server:attach(endpoint, "/source")
