import asyncio
import bleak
bleak.register_uuids({
    "3b000001-1226-4a53-9d24-afa50c0163a3": "iotsa RGB LED service",
    "3b000002-1226-4a53-9d24-afa50c0163a3": "iotsa RGB LED value",
    "e4d90002-250f-46e6-90a4-ab98f01a0587": "iotsa USB voltage level",
    "e4d90003-250f-46e6-90a4-ab98f01a0587": "iotsa soft reboot",
    "6b2f0001-38bc-4204-a506-1d3546ad3688": "iotsa/lissabon lighting service",
    "6b2f0002-38bc-4204-a506-1d3546ad3688": "iotsa/lissabon light on",
    "6b2f0003-38bc-4204-a506-1d3546ad3688": "iotsa/lissabon identify fixture",
    "6b2f0004-38bc-4204-a506-1d3546ad3688": "iotsa/lissabon brightness",
    "6b2f0005-38bc-4204-a506-1d3546ad3688": "iotsa/lissabon temperature",
})

VERBOSE=True
FILTER=True

async def discover_devices_simple(duration=5):
    task =  bleak.discover(duration)
    devices = await task
    return devices

class BTError(RuntimeError):
    pass
   
class BTCharacteristic:
    def __init__(self, characteristic, service):
        self.characteristic = characteristic
        self.service = service
        
    def __repr__(self):
        return str(self.characteristic)
        
    def can_read(self):
        return 'read' in self.characteristic.properties
        
    def can_write(self):
        return 'write' in self.characteristic.properties
        
    def can_notify(self):
        return 'notify' in self.characteristic.properties
        
    def can_indicate(self):
        return 'indicate' in self.characteristic.properties
        
    async def read(self):
        try:
            rv = await self.service.device.read(self.characteristic.uuid)
        except asyncio.exceptions.TimeoutError as e:
            return f'Error: Timeout'
        except bleak.exc.BleakError as e:
            return f'Error: {e}'
        return rv
        
    def dump(self):
        print(f'\t\t\t{self.characteristic}')
         
class BTService:
    def __init__(self, service, device):
        self.service = service
        self.device = device
        self.characteristics = {}
        self._init_characteristics()
        
    def __getitem__(self, key):
        return self.characteristics[key]
        
    def __in__(self, key):
        return key in self.characteristics
        
    def __iter__(self):
        return self.characteristics
        
    def items(self):
        return self.characteristics.items()
        
    def _init_characteristics(self):
        for c in self.service.characteristics:
            id = c.description or c.uuid
            self.characteristics[id] = BTCharacteristic(c, self)    
        
    def dump(self):
        print(f'\t\tService {self.service.uuid} {self.service.description}:')
        for i, c in self.characteristics.items():
            print(f'\t\t- Item {i}:')
            c.dump()

class BTServer:
    def __init__(self, device):
        self.device = device
        self.client = None
        self.services = {}
        self.error = ''

    def __getitem__(self, key):
        return self.services[key]
        
    def __in__(self, key):
        return key in self.services
        
    def __iter__(self):
        return self.services
        
    def items(self):
        return self.services.items()

    def connect(self):
        return bleak.BleakClient(self.device.address)
        
#     async def disconnect(self):
#         self.client = None

    async def read(self, attr):
        async with self.connect() as client:
            #await client.get_services()
            #paired = await client.pair()
            rv = await client.read_gatt_char(attr)
        return rv
        
        
    async def get_device_services(self):
        error = None
        services = None
        if VERBOSE: print(f'+ get services: start {self.device.name} {self.device.address}')
        try:
            async with self.connect() as server:
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
                id = s.description or s.uuid
                self.services[id] = BTService(s, self)
        
    def dump(self):
        print(f'\tDevice {self.device.name} {self.device.address} {self.error}:')
        for i, s in self.services.items():
            print(f'\t- Item {i}:')
            s.dump()


class BT:
    def __init__(self):
        self.raw_devices = []
        self.devices = {}
        
    def __getitem__(self, key):
        return self.devices[key]
        
    def __in__(self, key):
        return key in self.devices
        
    def __iter__(self):
        return self.devices
    
    def items(self):
        return self.devices.items()
        
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
            id = d.name or d.address
            self.devices[id] = BTServer(d)
    
    def dump(self):
        print(f'{len(self.devices)} devices:')
        for i, d in self.devices.items():
            print(f'- Entry {i}:')
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
    
    for d_id, d in bt.items():
        print(f'{d_id}:')
        for s_id, s in d.items():
            print(f'\t{s_id}:')
            for c_id, c in s.items():
                print(f'\t\t{c_id}: can {"read" if c.can_read() else ""} {"write" if c.can_write() else ""}  {"notify" if c.can_notify() else ""} {"indicate" if c.can_indicate() else ""}')
                if c.can_read():
                    print(f'\t\t\t {await c.read()}')
                
                
        
    
asyncio.run(main())