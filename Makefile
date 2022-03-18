ROOT = $(shell pwd)
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
RUNDIR = /usr/local/etc/sliderule

FULLCFG  = -DENABLE_TRACING=ON
FULLCFG += -DUSE_AWS_PACKAGE=ON
FULLCFG += -DUSE_CCSDS_PACKAGE=ON
FULLCFG += -DUSE_GEOTIFF_PACKAGE=ON
FULLCFG += -DUSE_H5_PACKAGE=ON
FULLCFG += -DUSE_LEGACY_PACKAGE=ON
FULLCFG += -DUSE_NETSVC_PACKAGE=ON
FULLCFG += -DUSE_PISTACHE_PACKAGE=ON

PYTHONCFG  = -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=ON
PYTHONCFG += -DUSE_CCSDS_PACKAGE=ON
PYTHONCFG += -DUSE_GEOTIFF_PACKAGE=ON
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192

all: default-build

default-build:
	make -j4 -C build

config: release-config

# release version of sliderule binary
release-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DPACKAGE_FOR_DEBIAN=ON $(ROOT)

# debug version of sliderule binary
development-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Debug $(FULLCFG) $(ROOT)

# python bindings
python-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

# shared library libsliderule.so
library-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

# static analysis
scan: prep
	cd build; export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(FULLCFG) $(ROOT)
	cd build; scan-build -o scan-results make

# address sanitizer debug build of sliderule binary
asan: prep
	cd build; export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(FULLCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd build; make

install:
	make -C build install

uninstall:
	xargs rm < build/install_manifest.txt

package: distclean release-config
	make -C build package
	# sudo dpkg -i build/sliderule-X.Y.Z.deb

prep:
	mkdir -p build

clean:
	make -C build clean

distclean:
	- rm -Rf build

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

testpy:
	cp scripts/tests/coro.py build
	cd build; /usr/bin/python3 coro.py