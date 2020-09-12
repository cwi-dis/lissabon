import asyncio
import bleak
import sys
from bleak.backends.device import BLEDevice

VERBOSE=True
FILTER=False

async def discover_devices(duration=5):
    devices = await bleak.discover(duration)
    return devices
    
async def filter_devices(devices):
    print(f'{len(devices)} devices:')
    def has_name(d): return d.name and d.name != 'Unknown'
    def has_battery(d): return 'battery' in d.metadata.get('uuids', [])
    #devices = filter(lambda d:has_name(d) and has_battery(d), devices)
    devices = filter(lambda d: has_name(d), devices)
    devices = list(devices)
    return devices
    
async def get_device_services(device):
    try:
        async with bleak.BleakClient(device.address) as server:
            services = await server.get_services()
    except bleak.exc.BleakError:
        services = None
    return device, services

def _unused():
    print(f'\t{len(services)} services:')
    for service in services:
        print(f'\t\t{service}:')
        for characteristic in service.characteristics:
            print(f'\t\t\t{characteristic}')

async def main():
    devices = [BLEDevice(addr, None) for addr in sys.argv[1:]]
    tasks = []
    for device in devices:
        tasks.append(asyncio.create_task(get_device_services(device)))
    if tasks:
        results = await asyncio.gather(*tasks)
    else:
        results = []
    
    for device, services in results:
        print(f'Device {device}')
        if services:
            for service in services:
                print(f'\tService {services}')
        else:
            print(f'\tunknown')
    print('all done')
    
asyncio.run(main())