variable "name_prefix" {
  type = string
  default = "sliderule"
}
variable "version" {
  type = string
  default = "latest"
}
variable "region" {
  type    = string
  default = "us-west-2"
}

source "amazon-ebs" "base-image" {
  tags = {
    Name = "sliderule-base-image"
  }
  ami_name              = "${var.name_prefix}-${var.version}"
  instance_type         = "t4g.micro"
  iam_instance_profile  = "sliderule_terraform"
  region                = var.region
  source_ami_filter {
    filters = {
      name                = "ubuntu/images/*ubuntu-focal-20.04-arm64-server-*"
      root-device-type    = "ebs"
      virtualization-type = "hvm"
    }
    most_recent = true
    owners      = ["099720109477"]
  }
  force_deregister = true
#  force_delete_snapshot = true
  ssh_username = "ubuntu"
}

# a build block invokes sources and runs provisioning steps on them.
build {
  sources = ["source.amazon-ebs.base-image"]

  provisioner "file"{
     source = "./"
     destination = "/tmp/"
  }

  provisioner "shell" {
    inline = [

      "sudo apt-get update",
      "sudo apt-get install -y ca-certificates curl gnupg lsb-release",
      "sudo mkdir -p /etc/apt/keyrings",
      "curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg",
      "echo \"deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable\" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null",
      "sudo apt-get update",
      "sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin",
      "sudo usermod -aG docker ubuntu",
      "sudo systemctl enable docker",

      "sudo apt-get update && sudo apt-get install -y zip curl",
      "curl -O 'https://awscli.amazonaws.com/awscli-exe-linux-aarch64.zip'",
      "unzip awscli-exe-linux-aarch64.zip",
      "sudo ./aws/install",

      "wget https://github.com/docker/compose/releases/download/v2.11.2/docker-compose-linux-aarch64",
      "sudo mv docker-compose-linux-aarch64 /usr/local/bin/docker-compose",
      "sudo chmod +x /usr/local/bin/docker-compose",

      "wget https://github.com/prometheus/node_exporter/releases/download/v1.2.2/node_exporter-1.2.2.linux-arm64.tar.gz",
      "tar -xvzf node_exporter-1.2.2.linux-arm64.tar.gz",
      "sudo mv node_exporter-1.2.2.linux-arm64/node_exporter /usr/local/bin/node_exporter",
      "sudo mv /tmp/node_exporter.service /etc/systemd/system/node_exporter.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable node_exporter.service",

      "wget https://github.com/grafana/loki/releases/download/v2.4.0/promtail-linux-arm64.zip",
      "unzip promtail-linux-arm64.zip",
      "sudo mv promtail-linux-arm64 /usr/local/bin/promtail",
      "sudo mv /tmp/promtail.yml /etc/",
      "sudo mv /tmp/promtail.service /etc/systemd/system/promtail.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable promtail.service",

      "wget https://github.com/grafana/loki/releases/download/v2.4.2/logcli-linux-arm64.zip",
      "unzip logcli-linux-arm64.zip",
      "sudo mv logcli-linux-arm64 /usr/local/bin/logcli",

      "sudo apt-get -y autoremove",
      "sudo apt-get -y clean",
    ]
  }
}
