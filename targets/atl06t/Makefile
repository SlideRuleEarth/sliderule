
list-granules:
	python scripts/build.py --list_granules

partition:
	python scripts/build.py --partition

partition-runner:
	sliderule-runner submit atl06t_partition scripts/partition_runner.py /data/ATL06/atl06_granules.txt --vcpus 1 --memory 8000 --image pysr:unstable

list-partitions:
	python scripts/build.py --list_tiles

tile:
	python scripts/build.py --tile --workers 4

tile-runner:
	sliderule-runner submit atl06t_tile scripts/tile_runner.py /data/ATL06/atl06_tiles.txt --image pysr:unstable

stat:
	python scripts/build.py --stat

stat-runner:
	sliderule-runner submit atl06t_stat scripts/stat_runner.py /data/ATL06/atl06_stats_l7.txt --image pysr:unstable --vcpus 2 --memory 8000

manifest:
	python scripts/build.py --manifest