--
-- ENDPOINT:    /source/defaults
--
-- INPUT:       none
-- OUTPUT:      <default parameters>
--

local json = require("json")

local default_parms = {}

if __arrow__ then
    default_parms[arrow.PARMS] = arrow.parms({}):tojson()
end

if __cre__ then
    default_parms[cre.PARMS] = cre.parms({}):tojson()
end

if __geo__ then
    default_parms[geo.PARMS] = geo.parms({}):tojson()
end

if __netsvc__ then
    default_parms[netsvc.PARMS] = netsvc.parms({}):tojson()
end

if __swot__ then
    default_parms[swot.PARMS] = swot.parms({}):tojson()
end

if __gedi__ then
    default_parms[gedi.PARMS] = gedi.parms({}):tojson()
end

if __icesat2__ then
    default_parms[icesat2.PARMS] = icesat2.parms({}):tojson()
end

return json.encode(default_parms)

