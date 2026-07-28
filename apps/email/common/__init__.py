"""Reusable modules for the SlideRule SES mail gateway.

This package holds the shared, side-effect-light building blocks used by the
``support`` and ``users`` Lambda handlers:

* :mod:`common.config`  -- typed, immutable configuration loaded from the
  Lambda environment.
* :mod:`common.logging` -- structured (JSON) logging helpers.
* :mod:`common.mail`    -- MIME parsing and forward-message reconstruction.
* :mod:`common.ses`     -- thin wrappers around the SES / SES v2 APIs.
* :mod:`common.storage` -- retrieval of archived MIME messages from S3.

The modules are intentionally free of module-level mutable state so that they
remain easy to unit test in isolation.
"""

__all__ = [
    "config",
    "logging",
    "mail",
    "ses",
    "storage",
]
