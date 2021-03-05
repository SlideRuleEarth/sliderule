from distutils.core import setup

setup(
  name          = 'h5coro',
  version       = '${TGTVER}',
  description   = 'Python module for cloud-optimized read-only access to HDF5 files',
  packages      = ['h5coro'],
  package_dir   = {'': '${CMAKE_CURRENT_BINARY_DIR}' },
  package_data  = {'': ['h5coro.so'] }
)