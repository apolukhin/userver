# /// [psql prepare]
import os
import pytest
import subprocess

from testsuite.databases.pgsql import discover

pytest_plugins = ['pytest_userver.plugins.postgresql']


@pytest.fixture(scope='session')
def pgsql_local(service_tmpdir, service_binary, pgsql_local_create):
    schema_path = service_tmpdir.joinpath('schemas')
    os.mkdir(schema_path)
    subprocess.run([service_binary, '--dump-schema', schema_path]) 
    databases = discover.find_schemas('admin', [schema_path])
    return pgsql_local_create(list(databases.values()))
    # /// [psql prepare]
