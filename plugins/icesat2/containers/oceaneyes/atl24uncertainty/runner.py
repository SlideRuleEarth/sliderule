import rasterio as rio
from datetime import datetime as dt

common_path = argoutpath + '/env_stats'

kd_out, req_ok = get_kd_values(profile_date, outpath=common_path, keep=False)

coord_list = [(x, y) for x, y in zip(bathy_points.lon_ph, bathy_points.lat_ph)]
dt1 = dt.now()

with rio.open(f'netcdf:{kd_out}:Kd_490') as kd_src:
    bathy_points["kd_490"] = [x[0] for x in kd_src.sample(coord_list)]
    bathy_points["kd_490"] = bathy_points["kd_490"] * kd_src.scales[0] + kd_src.offsets[0]