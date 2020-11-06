# python

import sys

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Build database
    db = []
    for line in sys.stdin:
        if "s3Client.getS3Bytes" in line:
            entry = {}
            tokens = line.split(" ")
            for token in tokens:
                values = token.split("=")
                if "start=" in token:
                    entry["start"] = float(values[1].strip())
                elif "finish=" in token:
                    entry["finish"] = float(values[1].strip())
                elif "elapsed=" in token:
                    entry["elapsed"] = float(values[1].strip())
                elif "bytes=" in token:
                    entry["bytes"] = int(values[1].strip())
            db.append(entry)

    # Calculate rate statistics
    for entry in db:
        entry["rate"] = entry["bytes"] / entry["elapsed"]
    
    # Calculate time extent
    start_time = min([entry["start"] for entry in db])
    stop_time = max([entry["finish"] for entry in db])
    total_duration = stop_time - start_time

    # Output results
    sys.stderr.write("Total Duration: %d\n" % (total_duration))
    sys.stdout.write("elapsed,bytes,rate\n")
    for entry in db:
        sys.stdout.write("%f,%d,%f\n" % (entry["elapsed"], entry["bytes"], entry["rate"]))
