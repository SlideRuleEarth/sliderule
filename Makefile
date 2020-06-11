ROOT = $(shell pwd)
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
STAGEDIR = stage
RUNDIR = /usr/local/etc/sliderule

all: default-build

default-build:
	make -j4 -C build

config: default-config

default-config:
	mkdir -p build
	cd build; cmake ..

release-config:
	mkdir -p build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release ..

offline-config:
	mkdir -p build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(ROOT)/$(STAGEDIR) -DRUNTIMEDIR=$(RUNDIR) ..

docker-image: distclean offline-config default-build install
	cp targets/sliderule-docker/Dockerfile $(STAGEDIR)
	cp -R scripts/test $(STAGEDIR)/tests
	cp plugins/icesat2/server.lua $(STAGEDIR)/etc/sliderule
	cd $(STAGEDIR); docker build -t sliderule-linux:latest .
	# docker run -it --rm --name=sliderule1 -v /data:/data sliderule-linux /usr/local/tests/test_runner.lua
	# docker run -it --rm --name=sliderule1 -v /data:/data -p 9081:9081 sliderule-linux /usr/local/etc/sliderule/server.lua

scan:
	mkdir -p build
	cd build; export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) ..
	cd build; scan-build -o scan-results make

asan:
	mkdir -p build
	cd build; export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON ..
	cd build; make

install:
	make -C build install

uninstall:
	xargs rm < build/install_manifest.txt

package: distclean release-config
	make -C build package
	# sudo dpkg -i build/sliderule-X.Y.Z.deb

clean:
	make -C build clean

distclean:
	- rm -Rf build
	- rm -Rf $(STAGEDIR)

testcase=
testmem:
	valgrind --leak-check=full --track-origins=yes --track-fds=yes sliderule $(testcase)

testcpu:
	valgrind --tool=callgrind sliderule $(testcase)
	# kcachegrind callgrind.out.<pid>

testheap:
	valgrind --tool=massif --time-unit=B --pages-as-heap=yes sliderule $(testcase)
	# ms_print massif.out.<pid>

testheaptrack:
	# heaptrack sliderule $(testcase)
	# heaptrack_gui heaptrack.sliderule.<pid>.gz

testcov:
	lcov -c --directory build --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage_html
	# firefox build/coverage_html/index.html

