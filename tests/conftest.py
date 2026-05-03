import pytest

def pytest_addoption(parser):
    parser.addoption(
        "--fast", action="store_true", default=False,
        help="Skip slow tests (pairing-heavy, >5s each). Run in under 30s."
    )

def pytest_configure(config):
    config.addinivalue_line(
        "markers", "slow: pairing-heavy test (skipped with --fast)"
    )

def pytest_collection_modifyitems(config, items):
    if config.getoption("--fast"):
        skip = pytest.mark.skip(reason="--fast: skipping pairing-heavy test")
        for item in items:
            if "slow" in item.keywords:
                item.add_marker(skip)
