--
-- Initialize the environment needed for ATL24 processing
--
--  NOTES:  1. Downloads the ATL24 bathy mask from S3
--

local bathy_mask_complete = false

while sys.alive() and (not bathy_mask_complete) do

    sys.log(core.INFO, "Initializing ATL24 processing environment...")

    local bathy_mask_local = "/data/ATL24_Mask_v5_Raster.tif"
    local bathy_mask_remote = "config/ATL24_Mask_v5_Raster.tif"
    bathy_mask_complete = sys.fileexists(bathy_mask_local) or aws.s3download("sliderule", bathy_mask_remote, aws.DEFAULT_REGION, aws.DEFAULT_IDENTITY, bathy_mask_local)

    if (not bathy_mask_complete) then
        sys.log(core.CRITICAL, string.format("Failed to initialize ATL24 processing environment: bathy_mask=%s", tostring(bathy_mask_complete)))
        sys.wait(30)
    end

end

sys.log(core.INFO, "Successfully initialized ATL24 processing environment")
