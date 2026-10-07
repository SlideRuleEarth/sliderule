#!/usr/bin/env pythonw
""" Creates a cloud-optimized version of an ICESat-2 HDF5 file. """

# Copyright (c) 2024, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys
import argparse
import traceback
import h5py
from h5py import h5p, h5f
import numpy as np
import time

__version_= '1.0.1'

#-----------------------------------------------------------------------------
def h5_s2b(s:str) -> bytes:
  """ Converts a Python3 string to ASCII (bytes) for low-level calls.

  Args:
    s: Python string

  Returns:
    `s` as bytes
  """
  try:
    b = s.encode('ascii', 'ignore')
  except (UnicodeEncodeError, AttributeError):
    b = s
  return b

#------------------------------------------------------------------------------
def init():
  """ Parses arguments """

  parser = argparse.ArgumentParser(description="""Re-writes ICESat-2 HDF5 files using cloud optimization techniques.""")

  parser.add_argument('infile', help='Input HDF5 file.')

  parser.add_argument('outfile', help='Output HDF5 file.')

  parser.add_argument('-min_chunk', '--min_chunk', required=False,
                      type=int, default=500, help="""Do not resize any dataset with chunksize < this size (num elements).""")

  parser.add_argument('-seg_chunk', '--seg_chunk', required=False,
                      type=int, default=100000, help="""New chunksize for segment (non_photon) datasets (num elements).""")

  parser.add_argument('-ph_chunk', '--ph_chunk', required=False,
                      type=int, default=1000000, help="""New chunksize for any photon datasets (num elements).""")

  parser.add_argument('-gzip', '--gzip', required=False,
                      type=int, default=-1, help="""New gzip level (if dataset is compressed; 0=disable, -1=do not change)""")

  parser.add_argument('-fs_space_page', '--fs_space_page', dest='fs_space_page',
                      action='store_true', required=False, default=False,
                      help='Use H5F_FSPACE_STRATEGY_PAGE.')

  parser.add_argument('-fs_page_size', '--fs_page_size', required=False,
                      type=int, default=0., help="""Filespace page size (bytes), 0=do not change (requires -fs_space_page).""")

  parser.add_argument('-cache_slots', '--cache_slots',required=False,
                      type=int, default=0, help="""Number of cache slots, 0=do not change. (requires -fs_space_page)""")

  parser.add_argument('-chunk_cache', '--chunk_cache',required=False,
                      type=int, default=0., help="""Size of the chunk cache (bytes), 0=do not change. (requires -fs_space_page)""")

  parser.add_argument('-user_blocksize', '--user_blocksize', required=False,
                      type=int, default=0., help="""User block size (bytes), 0=do not change.""")

  parser.add_argument('-meta_blocksize', '--meta_blocksize', required=False,
                      type=int, default=0., help="""Metadata block size (bytes), 0=do not change.""")

  return parser.parse_args()

#------------------------------------------------------------------------------
def full_path(obj):
  """ Returns the full pathname of an object """

  if obj.parent is not None and obj.name != '/':
    return os.path.join(full_path(obj.parent), obj.name)
  else:
    return ''

#------------------------------------------------------------------------------
def print_stats_hdr():
  """ Prints the stats header """

  print('  {0:40s} {1:7s} {2:10s} {3:10s} {4:7s} {5:10s} {6:10s}'.format(
    'name',
    'src_zip',
    'src_size',
    'src_chunks',
    'dst_zip',
    'dst_nbytes',
    'dst_chunks',))
  print('  {0:40s} {1:7s} {2:10s} {3:10s} {4:7s} {5:10s} {6:10s}'.format(
    '----',
    '-------',
    '----------',
    '----------',
    '-------',
    '----------',
    '----------',))

#------------------------------------------------------------------------------
def print_stats(stats:dict):
  """ Prints stats """

  print('  {0:40s} {1:7d} {2:10d} {3:10d} {4:7d} {5:10d} {6:10d}'.format(
    stats['name'],
    stats['src_gzip'],
    stats['src_size'],
    stats['src_chunks'],
    stats['dst_gzip'],
    stats['dst_nbytes'],
    stats['dst_chunks'],))

#------------------------------------------------------------------------------
def copy_attributes(src, dest):
  """ Copies attributes from an input object to an output object """

  for key in src.attrs.keys():
    dest.attrs.create(key, data=src.attrs[key], dtype=src.attrs[key].dtype)

#------------------------------------------------------------------------------
def copy_dataset(args, src, dst, name, photons=False):
  """ Copies a dataset from an input group to an output group """

  stats = {
    'name':name,
    'src_gzip':0,
    'src_size':0,
    'src_nbytes':0,
    'src_chunks':0,
    'dst_gzip':0,
    'dst_nbytes':0,
    'dst_chunks':0,
    }

  src_dset = src[name]
  chunks = src_dset.chunks
  maxshape = src_dset.maxshape
  compress = src_dset.compression
  compress_opts = src_dset.compression_opts

  if src_dset.compression_opts is None:
    stats['src_gzip'] = -1
  else:
    stats['src_gzip'] = src_dset.compression_opts

  stats['src_size'] = src_dset.size
  stats['src_nbytes'] = src_dset.size

  if src_dset.chunks is None:
    stats['src_chunks'] = -1
  else:
    stats['src_chunks'] = src_dset.chunks[0]

  # Optionally modify compression (for compressed datasets)
  if compress == 'gzip':
    if args.gzip == 0:
      compress = None
      compress_opts = None
    elif args.gzip > 0:
      compress_opts = args.gzip

  # Optionally modify chunking (for select chunked datasets)
  #
  # This simplistic approach is probably not optimal for 2D or 3D
  # datasets, but it's a start.
  #
  # The 'variable' dimensions for ICESat-2 data are the left-most.
  # Do not change any chunksize less than args and do not change
  # any chunksize that is equal to maxshape.
  if maxshape[0] is None and chunks[0] is not None:
    if chunks[0] > args.min_chunk:
      chunks = list(chunks)
      if photons:
        chunks[0] = args.ph_chunk
      else:
        chunks[0] = args.seg_chunk
      chunks = tuple(chunks)

  dst_dset = dst.create_dataset(name, shape=src_dset.shape,
                                dtype=src_dset.dtype,
                                maxshape=src_dset.maxshape,
                                chunks=chunks,
                                compression=compress,
                                compression_opts=compress_opts,
                                shuffle=src_dset.shuffle,
                                fillvalue=src_dset.fillvalue,
                                track_order=True)
  dst_dset.write_direct(np.array(src_dset))
  if src_dset.is_scale:
    dst_dset.make_scale()

  copy_attributes(src_dset, dst_dset)

  if dst_dset.compression_opts is None:
    stats['dst_gzip'] = -1
  else:
    stats['dst_gzip'] = dst_dset.compression_opts

  stats['dst_size'] = dst_dset.size
  stats['dst_nbytes'] = dst_dset.size

  if dst_dset.chunks is None:
    stats['dst_chunks'] = -1
  else:
    stats['dst_chunks'] = dst_dset.chunks[0]

  print_stats(stats)

  return stats

#------------------------------------------------------------------------------
def copy_group(args, src, dst, name):
  """ Copies a group from an input file to an output file

  Could do something with stats here.  ie:
    min/max within each group, etc.  Not sure its worth it now.
  """

  src_grp = src[name]
  if name != '/':
    dst_grp = dst.create_group(name, track_order=True)
  else:
    dst_grp = dst

  # Copy group attributes
  copy_attributes(src_grp, dst_grp)

  path = full_path(src_grp)
  print(f'\nGroup: {path}\n')
  print_stats_hdr()

  # This hack will only work for ATL03
  if name == 'heights':
    photons = True
  else:
    photons = False

  # Copy child datasets
  for item in src_grp.keys():
    if isinstance(src_grp[item], h5py.Dataset):
      if item == 'control':
        continue
      stats = copy_dataset(args, src_grp, dst_grp, item, photons=photons)

  # Copy child groups
  for item in src_grp.keys():
    if isinstance(src_grp[item], h5py.Group):
      copy_group(args, src_grp, dst_grp, item)

#------------------------------------------------------------------------------
def optimize_file(args):
  """ Copies an ICESat-2 HDF5 file to a new cloud-optimized file.

  Uses the low-level h5py APID to open/query the input file and to create
  the output file. I started using existing code that was writted with an
  old h5py and then realized (too late) new h5py supports all these things.

  So, then switches to the high-level APID to actually copy the data.
  """

  strategies = [
    'H5F_FSPACE_STRATEGY_FSM_AGGR',
    'H5F_FSPACE_STRATEGY_PAGE',
    'H5F_FSPACE_STRATEGY_AGGR',
    'H5F_FSPACE_STRATEGY_NONE',
   ]

  if not os.path.isfile(args.infile):
    print(f'{args.infile} : file does not exist.')
    sys.exit(3)

  # Open the input file
  try:
    in_fid = h5f.open(h5_s2b(args.infile), h5f.ACC_RDONLY)
  except Exception as e:
    print(f'Error opening input file - {e}')
    sys.exit(3)

  # Get info from the file creation plist
  try:
    fcpl = in_fid.get_create_plist()
    fs_strategy = fcpl.get_file_space_strategy()
    fspace_page_size = fcpl.get_file_space_page_size()
    user_blocksize = fcpl.get_userblock()
  except Exception as e:
    print(f'Error reading the file creation plist - {e}')
    in_fid.close()
    sys.exit(3)

  # Get info from the file access plist
  try:
    fapl = in_fid.get_access_plist()
    cache_info = fapl.get_cache()
    meta_blocksize = fapl.get_meta_block_size()
  except Exception as e:
    print(f'Error reading the file access plist - {e}')
    in_fid.close()
    sys.exit(3)

  cache_slots = cache_info[1]
  chunk_cache = cache_info[2]

  strategy_str = strategies[fs_strategy[0]]
  print('--')
  print(f'Original fs_strategy      = {strategy_str}')
  print(f'Original fspace_page_size = {fspace_page_size}')
  print(f'Original cache_slots      = {cache_slots}')
  print(f'Original chunk_cache      = {chunk_cache}')
  print(f'Original userblock_size   = {user_blocksize}')
  print(f'Original meta_blocksize   = {meta_blocksize}')
  print('--')

  # Apply any optional argument values
  if args.fs_space_page:
    fs_strategy = list(fs_strategy)
    cache_info = list(cache_info)
    fs_strategy[0] = 1
    if args.fs_page_size > 0.:
      fspace_page_size = args.fs_page_size
    if args.chunk_cache > 0.:
      cache_info[2] = args.chunk_cache
    if args.cache_slots > 0.:
      cache_info[1] = args.cache_slots
    fs_strategy = tuple(fs_strategy)
    cache_info = tuple(cache_info)

  if args.user_blocksize > 0.:
    user_blocksize = args.user_blocksize
  if args.meta_blocksize > 0.:
    meta_blocksize = args.meta_blocksize

  # Create a new file create property list
  try:
    fcpl = h5p.create(h5p.FILE_CREATE)
    fcpl.set_link_creation_order(h5p.CRT_ORDER_TRACKED|h5p.CRT_ORDER_INDEXED)
    fcpl.set_attr_creation_order(h5p.CRT_ORDER_TRACKED|h5p.CRT_ORDER_INDEXED)
    fcpl.set_file_space_strategy(fs_strategy[0], fs_strategy[1], fs_strategy[2])
    fcpl.set_file_space_page_size(fspace_page_size)
    fcpl.set_userblock(user_blocksize)
  except Exception as e:
    print(f'Error creating the file creation plist - {e}')
    in_fid.close()
    sys.exit(3)

  # Create a new file access property list
  try:
    fapl = h5p.create(h5p.FILE_ACCESS)
    fapl.set_cache(cache_info[0], cache_info[1], cache_info[2], cache_info[3])
    fapl.set_meta_block_size(meta_blocksize)
  except Exception as e:
    print(f'Error creating the file access plist - {e}')
    in_fid.close()
    sys.exit(3)

  # Create the output file
  try:
    out_fid = h5f.create(h5_s2b(args.outfile), h5f.ACC_TRUNC, fcpl=fcpl, fapl=fapl)
  except Exception as e:
    print(f'Error creating the output file - {e}')
    sys.exit(3)

  # Get info from the file creation plist
  try:
    fcpl = out_fid.get_create_plist()
    fs_strategy = fcpl.get_file_space_strategy()
    fspace_page_size = fcpl.get_file_space_page_size()
    user_blocksize = fcpl.get_userblock()
  except Exception as e:
    print(f'Error reading the file creation plist - {e}')
    in_fid.close()
    out_fid.close()
    sys.exit(3)

  # Get info from the file access plist
  try:
    fapl = out_fid.get_access_plist()
    cache_info = fapl.get_cache()
    meta_blocksize = fapl.get_meta_block_size()
  except Exception as e:
    print(f'Error reading the file access plist - {e}')
    in_fid.close()
    out_fid.close()
    sys.exit(3)

  cache_slots = cache_info[1]
  chunk_cache = cache_info[2]

  strategy_str = strategies[fs_strategy[0]]
  print('--')
  print(f'Revised fs_strategy       = {strategy_str}')
  print(f'Revised fspace_page_size  = {fspace_page_size}')
  print(f'Revised cache_slots       = {cache_slots}')
  print(f'Revised chunk_cache       = {chunk_cache}')
  print(f'Revised userblock_size    = {user_blocksize}')
  print(f'Revised meta_blocksize    = {meta_blocksize}')
  print('--')

  # Close the files so we can reopen them in the high-level APID
  in_fid.close()
  out_fid.close()

  # Read from input and write to output
  try:

    beg_time = time.time()
    with h5py.File(args.infile, 'r') as src, \
         h5py.File(args.outfile, 'r+') as dst:
      copy_group(args, src, dst, '/')
    end_time = time.time()

  except Exception as e:
    print(f'Error reading/writing data - {e}')
    print(traceback.format_exc())
    in_fid.close()
    out_fid.close()
    sys.exit(3)

  # Print file sizes/change
  src_stats = os.stat(args.infile)
  dst_stats = os.stat(args.outfile)
  src_mb = src_stats.st_size / (1024 * 1024)
  dst_mb = dst_stats.st_size / (1024 * 1024)
  schange = (dst_mb / src_mb) * 100.
  print('--')
  print('Total read/write time=', end_time - beg_time)
  print(f'File Sizes (MiB): input={src_mb}, output={dst_mb}, change={schange}%')
  print('--')

#------------------------------------------------------------------------------
def run():

  args = init()
  optimize_file(args)

#-------------------------------------------------------------------------------
if __name__ == '__main__':
  run()
