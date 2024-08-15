# Input Variables
variable "architecture" {
  type    = string
}
variable "docker_compose_arch" {
  type    = string
}
variable "node_exporter_arch" {
  type    = string
}
variable "loki_arch" {
  type    = string
}
variable "source_ami" {
  type    = string
}
variable "source_owner" {
  type    = string
}
variable "build_instance" {
  type    = string
}

# Default Variables
variable "pkgmgr" {
  type    = string
  default = "dnf"
}
variable "version" {
  type    = string
  default = "latest"
}
variable "region" {
  type    = string
  default = "us-west-2"
}
variable "docker_compose_version" {
  type    = string
  default = "2.29.1"
}
variable "node_exporter_version" {
  type    = string
  default = "1.8.2"
}
variable "loki_version" {
  type    = string
  default = "2.9.9"
}

# Base Image
source "amazon-ebs" "base-image" {
  tags = {
    Name = "sliderule-base-image"
  }
  ami_name              = "sliderule-${var.architecture}-${var.version}"
  instance_type         = "${var.build_instance}"
  iam_instance_profile  = "sliderule_terraform"
  region                = var.region
  source_ami_filter {
    filters = {
      name                = "${var.source_ami}"
      root-device-type    = "ebs"
      virtualization-type = "hvm"
    }
    most_recent = true
    owners      = ["${var.source_owner}"]
  }
  force_deregister = true
  ssh_username = "ec2-user"
}

# Build Steps
build {
  sources = ["source.amazon-ebs.base-image"]

  provisioner "file"{
     source = "./"
     destination = "/tmp/"
  }

  provisioner "shell" {
    inline = [

      "sudo ${var.pkgmgr} -y upgrade",
      "sudo ${var.pkgmgr} -y install docker",
      "sudo usermod -aG docker ec2-user",

      "wget https://github.com/docker/compose/releases/download/v${var.docker_compose_version}/docker-compose-linux-${var.docker_compose_arch}",
      "sudo mv docker-compose-linux-${var.docker_compose_arch} /usr/local/bin/docker-compose",
      "sudo chmod +x /usr/local/bin/docker-compose",

      "wget https://github.com/prometheus/node_exporter/releases/download/v${var.node_exporter_version}/node_exporter-${var.node_exporter_version}.linux-${var.node_exporter_arch}.tar.gz",
      "tar -xvzf node_exporter-${var.node_exporter_version}.linux-${var.node_exporter_arch}.tar.gz",
      "sudo mv node_exporter-${var.node_exporter_version}.linux-${var.node_exporter_arch}/node_exporter /usr/local/bin/node_exporter",
      "sudo mv /tmp/node_exporter.service /etc/systemd/system/node_exporter.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable node_exporter.service",

      "wget https://github.com/grafana/loki/releases/download/v${var.loki_version}/promtail-linux-${var.loki_arch}.zip",
      "unzip promtail-linux-${var.loki_arch}.zip",
      "sudo mv promtail-linux-${var.loki_arch} /usr/local/bin/promtail",
      "sudo mv /tmp/promtail.yml /etc/",
      "sudo mv /tmp/promtail.service /etc/systemd/system/promtail.service",
      "sudo systemctl daemon-reload",
      "sudo systemctl enable promtail.service",

      "wget https://github.com/grafana/loki/releases/download/v${var.loki_version}/logcli-linux-${var.loki_arch}.zip",
      "unzip logcli-linux-${var.loki_arch}.zip",
      "sudo mv logcli-linux-${var.loki_arch} /usr/local/bin/logcli",

      "sudo ${var.pkgmgr} -y clean all"
    ]
  }
}
