# /// [Functional test]
async def test_hello_simple(service_client):
    response = await service_client.get('/hello')
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hello world'
    # /// [Functional test]

async def test_hello_to(service_client):
    response = await service_client.get('/hello/to/flusk')
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hello, flusk'

async def test_hi(service_client):
    response = await service_client.get('/hi?name=pal')
    assert response.status == 200
    assert 'text/plain' in response.headers['Content-Type']
    assert response.text == 'Hi, pal'

async def test_kv(service_client):
    response = await service_client.post('/kv?key=1&value=one')
    assert response.status == 200

    response = await service_client.get('/kv?key=1')
    assert response.status == 200
    assert response.text == 'one'

    response = await service_client.post('/kv?key=1&value=again_1')
    assert response.status == 200

    response = await service_client.get('/kv?key=1')
    assert response.status == 200
    assert response.text == 'again_1'
