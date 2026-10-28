local runner = require("test_executive")
local csv = require("csv")

-- Local --

local PI = 3.141592653589793

local function d2r(d)
    return (90.0 - d) * (PI / 180.0)
end

-- Self Test --

runner.unittest("ATL24 Uncertainty NIL", function()

    local rqst          = {}
    local timeout       = 60 * 1000
    local resource      = "local"
    local parms         = bathy.parms(rqst, nil, "icesat2", resource)
    local uncertainty   = atl24.uncertainty(parms)

    local df            = core.dataframe({
        surface_h           = {0, 0, 0, 0},
        kd                  = {0, 0, 0, 0},
        surface_roughness   = {0, 0, 0, 0},
        ref_el              = {0, 0, 0, 0},
        geoid_corr_h        = {0, 0, 0, 0},
        sigma_h             = {0, 0, 0, 0},
        sigma_along         = {0, 0, 0, 0},
        sigma_across        = {0, 0, 0, 0}
    }, {
        spot = 0,
        granule = resource
    })

    df:run(uncertainty)
    df:run(core.TERMINATE)

    runner.assert(df:start(), "failed to start dataframe processing", true)
    runner.assert(df:finished(timeout), "failed to finish dataframe processing", true)

    local output = df:export()["gdf"]

    runner.assert(output["sigma_tvu"][1] - 0.1 < 0.001, tostring(output["sigma_tvu"][1]))
    runner.assert(output["sigma_tvu"][2] - 0.1 < 0.001, tostring(output["sigma_tvu"][2]))
    runner.assert(output["sigma_tvu"][3] - 0.1 < 0.001, tostring(output["sigma_tvu"][3]))
    runner.assert(output["sigma_tvu"][4] - 0.1 < 0.001, tostring(output["sigma_tvu"][4]))

    runner.assert(output["sigma_thu"][1] == 0.0, tostring(output["sigma_thu"][1]))
    runner.assert(output["sigma_thu"][2] == 0.0, tostring(output["sigma_thu"][2]))
    runner.assert(output["sigma_thu"][3] == 0.0, tostring(output["sigma_thu"][3]))
    runner.assert(output["sigma_thu"][4] == 0.0, tostring(output["sigma_thu"][4]))

end)

-- Self Test --

runner.unittest("ATL24 Uncertainty Tables", function()

    local rqst          = {}
    local timeout       = 60 * 1000
    local tolerance     = 0.4
    local resource      = "local"
    local parms         = bathy.parms(rqst, nil, "icesat2", resource)
    local uncertainty   = atl24.uncertainty(parms)
    local _,dir         = runner.srcscript()
    local total_count   = 0
    local tvu_sum       = 0.0
    local thu_sum       = 0.0
    local tvu_max       = 0.0
    local thu_max       = 0.0

    for _,pointing_angle in ipairs({"0", "1", "2", "3", "4", "5"}) do
        for _,depth in ipairs({"5", "10", "15"}) do
            local filename = dir .. string.format("../tables/ATL24_LUT_Validation_Table_%s_deg_%sm.csv", pointing_angle, depth)
            print(string.format("Running %sdeg at %sm - %s", pointing_angle, depth, filename))
            local lut = csv.open(filename)
            local ref_el = d2r(tonumber(pointing_angle))

            local input = {
                surface_h           = {},
                kd                  = {},
                surface_roughness   = {},
                ref_el              = {},
                geoid_corr_h        = {},
                sigma_h             = {},
                sigma_along         = {},
                sigma_across        = {}
            }

            for i,t in ipairs(lut) do
                table.insert(input.surface_h, tonumber(t["Depth (m)"])) -- forces depth calculation to be this when geoid_corr_h is 0
                table.insert(input.kd, tonumber(t["Kd_input"]))
                table.insert(input.surface_roughness, tonumber(t["Wind_input"]))
                table.insert(input.ref_el, ref_el)
                table.insert(input.geoid_corr_h, 0.0)
                table.insert(input.sigma_h, 0.0)
                table.insert(input.sigma_along, 0.0)
                table.insert(input.sigma_across, 0.0)
            end
            local df = core.dataframe(input, {})

            df:run(uncertainty)
            df:run(core.TERMINATE)

            runner.assert(df:start(), "failed to start dataframe processing", true)
            runner.assert(df:finished(timeout), "failed to finish dataframe processing", true)

            local output = df:export()["gdf"]

            local mismatch_count = 0
            for i,t in ipairs(lut) do
                total_count = total_count + 1
                local tvu_delta = math.abs(output["sigma_tvu"][i] - tonumber(t["SVU (m)"]))
                if tvu_delta > tvu_max then tvu_max = tvu_delta end
                tvu_sum = tvu_sum + tvu_delta
                if tvu_delta > tolerance then
                    print(string.format("tvu mismatch - %d: %f ~- %f", i, output["sigma_tvu"][i], t["SVU (m)"]))
                    mismatch_count = mismatch_count + 1
                end
                local thu_delta = math.abs(output["sigma_thu"][i] - tonumber(t["SHU (m)"]))
                if thu_delta > thu_max then thu_max = thu_delta end
                thu_sum = thu_sum + thu_delta
                if thu_delta > tolerance then
                    print(string.format("thu mismatch - %d: %f ~- %f", i, output["sigma_thu"][i], t["SHU (m)"]))
                    mismatch_count = mismatch_count + 1
                end
            end
            runner.assert(mismatch_count == 0, string.format("Mismatches for %sdeg at %sm - %d", pointing_angle, depth, mismatch_count))
        end
    end

    local tvu_avg = tvu_sum / total_count
    local thu_avg = thu_sum / total_count

    print(string.format("TVU Summary: avg = %f, max = %f", tvu_avg, tvu_max))
    print(string.format("THU Summary: avg = %f, max = %f", thu_avg, thu_max))

end)

-- Report Results --

runner.report()
