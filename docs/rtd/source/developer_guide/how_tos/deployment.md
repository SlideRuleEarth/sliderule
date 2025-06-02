# Checklist for Deploying Public Cluster

2025-05-30

## Overview

Provided are a list of steps needed when a new SlideRule release is deployed as the public cluster.

SlideRule uses a Blue-Green deployment strategy where the new release is first deployed as a _cold_ cluster that is not associated with the public "sliderule.slideruleearth.io" domain.  The _cold_ cluster is tested and once validated, the "sliderule.slideruleearth.io" domain is updated to point to it, thereby switching it to become the live or _hot_ cluster.  The previously deployed cluster is now the _cold_ cluster and is left running until the time-to-live on the DNS entries expire and all processing requests have completed.  This should take at most 15 minutes (TTL of 5 minutes + Maximum Request Time of 10 minutes).  The _cold_ cluster is then destroyed.

## Checklist

#### (1) Check which color the _hot_ cluster is running as

At targets/slideruleearth-aws/:
```
make public-cluster-status COLOR=green
make public-cluster-status COLOR=blue
```

Only one of the colors should have actively deployed resources in AWS.  Consider the active color as the _hot_ cluster, and use the inactive color for the _cold_ cluster.

#### (2) Deploy the new release as the _cold_ cluster color

At targets/slideruleearth-aws/:
```
make public-cluster-deploy COLOR=<cold> VERSION=<new release version>
```

The `<cold>` is the _cold_ color identified in the step above.  The `<new release version>` is the version of the code that is to be deployed.

#### (3) Test out the _cold_ cluster

Using `sliderule-<cold>` as the organization, run the __pytests__ against the newly deployed cluster.

#### (4) Switch to the _cold_ cluster

At targets/slideruleearth-aws/:
```
make public-cluster-go-live COLOR=<cold> PUBLIC_IP=<ip>
```

This will update the DNS entry for "sliderule.slideruleearth.io" to point to the newly deployed cluster.  The `<ip>` address supplied in the command is the `ilb_ip_address` output from the deployment of the _cold_ cluster.

#### (5) Wait 30 minutes

This will allow DNS caches to flush and any active requests on the old cluster to complete.

#### (6) Run usage report and save off manager database

At clients/python/utils/:
```
python usage_report.py
```

This will export the existing database to S3 and run a final usage report on the old cluster.

#### (7) Destroy the old cluster

At targets/slideruleearth-aws/:
```
make public-cluster-destroy COLOR=<hot>
```

The `<hot>` color is the color of the old cluster that we want to now destroy.


