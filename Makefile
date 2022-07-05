ROOT = $(shell pwd)
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
RUNDIR = /usr/local/etc/sliderule
BUILDDIR = build

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
	make -j4 -C $(BUILDDIR)

config: release-config

# release version of sliderule binary
release-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Release -DPACKAGE_FOR_DEBIAN=ON $(ROOT)

# debug version of sliderule binary
development-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Debug $(FULLCFG) $(ROOT)

# python bindings
python-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

# shared library libsliderule.so
library-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

# atlas plugin
atlas-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/atlas

# icesat2 plugin
icesat2-config: prep
	cd $(BUILDDIR); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/icesat2

# static analysis
scan: prep
	cd $(BUILDDIR); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(FULLCFG) $(ROOT)
	cd $(BUILDDIR); scan-build -o scan-results make

# address sanitizer debug build of sliderule binary
asan: prep
	cd $(BUILDDIR); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(FULLCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(BUILDDIR); make

install:
	make -C $(BUILDDIR) install

uninstall:
	xargs rm < $(BUILDDIR)/install_manifest.txt

package: distclean release-config
	make -C $(BUILDDIR) package
	# sudo dpkg -i $(BUILDDIR)/sliderule-X.Y.Z.deb

prep:
	mkdir -p $(BUILDDIR)

clean:
	make -C $(BUILDDIR) clean

distclean:
	- rm -Rf $(BUILDDIR)

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
	lcov -c --directory $(BUILDDIR) --output-file $(BUILDDIR)/coverage.info
	genhtml $(BUILDDIR)/coverage.info --output-directory $(BUILDDIR)/coverage_html
	# firefox $(BUILDDIR)/coverage_html/index.html

testpy:
	cp scripts/tests/coro.py $(BUILDDIR)
	cd $(BUILDDIR); /usr/bin/python3 coro.py