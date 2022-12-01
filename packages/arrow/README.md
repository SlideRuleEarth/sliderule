# Arrow Package

This package provides classses for building and using Apache Arrow in-memory and file formats.

## Install Arrow Library

To get the latest information and releases for Apache Arrow, please refer to their website: https://arrow.apache.org/.  For detailed instructions on using their cmake build system, see: https://github.com/apache/arrow/blob/master/docs/source/developers/cpp/building.rst.

For Ubuntu 22.04
```bash
clone https://github.com/apache/arrow.git
cd arrow/cpp
mkdir build
cd build
cmake .. -DARROW_PARQUET=ON -DARROW_WITH_ZLIB=ON
make -j8
make install
```
