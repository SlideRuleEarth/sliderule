ROOT = $(shell pwd)
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
RUNDIR = /usr/local/etc/sliderule

FULLCFG  = -DENABLE_LTTNG_TRACING=ON
FULLCFG += -DUSE_LEGACY_PACKAGE=ON
FULLCFG += -DUSE_H5_PACKAGE=ON
FULLCFG += -DUSE_AWS_PACKAGE=ON
FULLCFG += -DUSE_PISTACHE_PACKAGE=ON

PYTHONCFG  = -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=OFF
PYTHONCFG += -DUSE_CCSDS_PACKAGE=OFF

LIBRARYCFG  = -DSHARED_LIBRARY=ON
LIBRARYCFG += -DUSE_H5_PACKAGE=ON
LIBRARYCFG += -DUSE_AWS_PACKAGE=ON
LIBRARYCFG += -DUSE_LEGACY_PACKAGE=ON
LIBRARYCFG += -DUSE_CCSDS_PACKAGE=ON

all: default-build

default-build:
	make -j4 -C build

config: release-config

release-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DPACKAGE_FOR_DEBIAN=ON $(ROOT)

development-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Debug $(FULLCFG) $(ROOT)

python-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

library-config: prep
	cd build; cmake -DCMAKE_BUILD_TYPE=Release $(LIBRARYCFG) $(ROOT)

scan: prep
	cd build; export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(FULLCFG) $(ROOT)
	cd build; scan-build -o scan-results make

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
	- rm -Rf stage

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

