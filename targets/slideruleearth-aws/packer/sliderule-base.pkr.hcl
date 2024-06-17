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
      name                = "al2023-ami-2023.*-arm64"
      root-device-type    = "ebs"
      virtualization-type = "hvm"
    }
    most_recent = true
    owners      = ["137112412989"]
  }
  force_deregister = true
#  force_delete_snapshot = true
  ssh_username = "ec2-user"
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

      "sudo dnf -y upgrade --refresh",
      "sudo dnf -y install docker",
      "sudo usermod -aG docker ec2-user",

      "wget https://github.com/docker/compose/releases/download/v2.23.2/docker-compose-linux-aarch64",
      "sudo mv docker-compose-linux-aarch64 /usr/local/bin/docker-compose",
      "sudo chmod +x /usr/local/bin/docker-compose",

      "wget https://github.com/prometheus/node_exporter/releases/download/v1.7.0/node_exporter-1.7.0.linux-arm64.tar.gz",
      "tar -xvzf node_exporter-1.7.0.linux-arm64.tar.gz",
      "sudo mv node_exporter-1.7.0.linux-arm64/node_exporter /usr/local/bin/node_exporter",
      "sudo mv /tmp/node_exporter.service /etc/systemd/system/node_exporter.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable node_exporter.service",

      "wget https://github.com/grafana/loki/releases/download/v2.9.3/promtail-linux-arm64.zip",
      "unzip promtail-linux-arm64.zip",
      "sudo mv promtail-linux-arm64 /usr/local/bin/promtail",
      "sudo mv /tmp/promtail.yml /etc/",
      "sudo mv /tmp/promtail.service /etc/systemd/system/promtail.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable promtail.service",

      "wget https://github.com/grafana/loki/releases/download/v2.9.3/logcli-linux-arm64.zip",
      "unzip logcli-linux-arm64.zip",
      "sudo mv logcli-linux-arm64 /usr/local/bin/logcli",

      "sudo dnf -y clean all"
    ]
  }
}
