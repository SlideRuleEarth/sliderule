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
local assets_file = cfgtbl["assets"] or nil

-- Configure Logging --
console.logger:config(loglvl)

-- Configure Assets --
assets = asset.load(assets_file)

-- Configure and Run Server --
if __pistache__ then
    server = pistache.endpoint(port)
    server:name("SlideRuleServer")
else
    print("Must build pistache package to run server")
end