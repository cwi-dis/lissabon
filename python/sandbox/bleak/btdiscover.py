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
    task =  bleak.discover(duration)
    devices = await task
    return devices

class BTError(RuntimeError):
    pass
   
class BTCharacteristic:
    def __init__(self, characteristic, server):
        self.characteristic = characteristic
        self.server = server
         
class BTService:
    def __init__(self, service, device):
        self.service = service
        self.device = device
        self.characteristics = {}
        
        
    def dump(self):
        print(f'\t\tService {self.service.uuid} {self.service.description}:')
        for c in self.service.characteristics:
            print(f'\t\t\tCharacteristic {c}')

class BTServer:
    def __init__(self, device):
        self.device = device
        self.services = {}
        self.error = ''

    async def get_device_services(self):
        error = None
        services = None
        if VERBOSE: print(f'+ get services: start {self.device.name} {self.device.address}')
        try:
            async with bleak.BleakClient(self.device.address) as server:
                _services = await server.get_services()
        except bleak.exc.BleakError as e:
            error = str(e)
        except asyncio.exceptions.TimeoutError:
            error = 'Timeout'
        if VERBOSE: print(f'+ get services: done {self.device.name} {self.device.address}')
        if error:
            self.error = error
        else:
            for s in _services:
                self.services[s.uuid] = BTService(s, self)
        
    def dump(self):
        print(f'\tDevice {self.device.name} {self.device.address} {self.error}:')
        for s in self.services.values():
            s.dump()


class BT:
    def __init__(self):
        self.raw_devices = []
        self.devices = {}
        
    def discovery_callback(self, device, advertisement_data):
        print(f'discovery_callback({device.name}, {device.address}, {advertisement_data.service_uuids})')
    
    async def discover_devices(self, duration=5, filter=True):
        scanner = bleak.BleakScanner()
        scanner.register_detection_callback(self.discovery_callback)
        await scanner.start()
        await asyncio.sleep(duration)
        await scanner.stop()
        self.raw_devices = await scanner.get_discovered_devices()
        if filter:
            await self._filter_devices()
        else:
            for d in self.raw_devices:
                k = d.name if d.has_name() else d.address
                self.devices[k] = BTSserver(d)
        if VERBOSE: print(f'+ discovery: {len(self.raw_devices)} devices, {len(self.devices)} filtered devices')
    
    async def _filter_devices(self):
        if VERBOSE: print(f'filter_devices: {len(self.raw_devices)} devices:')
        def has_name(d): return d.name and d.name != 'Unknown'
        def has_battery(d): return 'battery' in d.metadata.get('uuids', [])
        #devices = filter(lambda d:has_name(d) and has_battery(d), devices)
        filtered = filter(lambda d: has_name(d), self.raw_devices)
        for d in filtered:
            k = d.name or d.address
            self.devices[k] = BTServer(d)
    
    def dump(self):
        print(f'{len(self.devices)} devices:')
        for d in self.devices.values():
            d.dump()

    def _unused():
        print(f'\t{len(services)} services:')
        for service in services:
            print(f'\t\t{service.uuid()} ({service.description()}):')
            for characteristic in service.characteristics:
                print(f'\t\t\t{characteristic}')

async def main():
    if VERBOSE: print('+ step 1: discovery: start')
    bt = BT()
    await bt.discover_devices(10, filter=FILTER)
    if VERBOSE: print('+ step 1: discovery: done')
    if VERBOSE:
        bt.dump()

    tasks = []
    if VERBOSE: print('+ step 3: get services: start')
    for device in bt.devices.values():
        tasks.append(asyncio.create_task(device.get_device_services()))
    if tasks:
        results = await asyncio.gather(*tasks)
    else:
        results = []
    
    if VERBOSE: print('+ step 3: get services: done')
    bt.dump()
    
asyncio.run(main())