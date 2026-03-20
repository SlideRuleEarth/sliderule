#!/bin/bash

# Retry function
retry() {
local max_attempts=3
local attempt=1
local delay=5

until "$@"; do
    if [ $attempt -eq $max_attempts ]; then
    echo "Command failed after $max_attempts attempts: $*"
    return 1
    fi
    echo "Attempt $attempt failed. Retrying in $delay seconds..."
    sleep $delay
    delay=$((delay * 2))
    ((attempt++))
done
return 0
}

# Install and configure docker
retry dnf update -y
retry dnf install -y docker
systemctl enable docker
systemctl start docker
usermod -aG docker ec2-user

# Install Docker Compose V2
retry curl -L "https://github.com/docker/compose/releases/download/v2.24.5/docker-compose-linux-aarch64" -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose
mkdir -p /usr/local/lib/docker/cli-plugins
ln -s /usr/local/bin/docker-compose /usr/local/lib/docker/cli-plugins/docker-compose

# Log into ECR
aws ecr get-login-password --region $AWS_REGION  | docker login --username AWS --password-stdin $CONTAINER_REGISTRY

# Create docker-compose.yml
cat > docker-compose.yml << EOF
version: "3.9"
services:
monitor:
    image: $CONTAINER_REGISTRY/monitor:$VERSION
    container_name: monitor
    network_mode: host
    restart: always
    pull_policy: if_not_present
    labels:
    - autoheal=true
monitor-agent:
    image: $CONTAINER_REGISTRY/monitor-agent:$VERSION
    container_name: monitor-agent
    network_mode: host
    restart: unless-stopped
    pull_policy: if_not_present
    volumes:
    - /proc:/host/proc:ro
    - /sys:/host/sys:ro
    - /:/rootfs:ro
    - /var/log:/var/log:ro
    - /var/log/journal:/var/log/journal:ro
    - /run/log/journal:/run/log/journal:ro
    - /etc/machine-id:/etc/machine-id:ro
    - /var/lib/docker/containers:/var/lib/docker/containers:ro
    - /var/run/docker.sock:/var/run/docker.sock
    environment:
    - DISABLE_PROMTAIL=true
    labels:
    - autoheal=true
autoheal:
    image: willfarrell/autoheal:latest
    container_name: autoheal
    restart: always
    environment:
    - AUTOHEAL_CONTAINER_LABEL=all
    - AUTOHEAL_INTERVAL=30
    volumes:
    - /var/run/docker.sock:/var/run/docker.sock
EOF

# Start services
docker compose -p cluster up --detach
