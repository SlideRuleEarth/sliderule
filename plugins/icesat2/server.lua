local console = require("console")

console.logger:config(core.WARNING)

server = pistache.endpoint(9081)
server:name("SlideRuleServer")
