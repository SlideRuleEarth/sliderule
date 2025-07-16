
variable "domain" {
    description = "domain name e.g. testsliderule.org"
    # Must provide on cmd line
}

variable "domain_root" {
    description = "domain name without TLD e.g. testsliderule"
    # Must provide on cmd line
}

variable "cost_grouping" {
  description = "the name tag to identify a grouping or subset of resources"
  type        = string
  default     = "docs"
}
