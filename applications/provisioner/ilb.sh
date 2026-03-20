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
retry curl -L "https://github.com/docker/compose/releases/download/${DockerComposeVersion}/docker-compose-linux-aarch64" -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose
mkdir -p /usr/local/lib/docker/cli-plugins
ln -s /usr/local/bin/docker-compose /usr/local/lib/docker/cli-plugins/docker-compose

# Log into ECR
aws ecr get-login-password --region ${AWS::Region} | docker login --username AWS --password-stdin ${ContainerRegistry}

# Setup directories and download files
mkdir -p /etc/haproxy/pem
retry curl ${JwtIssuer}/auth/pem > /etc/haproxy/pem/pubkey.pem
mkdir -p /etc/ssl/private
aws s3 cp s3://${ProjectBucket}/${ProjectFolder}/${Domain}.pem /etc/ssl/private/${Domain}.pem

# Create docker-compose.yml
cat > docker-compose.yml << EOF
version: "3.9"
services:
ilb:
    image: ${ContainerRegistry}/ilb:${Version}
    container_name: ilb
    network_mode: host
    restart: always
    pull_policy: if_not_present
    volumes:
    - /haproxy:/haproxy
    - /etc/ssl/private:/etc/ssl/private
    - /etc/haproxy/pem/:/etc/haproxy/pem/
    environment:
    - IS_PUBLIC=${IsPublic}
    - DOMAIN=${Domain}
    - CLUSTER=${Cluster}
    - JWT_ISSUER=${JwtIssuer}
    healthcheck:
    test: curl -f http://localhost:8050/discovery/health
    interval: 30s
    timeout: 10s
    retries: 1
    start_period: 30s
    labels:
    - autoheal=true
monitor-agent:
    image: ${ContainerRegistry}/monitor-agent:${Version}
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
