from sliderule import icesat2
icesat2.init("slideruleearth.io", True, organization="sliderule")

rec_cnt = 0
ph_cnt = 0

def atl03rec_cb(rec):
  global rec_cnt, ph_cnt
  rec_cnt += 1
  ph_cnt += rec["count"][0] + rec["count"][1]
  print("{} {}".format(rec_cnt, ph_cnt), end='\r')

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

gdf = icesat2.atl03sp(params, asset="nsidc-s3", resources=['ATL03_20190502021224_05160312_005_01.h5'], callbacks = {"atl03rec": atl03rec_cb})
