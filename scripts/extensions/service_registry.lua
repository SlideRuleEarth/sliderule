local default_name = "http://"..sys.ipv4()..":9081"
local name = arg[1] or default_name
local service = "sliderule"
local lifetime = 120 -- seconds
local registration_state = false

while sys.alive() do
    if sys.healthy() then
        sys.log(core.DEBUG, "Registering "..name.." to service <"..service.."> for "..tostring(lifetime))
        local status = netsvc.orchreg(service, lifetime, name, false)
        if status then
            if registration_state then Lvl = core.DEBUG
            else Lvl = core.INFO end
            sys.log(Lvl, "Successfully registered to <"..service..">: "..response)
            registration_state = true

            -- wait until next registration time
            local next_registration_time = time.gps() + ((lifetime / 2) * 1000)
            while sys.alive() and time.gps() < next_registration_time do
                sys.wait(5)
            end
        else
            if not registration_state then Lvl = core.DEBUG
            else Lvl = core.ERROR end
            sys.log(Lvl, "Failed to register to <"..service..">: "..response)
            registration_state = false

            -- wait before trying again
            sys.wait(5)
        end
    else
        sys.log(core.ERROR, "Aborting registration to service <"..service..">: system unhealthy")
        sys.wait(30)
    end
end
