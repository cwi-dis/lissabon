import asyncio
import bleak
bleak.uuids.uuid128_dict["3b000001-1226-4a53-9d24-afa50c0163a3"] = "iotsa RGB LED service"
bleak.uuids.uuid128_dict["3b000002-1226-4a53-9d24-afa50c0163a3"] = "iotsa RGB LED value"
bleak.uuids.uuid16_dict[0x2A19] = "Battery Level"
bleak.uuids.uuid128_dict["e4d90002-250f-46e6-90a4-ab98f01a0587"] = "iotsa USB voltage level"
bleak.uuids.uuid128_dict["e4d90003-250f-46e6-90a4-ab98f01a0587"] = "iotsa soft reboot"
bleak.uuids.uuid128_dict["6b2f0001-38bc-4204-a506-1d3546ad3688"] = "iotsa/lissabon lighting service"
bleak.uuids.uuid128_dict["6b2f0002-38bc-4204-a506-1d3546ad3688"] = "iotsa/lissabon light on"
bleak.uuids.uuid128_dict["6b2f0003-38bc-4204-a506-1d3546ad3688"] = "iotsa/lissabon identify fixture"
bleak.uuids.uuid128_dict["6b2f0004-38bc-4204-a506-1d3546ad3688"] = "iotsa/lissabon brightness"
bleak.uuids.uuid128_dict["6b2f0005-38bc-4204-a506-1d3546ad3688"] = "iotsa/lissabon temperature"

VERBOSE=True
FILTER=True

async def discover_devices_simple(duration=5):
    devices = await bleak.discover(duration)
    return devices
    
def discovery_callback(device, advertisement_data):
    print(f'discovery_callback({device.name}, {device.address}, {advertisement_data.service_uuids})')
    
async def discover_devices(duration=5):
    scanner = bleak.BleakScanner()
    scanner.register_detection_callback(discovery_callback)
    await scanner.start()
    await asyncio.sleep(duration)
    await scanner.stop()
    devices = await scanner.get_discovered_devices()
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
    error = None
    services = None
    if VERBOSE: print(f'+ get services: start {device.name} {device.address}')
    try:
        async with bleak.BleakClient(device.address) as server:
            services = await server.get_services()
    except bleak.exc.BleakError as e:
        error = str(e)
    except asyncio.exceptions.TimeoutError:
        error = 'Timeout'
    if VERBOSE: print(f'+ get services: done {device.name} {device.address}')
    return device, services, error

def _unused():
    print(f'\t{len(services)} services:')
    for service in services:
        print(f'\t\t{service.uuid()} ({service.description()}):')
        for characteristic in service.characteristics:
            print(f'\t\t\t{characteristic}')

async def main():
    if VERBOSE: print('+ step 1: discovery: start')
    devices = await discover_devices(10)
    if VERBOSE: print('+ step 1: discovery: done')
    if VERBOSE:
        print(f'{len(devices)} devices:')
        for d in devices:
            print(f'\tname={d.name}, address={d.address}')

    if FILTER:
        if VERBOSE: print('+ step 2: filter: start')
        devices = await filter_devices(devices)
        if VERBOSE: print('+ step 2: filter: done')
        if VERBOSE:
            print(f'{len(devices)} filtered devices:')
            for d in devices:
                print(f'\tname={d.name}, address={d.address}')

    tasks = []
    if VERBOSE: print('+ step 3: get services: start')
    for device in devices:
        tasks.append(asyncio.create_task(get_device_services(device)))
    if tasks:
        results = await asyncio.gather(*tasks)
    else:
        results = []
    
    if VERBOSE: print('+ step 3: get services: done')
    for device, services, error in results:
        print(f'Device {device}')
        if services:
            for service in services:
                print(f'\tService {service}')
        elif error:
            print(f'\t{error}')
        else:
            print(f'\tunknown')
    print('all done')
    
asyncio.run(main())