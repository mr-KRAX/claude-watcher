import pytest
import tempfile
import os


@pytest.fixture
def tmp_path(tmp_path_factory):
    """Override tmp_path to use a shorter base directory for AF_UNIX socket compatibility."""
    # Create a temp directory with a much shorter path
    base = tmp_path_factory.getbasetemp()
    short_base = tempfile.mkdtemp(dir="/tmp", prefix="t")
    return base.__class__(short_base)
