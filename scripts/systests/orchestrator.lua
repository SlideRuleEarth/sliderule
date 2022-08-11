local runner = require("test_executive")
local console = require("console")
local json = require("json")

netsvc.orchurl("http://127.0.0.1:8050")

--
-- API: health
--
local h = netsvc.orchhealth()
runner.check(h, "orchestrator unhealthy")

--
-- API: <register>
--
local reg_status = netsvc.orchreg('test', 5, 'bob', true)
runner.check(reg_status, "failed to register")

--
-- API: lock
--
local m = netsvc.orchlock("test", 1, 5, true)
local first_tx_id = nil
for k,v in pairs(m) do
    first_tx_id = k
    print(k, v)
end
runner.check(m[first_tx_id] == "bob", "failed to lock")

--
-- API: unlock
--
local n = netsvc.orchunlock({tonumber(first_tx_id)}, true)
runner.check(n, "failed to unlock")


runner.report()
