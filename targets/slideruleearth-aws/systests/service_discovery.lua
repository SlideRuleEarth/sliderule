local runner = require("test_executive")
local console = require("console")

--
-- API: health
--
local h = core.orchhealth()
runner.assert(h, "orchestrator unhealthy")

--
-- API: register
--
local reg_status = core.orchreg('test', 5, 'bob', true, true)
runner.assert(reg_status, "failed to register")

--
-- API: lock
--
local m = core.orchlock("test", 1, 5, 1, true)
local first_tx_id = nil
for k,v in pairs(m) do
    first_tx_id = k
    print(k, v)
end
runner.assert(m[first_tx_id] == "bob", "failed to lock")

--
-- API: unlock
--
local n = core.orchunlock({tonumber(first_tx_id)}, true)
runner.assert(n, "failed to unlock")


runner.report()
