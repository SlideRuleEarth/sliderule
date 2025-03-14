--
-- ENDPOINT:    /source/defaults
--
-- INPUT:       none
--
-- OUTPUT:      json string of default parameters
--
local json = require("json")

local default_parms = {
    core = core.parms():export(),
    cre =  cre.parms():export(),
    icesat2 = __icesat2__ and icesat2.parms():export() or nil,
    gedi = __gedi__ and gedi.parms():export() or nil,
    swot = __swot__ and swot.parms():export() or nil,
    bathy = __bathy__ and bathy.parms():export() or nil
}

return json.encode(default_parms)

