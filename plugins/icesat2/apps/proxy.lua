local console = require("console")
local json = require("json")

-- Process Arguments: JSON Configuration File
json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s\n', json_input))
end

-- Process Arguments: Proxy Port Number
port = tonumber(arg[2]) or 35100

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
local ports = cfgtbl["proxy_ports"]
local cache_root = cfgtbl["cache_root"]
local cache_size = cfgtbl["cache_size"]

-- Configure Logging --
console.logger:config(loglvl)

-- Configure S3 Cache --
if cache_root then
    aws.s3cache(cache_root, cache_size)
end

-- Configure and Run Proxy --
proxy = h5.proxy(port)
proxy:name("h5proxy")
