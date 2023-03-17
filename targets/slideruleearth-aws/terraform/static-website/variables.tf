# core
variable "cost_grouping" {
  description = "the name tag to identify all items created"
  type        = string
  default     = "static-website"
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
  default     = "web"
}

# networking
variable "vpc_cidr" {
  description = "CIDR for the VPC"
  default     = "172.17.0.0/16"
}

variable "container_port" {
  description = "Port exposed by the docker image to redirect traffic to"
  default     = 4000
}

variable "task_count" {
  description = "Number of ECS tasks to run"
  default     = 1
}


# static-website load balancer
variable "static_web_health_check_path" {
  description = "Health check path for the default target group"
  default     = "/"
}

# ecs
variable "docker_image_url_static-website" {
  description = "static website jekyll Docker image to run in the ECS cluster"
  default = "MUST_SUPPLY"
}

# logs
variable "log_retention_in_days" {
  default = 30
}

# domain
variable "domain" {
  description = "root domain of site to use e.g. testsliderule.org"
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

variable "fargate_task_cpu" {
  description = "Fargate task CPU units to provision (1 vCPU = 1024 CPU units)"
  default     = 256
}

variable "fargate_task_memory" {
  description = "Fargate task memory to provision (in MiB)"
  default     = 512
}
