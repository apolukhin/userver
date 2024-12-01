# /// [pgsql prepare]
import os
import pytest
import subprocess

from testsuite.databases.pgsql import discover

pytest_plugins = ["pytest_userver.plugins.postgresql"]

USERVER_CONFIG_HOOKS = ["userver_actions_service"]


@pytest.fixture(scope="session")
def userver_actions_service(mockserver_info):
    def do_patch(config_yaml, config_vars):
        components = config_yaml["components_manager"]["components"]
        components["action-client"]["service-url"] = mockserver_info.url("/v1/action")

    return do_patch


@pytest.fixture(scope="session")
def schema_path(service_binary, service_tmpdir):
    path = service_tmpdir.joinpath("schemas")
    os.mkdir(path)
    subprocess.run([service_binary, "--dump-db-schema", path / "0_pg.sql"])
    return path


@pytest.fixture(scope="session")
def pgsql_local(schema_path, pgsql_local_create):
    databases = discover.find_schemas("admin", [schema_path])
    return pgsql_local_create(list(databases.values()))
    # /// [pgsql prepare]