VIRTUAL_ENV = $(shell echo $$VIRTUAL_ENV)
cmake_prefix_path=$(shell python -c 'import torch;print(torch.utils.cmake_prefix_path)')

default:
	@mkdir -p ./build/debug
	@echo cmake_prefix_path=$(cmake_prefix_path)
	@cd build/debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_PREFIX_PATH=${cmake_prefix_path} ../..
	@cd build/debug && make -j

clean:
	@rm -rf build
