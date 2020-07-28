local console = require("console")
local asset = require("asset")

console.logger:config(core.INFO)

assets = asset.load()

if __pistache__ then
    server = pistache.endpoint(9081)
    server:name("SlideRuleServer")
else
    print("Must build pistache package to run server")
end