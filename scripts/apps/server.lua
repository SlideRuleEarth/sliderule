local console = require("console")
local asset = require("asset")

console.logger:config(core.INFO)

assets = asset.load()

server = pistache.endpoint(9081)
server:name("SlideRuleServer")
