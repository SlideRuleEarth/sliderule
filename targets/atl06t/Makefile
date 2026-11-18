
list-granules:
	python scripts/build.py --list_granules

partition:
#	python scripts/build.py --partition
	sliderule-runner submit p1 scripts/partition_runner.py /data/ATL06/atl06_granules.txt --vcpus 1 --memory 8000 --image pysr:unstable

list-partitions:
	python scripts/build.py --list_tiles

tile:
#	python scripts/build.py --tile --workers 4
	sliderule-runner submit t1 scripts/tile_runner.py /data/ATL06/atl06_tiles.txt --image pysr:unstable

stat:
#	python scripts/build.py --stat
	sliderule-runner submit s2 scripts/stat_runner.py /data/ATL06/atl06_stats_l7.txt --image pysr:unstable --vcpus 2 --memory 8000

stack:
	python scripts/build.py --stack --stat_level 6
	python scripts/build.py --stack --stat_level 5
	python scripts/build.py --stack --stat_level 4
	python scripts/build.py --stack --stat_level 3
	python scripts/build.py --stack --stat_level 2
	python scripts/build.py --stack --stat_level 1
	python scripts/build.py --stack --stat_level 0

manifest:
	python scripts/build.py --manifest