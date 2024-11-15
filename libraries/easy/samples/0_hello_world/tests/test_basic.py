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
