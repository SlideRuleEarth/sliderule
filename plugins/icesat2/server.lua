local console = require("console")

console.logger:config(core.INFO)

server = pistache.endpoint(9081)
server:name("SlideRuleServer")
