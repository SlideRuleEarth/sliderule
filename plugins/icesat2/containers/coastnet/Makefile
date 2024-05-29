VIRTUAL_ENV = $(shell echo $$VIRTUAL_ENV)
cmake_prefix_path=$(shell python -c 'import torch;print(torch.utils.cmake_prefix_path)')

default:
	@mkdir -p ./build/release
	@echo cmake_prefix_path=$(cmake_prefix_path)
	@cd build/release && cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH=${cmake_prefix_path} ../..
	@cd build/release && make -j

clean:
	@rm -rf build
