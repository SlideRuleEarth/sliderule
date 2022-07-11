local name = sys.ipv4()
local service = "sliderule"
local lifetime = 120 -- seconds
local orchestrator_url = os.getenv("ORCHESTRATOR") or "http://127.0.0.1:8050"

while sys.alive() do
    if sys.healthy() then
        sys.log(core.DEBUG, "Registering "..name.." to service <"..service.."> for "..tostring(lifetime))
        local response, status = netsvc.post(orchestrator_url.."/discovery/", string.format("{\"service\": \"%s\", \"lifetime\": %d, \"name\":\"%s\"}", service, lifetime, name))
        if status then
            sys.log(core.DEBUG, "Successfully registered to <"..service..">: "..response)
        else
            sys.log(core.ERROR, "Failed to register to <"..service..">: "..response)
        end
        -- wait until next registration time
        local next_registration_time = time.gps() + ((lifetime / 2) * 1000)
        while sys.alive() and time.gps() < next_registration_time do
            sys.wait(5)
        end
    else
        sys.log(core.ERROR, "Aborting registration to service <"..service..">: system unhealthy")
        sys.wait(30)
    end
end
