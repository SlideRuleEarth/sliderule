# core
variable "cost_grouping" {
  description = "the name tag to identify all items created"
  type        = string
  default     = "demo"
}

variable "region" {
  description = "The AWS region to create resources in."
  default     = "us-west-2"
}

variable "az_count" {
  description = "Number of AZs to cover in a given AWS region"
  default     = "2"
}

variable "Name_tag" {
  description = "The global string to use for Name tag."
  default     = "demo"
}

# networking
variable "vpc_cidr" {
  description = "CIDR for the VPC"
  default     = "172.17.0.0/16"
}
variable "docker_image_url_demo-nginx" {
  description = "nginx Docker image to run in the ECS cluster"
  default = "MUST_SUPPLY"
}

# demo load balancer
variable "demo_health_check_path" {
  description = "Health check path for the default target group"
  default     = "/ping/"
}

# ecs

variable "docker_image_url_demo-client" {
  description = "static website jekyll Docker image to run in the ECS cluster"
  default = "MUST_SUPPLY"
}

# logs

variable "log_retention_in_days" {
  default = 30
}

# domain
variable "domain" {
  description = "root domain of site to use"
  # Must provide on cmd line
}

variable "domain_root" {
  description = "domain name of site to use without extension e.g. testsliderule"
  # Must provide on cmd line
}

# Fargate
variable "runtime_cpu_arch" {
  description = "The type of CPU to run container in"
  #default = "X86_64"
  default = "ARM64"
}
variable "task_count" {
  description = "Number of ECS Fargate tasks to run"
  default     = 1
}

variable "demo_task_cpu" {
  description = "Fargate task CPU units to provision (1 vCPU = 1024 CPU units)"
  default     = 1024
}

variable "demo_task_memory" {
  description = "Fargate task memory to provision (in MiB)"
  default     = 4096
}
#

variable "demo_container_port" {
    description = "port exposed by demo app container"
    default     = 8866
}
variable "nginx_container_port" {
    description = "port exposed by nginx app container"
    default     = 80
}

variable "auto_scale_min"{
  description = "minimum service instances"
  default = 1
}
variable "auto_scale_max"{
  description = "maximum service instances"
  default = 40
}
variable "mem_scale_target_value" {
  description = "optimal average utilization (percentage)"
  default = 25
}
variable "mem_scale_in_cooldown" {
  description = "scale in cooldown period in seconds"
  default = 300
}
variable "mem_scale_out_cooldown" {
  description = "scale out cooldown period in seconds"
  default = 60
}

variable "cpu_scale_target_value" {
  description = "optimal average utilization (percentage)"
  default = 75
}
variable "cpu_scale_in_cooldown" {
  description = "scale in cooldown period in seconds"
  default = 300
}
variable "cpu_scale_out_cooldown" {
  description = "scale out cooldown period in seconds"
  default = 60
}


variable "req_cnt_scale_target_value" {
  description = "Average Application Load Balancer request count per target for your Auto Scaling group"
  default = 3
}
variable "req_cnt_scale_in_cooldown" {
  description = "scale in cooldown period in seconds"
  default = 300
}
variable "req_cnt_scale_out_cooldown" {
  description = "scale out cooldown period in seconds"
  default = 60
}
