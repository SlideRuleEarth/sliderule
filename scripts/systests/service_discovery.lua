local runner = require("test_executive")
local console = require("console")

--
-- API: health
--
local h = netsvc.orchhealth()
runner.check(h, "orchestrator unhealthy")

--
-- API: register
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
