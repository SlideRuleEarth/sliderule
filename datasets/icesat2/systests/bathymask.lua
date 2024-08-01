local bathymask = geo.tiff("/data/ATL24_Mask_v5_Raster.tif")
bathymask:tobmp("/tmp/bathy_mask.bmp")

--local width, length = bathymask:dimensions()
--for x = 0, (width-1) do
--    for y = 0, (length -1) do
--        local p = bathymask:pixel(x, y)
--        if p ~= 0xFFFFFFFF then
--            print(string.format("%d, %d -> %08X", x, y, p))
--        end
--    end
--end
