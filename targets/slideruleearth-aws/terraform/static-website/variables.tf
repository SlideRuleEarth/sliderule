
variable "domain" {
  description = "domain name"
  default     = "slideruleearth.io"
}

variable "domain_root" {
  description = "domain name without TLD"
  default     = "slideruleearth"
}

variable "cost_grouping" {
  description = "the name tag to identify a grouping or subset of resources"
  type        = string
  default     = "docs"
}
