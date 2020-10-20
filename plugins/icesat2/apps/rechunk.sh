SRC=$1
DST=$2
h5repack -v -l \
gt1l/geolocation/delta_time,\
gt1r/geolocation/delta_time,\
gt2l/geolocation/delta_time,\
gt2r/geolocation/delta_time,\
gt3l/geolocation/delta_time,\
gt3r/geolocation/delta_time,\
gt1l/geolocation/segment_dist_x,\
gt1r/geolocation/segment_dist_x,\
gt2l/geolocation/segment_dist_x,\
gt2r/geolocation/segment_dist_x,\
gt3l/geolocation/segment_dist_x,\
gt3r/geolocation/segment_dist_x,\
gt1l/geolocation/reference_photon_lat,\
gt1r/geolocation/reference_photon_lat,\
gt2l/geolocation/reference_photon_lat,\
gt2r/geolocation/reference_photon_lat,\
gt3l/geolocation/reference_photon_lat,\
gt3r/geolocation/reference_photon_lat,\
gt1l/geolocation/reference_photon_lon,\
gt1r/geolocation/reference_photon_lon,\
gt2l/geolocation/reference_photon_lon,\
gt2r/geolocation/reference_photon_lon,\
gt3l/geolocation/reference_photon_lon,\
gt3r/geolocation/reference_photon_lon,\
gt1l/bckgrd_atlas/delta_time,\
gt1r/bckgrd_atlas/delta_time,\
gt2l/bckgrd_atlas/delta_time,\
gt2r/bckgrd_atlas/delta_time,\
gt3l/bckgrd_atlas/delta_time,\
gt3r/bckgrd_atlas/delta_time\
:CHUNK=6250000 \
-l \
gt1l/geolocation/segment_ph_cnt,\
gt1r/geolocation/segment_ph_cnt,\
gt2l/geolocation/segment_ph_cnt,\
gt2r/geolocation/segment_ph_cnt,\
gt3l/geolocation/segment_ph_cnt,\
gt3r/geolocation/segment_ph_cnt,\
gt1l/geolocation/segment_id,\
gt1r/geolocation/segment_id,\
gt2l/geolocation/segment_id,\
gt2r/geolocation/segment_id,\
gt3l/geolocation/segment_id,\
gt3r/geolocation/segment_id,\
gt1l/heights/dist_ph_along,\
gt1r/heights/dist_ph_along,\
gt2l/heights/dist_ph_along,\
gt2r/heights/dist_ph_along,\
gt3l/heights/dist_ph_along,\
gt3r/heights/dist_ph_along,\
gt1l/heights/h_ph,\
gt1r/heights/h_ph,\
gt2l/heights/h_ph,\
gt2r/heights/h_ph,\
gt3l/heights/h_ph,\
gt3r/heights/h_ph,\
gt1l/bckgrd_atlas/bckgrd_rate,\
gt1r/bckgrd_atlas/bckgrd_rate,\
gt2l/bckgrd_atlas/bckgrd_rate,\
gt2r/bckgrd_atlas/bckgrd_rate,\
gt3l/bckgrd_atlas/bckgrd_rate,\
gt3r/bckgrd_atlas/bckgrd_rate\
:CHUNK=12500000 \
-l \
gt1l/heights/signal_conf_ph,\
gt1r/heights/signal_conf_ph,\
gt2l/heights/signal_conf_ph,\
gt2r/heights/signal_conf_ph,\
gt3l/heights/signal_conf_ph,\
gt3r/heights/signal_conf_ph\
:CHUNK=50000000x5 \
 $SRC $DST
