import argparse
import sys
import boto3

# command line arguments
parser = argparse.ArgumentParser(description="""ATL24 Platinum Run""")
parser.add_argument('--source', type=str,   default="s3://sliderule-public/atl24r3/xml")
args = parser.parse_args()

# display raw
def display(s):
    sys.stdout.write(s)
    sys.stdout.flush()

# parse URL into bucket and subfolder
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    subfolder = '/'.join(path.split("/")[1:])
    return bucket, subfolder

# load remote file from s3
def load_remote_file(bucket, key):
    obj = s3.get_object(Bucket=bucket, Key=key)
    return obj["Body"].read().decode("utf-8")

# store remote file to s3
def store_remote_file(bucket, key, content):
    s3.put_object(Bucket=bucket, Key=key, Body=content.encode("utf-8"))

# globals
s3 = boto3.client("s3")
bucket, subfolder = parse_url(args.source)

# enumerate objects in s3 bucket
xml_filenames = []
is_truncated = True
continuation_token = None
while is_truncated:
    if continuation_token:  response = s3.list_objects_v2(Bucket=bucket, Prefix=subfolder, ContinuationToken=continuation_token)
    else:                   response = s3.list_objects_v2(Bucket=bucket, Prefix=subfolder)
    display("#")
    if 'Contents' in response:
        for obj in response['Contents']:
            resource = obj['Key'].split("/")[-1]
            if resource.endswith(".xml"):
                xml_filenames.append(resource)
    is_truncated = response['IsTruncated']
    continuation_token = response.get('NextContinuationToken')
display("\n")

# process each file
print(f"Processing {len(xml_filenames)} files")
for xml_filename in xml_filenames:
    path = f"{subfolder}/{xml_filename}"
    content = load_remote_file(bucket, path)
    print(f"Fixing {path}")
    # remove double quotes from date times
    content = content.replace("<gco:DateTime>\"", "<gco:DateTime>")
    content = content.replace("\"</gco:DateTime>", "</gco:DateTime>")
    # change version to release
    content = content.replace("<gco:CharacterString>001</gco:CharacterString>", "<gco:CharacterString>003</gco:CharacterString>")
    # polygon is lat lon, not lon lat
    start = '<gml:posList srsDimension="2" srsName="http://www.opengis.net/def/crs/EPSG/4326">'
    end = '</gml:posList>'
    poly_old_str = content[content.find(start) + len(start):content.find(end)]
    poly_old_list = poly_old_str.split()
    poly_new_list = []
    for i in range(0, len(poly_old_list), 2):
        poly_new_list.append(poly_old_list[i+1])
        poly_new_list.append(poly_old_list[i])
    poly_new_str = ' '.join(poly_new_list)
    content = content.replace(poly_old_str, poly_new_str)
    store_remote_file(bucket, path, content)
