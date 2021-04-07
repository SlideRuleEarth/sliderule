--
-- ENDPOINT:    /source/time
--
-- INPUT:       None
--
-- OUTPUT:      {
--                  "server":
--                  {
--                      "version": "1.0.1",
--                      "commit": "v1.0.1-4-g34f2782",
--                      "launch date": "2021:92:20:14:33"
--                  },
--                  "docker": "icesat2sliderule/sliderule:v1.0.2",
--                  "plugins": ["icesat2"],
--                  "icesat2":
--                  {
--                      "version": "1.0.1",
--                      "commit": "v1.0.1-15-ga52359e",
--                  }
--              }
--
-- NOTES:       The output is a json object
--

local json = require("json")
local parm = json.decode(arg[1])

version, commit = sys.version()

rsps = {server={version=version, commit=commit}}

return json.encode(rsps)
