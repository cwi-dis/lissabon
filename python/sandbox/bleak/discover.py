import asyncio
import bleak

async def run_discover_devices():
    devices = await bleak.discover()
    print(f'{len(devices)} devices:')
    def has_name(d): return d.name and d.name != 'Unknown'
    def has_battery(d): return 'battery' in d.metadata.get('uuids', [])
    devices = filter(lambda d:has_name(d) and has_battery(d), devices)
    devices = list(devices)
    for device in devices:
        print(f'\tdevice id {device.address} name {device.name} metadata {device.metadata.get("uuids", [])}')
    return devices
    
async def get_device_services(devices):
    for device in devices:
        print(f'\tdevice id {device.address} name {device.name}')
        try:
            async with bleak.BleakClient(device.address) as server:
                services = await server.get_services()
                print(f'\t{len(services)} services:')
                for service in services:
                    print(f'\t\t{service.uuid}:')
                    for characteristic in service.characteristics:
                        print(f'\t\t\t{characteristic}')
        except bleak.exc.BleakError as arg:
            print(f'\t\t* error {arg}')
        except KeyboardInterrupt:
            print(f'\t\t* skipped')
    return devices

devices = asyncio.run(run_discover_devices())
print('now getting services')
_ = asyncio.run(get_device_services(devices))
print('all done')