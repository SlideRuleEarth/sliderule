import boto3
import argparse
import json
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

parser = argparse.ArgumentParser(description="""Storage Class Transfer""")
parser.add_argument('--bucket',     type=str,               default="sliderule")
parser.add_argument('--prefix',     type=str,               default="data/ATL24")
parser.add_argument('--target',     type=str,               default="DEEP_ARCHIVE")
parser.add_argument('--skip',       type=str, nargs='+',    default=['GLACIER', 'DEEP_ARCHIVE'])
parser.add_argument('--workers',    type=int,               default=40)
parser.add_argument('--change',     action='store_true',    default=False)
args = parser.parse_args()

s3 = boto3.client('s3')

def change_storage_class(key):
    if args.change:
        s3.copy_object(
            Bucket=args.bucket,
            CopySource={'Bucket': args.bucket, 'Key': key},
            Key=key,
            StorageClass=args.target
        )
    return key

total_object_count = 0
keys_to_change = []
paginator = s3.get_paginator('list_objects_v2')
for page in paginator.paginate(Bucket=args.bucket, Prefix=args.prefix):
    sys.stdout.write(".")
    sys.stdout.flush()
    for obj in page.get('Contents', []):
        total_object_count += 1
        storage_class = obj.get('StorageClass', 'STANDARD')  # absent means STANDARD
        if storage_class not in args.skip:
            keys_to_change.append(obj['Key'])
print(f"Changing {len(keys_to_change)} out of {total_object_count} objects")

changed = 0
failed = []
with ThreadPoolExecutor(max_workers=args.workers) as executor:
    futures = {executor.submit(change_storage_class, key): key for key in keys_to_change}
    for future in as_completed(futures):
        key = futures[future]
        try:
            future.result()
            changed += 1
            if changed % 500 == 0:
                print(f"Progress: {changed}/{len(keys_to_change)}")
        except Exception as e:
            print(f"Failed: {key} — {e}")
            failed.append(key)

print(f"\nFailed list:")
print(json.dumps(failed, indent=2))
print(f"\nDone. Changed: {changed}, Failed: {len(failed)}")
