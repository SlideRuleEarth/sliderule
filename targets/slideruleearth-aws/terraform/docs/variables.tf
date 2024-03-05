
variable "domain_apex" {
    description = "domain name without subdomain e.g. testsliderule.org"
    # Must provide on cmd line
}

variable "domain_name" {
    description = "full domain name of site to use with TLD e.g. docs.testsliderule.org"
    # Must provide on cmd line
}

variable "domain_root" {
    description = "domain name without subdomain e.g. testsliderule"
    # Must provide on cmd line
}

variable "cost_grouping" {
  description = "the name tag to identify a grouping or subset of resources"
  type        = string
  default     = "docs"
}