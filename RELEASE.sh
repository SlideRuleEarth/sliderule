#!/bin/bash

#
# Acceptable version identifier is vX.Y.Z, e.g. v1.0.4
#
VERSION=$1
if [[ "$VERSION" != "v"*"."*"."* ]]; then
    echo "Invalid version number"
    exit 1
fi
if git tag -l | grep -w $VERSION; then
    echo "Git tag already exists"
	exit 1
fi

#
# Clean up any previously attempted archives
#
rm sliderule-$VERSION.tar.gz 2> /dev/null

#
# Update version in local repository
#
echo $VERSION > version.txt
git add version.txt
git commit -m "Version $VERSION"

#
# Create tag and acrhive
#
git tag -a $VERSION -m "version $VERSION"
git archive --format=tar.gz --prefix=sliderule/ $VERSION > sliderule-$VERSION.tar.gz

