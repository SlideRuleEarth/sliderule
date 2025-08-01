# ------------------------------------------------------------
# Remote access
# ------------------------------------------------------------
variable "key_pair_name" {
  description = "The EC2 Key Pair to associate with the EC2 Instance for SSH access."
  type        = string
  default     = "surface"
}

# -------------------------------------------------------------
# AWS Variables
# -------------------------------------------------------------
variable "aws_region" {
  description = "The AWS region to deploy into"
  type        = string
  default     = "us-west-2"
}
variable  "availability_zone" {
  description = "The AWS availability zone to deploy into"
  type        = string
  default     = "us-west-2d"
}

# -------------------------------------------------------------
# Network Variables
# -------------------------------------------------------------
variable "manager_ip" {
  type        = string
  default     = "10.0.1.3"
}
variable "monitor_ip" {
  type        = string
  default     = "10.0.1.4"
}
variable "ilb_ip" {
  type       = string
  default    = "10.0.1.5"
}
variable "vpcCIDRblock" {
  type       = string
  default    = "10.0.0.0/16"
}
variable "publicCIDRblock" {
  type       = string
  default    = "0.0.0.0/0"
}

#----------------------------------------------------------------
# Deployment Variables
#----------------------------------------------------------------
variable "cluster_name" {
  description = "name for the sliderule cluster"
  type        = string
  default     = "sliderule"
}
variable "cluster_version" {
  description = "version of docker containers to use for sliderule cluster"
  type        = string
  default     = "latest"
}
variable "node_asg_desired_capacity" {
  description = "number of nodes in sliderule cluster"
  type        = number
  default     = 7
}
variable "node_asg_min_capacity" {
  description = "minimum number of nodes in sliderule cluster"
  type        = number
  default     = 0
}
variable "node_asg_max_capacity" {
  description = "maxmum number of nodes in sliderule cluster"
  type        = number
  default     = 7
}
variable "domain" {
  description = "root domain of sliderule cluster"
  default     = "slideruleearth.io"
}
variable "is_public" {
  description = "(True/False): The cluster is public (Note: public clusters do NOT require authentication)"
  default     = "False"
}
variable "container_repo" {
  description = "container registry holding sliderule container images"
  default     = "742127912612.dkr.ecr.us-west-2.amazonaws.com"
}
variable "cluster_volume_size" {
  description = "EC2 instance disk space in GBs"
  type        = number
  default     = 64
}
variable "spot_max_price" {
  description = "maximum price sliderule cluster will pay for spot instances"
  type        = string
  default     = "0.18"
}
variable "spot_allocation_strategy" {
  description = "strategy for allocating spot instances"
  type        = string
  default     = "lowest-price"
}
variable "organization_name" {
  description = "name for the sliderule organization"
  type        = string
  default     = ""
}
locals {
  organization = (
    var.organization_name != "" ? var.organization_name : var.cluster_name
  )
}

# ------------------------------------------------------------
# Provisioning System
# ------------------------------------------------------------
variable "prov_sys_handshake" {
  description = "Well known output the provisioning system can key off of on a refresh command"
  type        = string
  default     = "sliderule"
}
