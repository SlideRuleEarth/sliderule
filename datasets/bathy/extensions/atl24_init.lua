--
-- Initialize the environment needed for ATL24 processing
--
--  NOTES:  1. Downloads the ATL24 bathy mask from S3
--          2. Downloads the ATL24 uncertainty lookup tables from S3
--

local bathy_mask_complete = false
local uncertainty_lut_complete = true -- logic below needs it to be seeded to true
local pointnet_model_complete = false
local qtrees_model_complete = false
local coastnet_model_complete = false

while sys.alive() and ( (not bathy_mask_complete) or
                        (not uncertainty_lut_complete) or
                        (not pointnet_model_complete) or
                        (not qtrees_model_complete) or
                        (not coastnet_model_complete)) do

    sys.log(core.INFO, "Initializing ATL24 processing environment...")

    -- bathy mask
    local bathy_mask_local = cre.HOST_DIRECTORY.."/ATL24_Mask_v5_Raster.tif"
    local bathy_mask_remote = "config/ATL24_Mask_v5_Raster.tif"
    bathy_mask_complete = sys.fileexists(bathy_mask_local) or aws.s3download("sliderule", bathy_mask_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, bathy_mask_local)

    -- uncertainty lookup table
    for deg = 0,5 do
        for _,dir in ipairs({"THU", "TVU"}) do
            local uncertainty_lut_local = string.format("%s/ICESat2_%ddeg_500000_AGL_0.022_mrad_%s.csv", cre.HOST_DIRECTORY, deg, dir)
            local uncertainty_lut_remote = string.format("config/ICESat2_%ddeg_500000_AGL_0.022_mrad_%s.csv", deg, dir)
            local uncertainty_lut_status = sys.fileexists(uncertainty_lut_local) or aws.s3download("sliderule", uncertainty_lut_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, uncertainty_lut_local)
            uncertainty_lut_complete = uncertainty_lut_complete and uncertainty_lut_status
        end
    end

    -- pointnet model
    local pointnet_model_local = cre.HOST_DIRECTORY.."/pointnet2_model.pth"
    local pointnet_model_remote = "config/pointnet2_model.pth"
    pointnet_model_complete = sys.fileexists(pointnet_model_local) or aws.s3download("sliderule", pointnet_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, pointnet_model_local)

    -- qtrees model
    local qtrees_model_local = cre.HOST_DIRECTORY.."/model-20240607.json"
    local qtrees_model_remote = "config/model-20240607.json"
    qtrees_model_complete = sys.fileexists(qtrees_model_local) or aws.s3download("sliderule", qtrees_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, qtrees_model_local)

    -- coastnet model
    local coastnet_model_local = cre.HOST_DIRECTORY.."/coastnet_model-20240628.json"
    local coastnet_model_remote = "config/coastnet_model-20240628.json"
    coastnet_model_complete = sys.fileexists(coastnet_model_local) or aws.s3download("sliderule", coastnet_model_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, coastnet_model_local)

    -- check for completeness
    if (not bathy_mask_complete) or
       (not uncertainty_lut_complete) or
       (not pointnet_model_complete) or
       (not qtrees_model_complete) or
       (not coastnet_model_complete) then
        sys.log(core.CRITICAL, string.format("Failed to initialize ATL24 processing environment"))
        if not bathy_mask_complete then sys.log(core.CRITICAL, "bathy mask did not download") end
        if not uncertainty_lut_complete then sys.log(core.CRITICAL, "uncertainty look up tables did not download") end
        if not pointnet_model_complete then sys.log(core.CRITICAL, "pointnet model did not download") end
        if not qtrees_model_complete then sys.log(core.CRITICAL, "qtrees model did not download") end
        if not coastnet_model_complete then sys.log(core.CRITICAL, "coastnet model did not download") end
        sys.wait(30)
    end

end

sys.log(core.INFO, "Successfully initialized ATL24 processing environment")
