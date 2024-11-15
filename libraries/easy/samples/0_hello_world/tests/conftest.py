import pytest

from testsuite.databases.pgsql import discover

pytest_plugins = ['pytest_userver.plugins.postgresql']

@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    with open(service_source_dir.joinpath('schemas/postgresql/1.sql'), 'w') as f:
        f.write('CREATE TABLE FOO(int);')

    databases = discover.find_schemas(
        'admin', [service_source_dir.joinpath('schemas/postgresql')],
    )
    return pgsql_local_create(list(databases.values()))

