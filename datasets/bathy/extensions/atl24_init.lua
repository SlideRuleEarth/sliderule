--
-- Initialize the environment needed for ATL24 processing
--
--  NOTES:  1. Downloads all external files needed for bathy processing
--          2. Runs uncertainty calculation initialization (uses downloaded files)
--

local status = true

local bathy_mask_complete = false
local water_ri_mask_complete = false
local uncertainty_lut_complete = true -- logic below needs it to be seeded to true
local pointnet_model_complete = false
local qtrees_model_complete = false
local coastnet_model_complete = false
local ensemble_model_complete = false

while sys.alive() and ( (not bathy_mask_complete) or
                        (not water_ri_mask_complete) or
                        (not uncertainty_lut_complete) or
                        (not pointnet_model_complete) or
                        (not qtrees_model_complete) or
                        (not coastnet_model_complete) or
                        (not ensemble_model_complete)) do

    sys.log(core.INFO, "Initializing ATL24 processing environment...")

    -- bathy mask
    local bathy_mask_local = cre.HOST_DIRECTORY.."/ATL24_Mask_v5_Raster.tif"
    local bathy_mask_remote = "config/ATL24_Mask_v5_Raster.tif"
    bathy_mask_complete = sys.fileexists(bathy_mask_local) or aws.s3download("sliderule", bathy_mask_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, bathy_mask_local)
    if not bathy_mask_complete then sys.deletefile(bathy_mask_local) end

    -- water ri mask
    local water_ri_mask_local = cre.HOST_DIRECTORY.."/cop_rep_ANNUAL_meanRI_d00.tif"
    local water_ri_mask_remote = "config/cop_rep_ANNUAL_meanRI_d00.tif"
    water_ri_mask_complete = sys.fileexists(water_ri_mask_local) or aws.s3download("sliderule", water_ri_mask_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, water_ri_mask_local)
    if not water_ri_mask_complete then sys.deletefile(water_ri_mask_local) end

    -- uncertainty lookup table
    for deg = 0,5 do
        for _,dir in ipairs({"THU", "TVU"}) do
            local uncertainty_lut_local = string.format("%s/ICESat2_%ddeg_500000_AGL_0.022_mrad_%s.csv", cre.HOST_DIRECTORY, deg, dir)
            local uncertainty_lut_remote = string.format("config/ICESat2_%ddeg_500000_AGL_0.022_mrad_%s.csv", deg, dir)
            local uncertainty_lut_status = sys.fileexists(uncertainty_lut_local) or aws.s3download("sliderule", uncertainty_lut_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, uncertainty_lut_local)
            if not uncertainty_lut_status then sys.deletefile(uncertainty_lut_local) end
            uncertainty_lut_complete = uncertainty_lut_complete and uncertainty_lut_status
        end
    end

    -- pointnet model
    local pointnet_model_local = cre.HOST_DIRECTORY.."/"..bathy.POINTNET_MODEL
    local pointnet_model_remote = "config/"..bathy.POINTNET_MODEL
    pointnet_model_complete = sys.fileexists(pointnet_model_local) or aws.s3download("sliderule", pointnet_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, pointnet_model_local)
    if not pointnet_model_complete then sys.deletefile(pointnet_model_local) end

    -- qtrees model
    local qtrees_model_local = cre.HOST_DIRECTORY.."/"..bathy.QTREES_MODEL
    local qtrees_model_remote = "config/"..bathy.QTREES_MODEL
    qtrees_model_complete = sys.fileexists(qtrees_model_local) or aws.s3download("sliderule", qtrees_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, qtrees_model_local)
    if not qtrees_model_complete then sys.deletefile(qtrees_model_local) end

    -- coastnet model
    local coastnet_model_local = cre.HOST_DIRECTORY.."/"..bathy.COASTNET_MODEL
    local coastnet_model_remote = "config/"..bathy.COASTNET_MODEL
    coastnet_model_complete = sys.fileexists(coastnet_model_local) or aws.s3download("sliderule", coastnet_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, coastnet_model_local)
    if not coastnet_model_complete then sys.deletefile(coastnet_model_local) end

    -- ensemble model
    local ensemble_model_local = cre.HOST_DIRECTORY.."/"..bathy.ENSEMBLE_MODEL
    local ensemble_model_remote = "config/"..bathy.ENSEMBLE_MODEL
    ensemble_model_complete = sys.fileexists(ensemble_model_local) or aws.s3download("sliderule", ensemble_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, ensemble_model_local)
    if not ensemble_model_complete then sys.deletefile(ensemble_model_local) end

    -- check for completeness
    if (not bathy_mask_complete) or
       (not water_ri_mask_complete) or
       (not uncertainty_lut_complete) or
       (not pointnet_model_complete) or
       (not qtrees_model_complete) or
       (not coastnet_model_complete) or
       (not ensemble_model_complete) then
        sys.log(core.CRITICAL, string.format("Failed to initialize ATL24 processing environment"))
        if not bathy_mask_complete then sys.log(core.CRITICAL, "bathy mask did not download") end
        if not water_ri_mask_complete then sys.log(core.CRITICAL, "water refractive index mask did not download") end
        if not uncertainty_lut_complete then sys.log(core.CRITICAL, "uncertainty look up tables did not download") end
        if not pointnet_model_complete then sys.log(core.CRITICAL, "pointnet model did not download") end
        if not qtrees_model_complete then sys.log(core.CRITICAL, "qtrees model did not download") end
        if not coastnet_model_complete then sys.log(core.CRITICAL, "coastnet model did not download") end
        if not ensemble_model_complete then sys.log(core.CRITICAL, "ensemble model did not download") end
        sys.wait(30)
    end

end

-- initialize uncertainty tables
if sys.alive() then
    status = bathy.inituncertainty()
end

if status then
    sys.log(core.INFO, "Successfully initialized ATL24 processing environment")
else
    sys.log(core.INFO, "Failed to initialize ATL24 processing environment")
end