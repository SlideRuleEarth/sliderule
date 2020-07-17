local console = require("console")
local asset = require("asset")

console.logger:config(core.INFO)

assets = asset.load("asset_directory.csv")
for k,v in pairs(assets) do
    id = string.format("%s (%s)", k, v["format"])
    print(string.format("%s: %s", id, v["url"]))
end

server = pistache.endpoint(9081)
server:name("SlideRuleServer")
