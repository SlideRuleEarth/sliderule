from sliderule import icesat2
icesat2.init("localhost", True, organization=None)
params={'srt': 1,
 'len': 20,
 'track': 0,
 'pass_invalid': True,
 'cnf': -2,
 't0': '2019-05-02T02:12:24',
 't1': '2019-05-02T03:00:00',
 'poly': [{'lat': -64.5, 'lon': 67.7},
  {'lat': -64.5, 'lon': 59.6},
  {'lat': -76.5, 'lon': 59.6},
  {'lat': -76.5, 'lon': 67.7},
  {'lat': -64.5, 'lon': 67.7}]}
gdf = icesat2.atl03s(params, asset="nsidc-s3",  resource='ATL03_20190502021224_05160312_005_01.h5')