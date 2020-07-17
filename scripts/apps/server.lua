local console = require("console")
local asset = require("asset")

console.logger:config(core.INFO)

asset_directory = asset.load("asset_directory.csv")
assets = asset.index(asset_directory)

server = pistache.endpoint(9081)
server:name("SlideRuleServer")
