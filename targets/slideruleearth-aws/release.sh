# In order to complete the release process, the following must be installed and configured in your environment:
#     * GitHub command line client (gh)
#           See https://github.com/cli/cli/blob/trunk/docs/install_linux.md for installation instructions.
#           The user must authenticate to GitHub via `gh auth login`
#     * AWS command line client (awscli)
#           See https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html for installation instructions
#           The user must have up-to-date aws credentials
#     * Docker
#           The user must be logged into the AWS Elastic Container Registry (ECR)
#           The user must be logged into the GitHub Container Registry (GHCR)
#     * Terraform & Packer
#           The binaries are sufficient, but pay close attention to the local package versions
#     * Node.js
#			The javascript npm package for SlideRule is updated via a node.js script on release
#     * conda-lock
#           The Python base image used for the container runtime environment uses conda-lock to create the conda dependency file
#
# To release a version of SlideRule:
#     1. Update .aws/credentials file with a temporary session token; {profile} references your long term aws credentials, {username} is your aws account, {code} is your MFA token
#        $ aws --profile={profile} sts get-session-token --serial-number arn:aws:iam::742127912612:mfa/{username} --token-code={code}
#     2. Login to AWS Elastic Container Registry
#        $ aws ecr get-login-password --region us-west-2 | docker login --username AWS --password-stdin 742127912612.dkr.ecr.us-west-2.amazonaws.com
#     3. Login to GitHub
#        $ gh auth login
#     4. Login to NPMJS
#        $ cd sliderule/clients/nodejs && make login
#     5. Execute the makefile target to release the code; X.Y.Z is the version
#        $ ./release.sh VERSION=vX.Y.Z
#
# To update the build environment for GitHub actions
#     1. Login to GitHub Container Registry; {my_github_key} is a file storing an access key created for your account
#        $ cat {my_github_key} | docker login ghcr.io -u jpswinski --password-stdin
#     2. Build the docker build environment on x86 machine (x86 is IMPORTANT to match GitHub actions environment)
#        $ make sliderule-buildenv-docker
#     3. Push to GitHub container registry
#        $ docker push ghcr.io/slideruleearth/sliderule-buildenv:latest
#

VERSION=$1
echo "Releasing $VERSION"

# check version
if [[ "$VERSION" != "v"*"."*"."* ]]; then
    echo "Invalid version number"
    exit 1
fi
if git tag -l | grep -w $VERSION; then
    echo "Git tag already exists"
	exit 1
fi

# tag version
# make tag $VERSION

# build sliderule development environment docker image
make sliderule-buildenv-docker VERSION=$(VERSION) ARCH=aarch64 BUILDX=buildx DOCKER_PLATFORM="--platform linux/arm64" DOCKEROPTS=--load
make sliderule-buildenv-docker VERSION=$(VERSION) ARCH=x86_64 BUILDX=buildx DOCKER_PLATFORM="--platform linux/amd64" DOCKEROPTS=--load

#	make sliderule-buildenv-docker-push
#	make cluster-docker
#	make cluster-docker-push
#	make static-website-docker
#	make static-website-docker-push
#	make cluster-build-packer
#	make cluster-upload-terraform
#