async def test_log_action(service_client, pgsql):
    response = await service_client.post("/log?action=test_1")
    assert response.status == 200

    cursor = pgsql["0_pg"].cursor()  # '0_pg.sql' is created by easy::PgDep, so the database is '0_pg'
    cursor.execute("SELECT action, host_id FROM events_table WHERE id=1")
    result = cursor.fetchall()
    assert len(result) == 1
    assert result[0][0] == "test_1"
    assert result[0][1] == 42
