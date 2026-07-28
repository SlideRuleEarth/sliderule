"""Unit tests for common.config."""

from __future__ import annotations

import pytest

from common.config import Config, ConfigError, parse_email_list


def test_parse_email_list_dedupes_and_trims():
    result = parse_email_list("a@x.com, b@x.com ,, a@x.com")
    assert result == ("a@x.com", "b@x.com")


def test_from_env_requires_mandatory_values():
    with pytest.raises(ConfigError):
        Config.from_env({})


def test_from_env_builds_normalized_developer_set(config: Config):
    # Display names are normalized away for authorization checks.
    assert config.is_authorized_sender("dev1@slideruleearth.io")
    assert config.is_authorized_sender("dev2@slideruleearth.io")
    assert not config.is_authorized_sender("stranger@example.com")


def test_s3_key_for_applies_prefix(config: Config):
    assert config.s3_key_for("msg-1") == "email/support/msg-1"


def test_forward_from_defaults_to_support():
    env = {
        "DOMAIN": "slideruleearth.io",
        "DEVELOPER_EMAILS": "dev@slideruleearth.io",
        "SUPPORT_ADDRESS": "support@slideruleearth.io",
        "USERS_ADDRESS": "users@slideruleearth.io",
        "PROJECT_BUCKET": "bucket",
    }
    cfg = Config.from_env(env)
    assert cfg.forward_from_address == "support@slideruleearth.io"
    assert cfg.max_message_size == 10 * 1024 * 1024
