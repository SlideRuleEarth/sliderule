
list-granules:
	python scripts/build.py --list_granules

partition:
	python scripts/build.py --partition

partition-runner:
	sliderule-runner submit atl06t_partition scripts/partition_runner.py /data/ATL0/atl06_granules.txt --image pysr:latest

list-partitions:
	python scripts/build.py --list_tiles

tile:
	python scripts/build.py --tile

tile-runner:
	sliderule-runner submit atl06t_tile scripts/tile_runner.py /data/ATL06/atl06_tiles.txt --image pysr:latest
