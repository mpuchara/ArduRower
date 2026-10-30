import asyncio,time
from bleak import BleakScanner,BleakClient

async def main():
 candidates=[]
 deadline=time.monotonic()+65
 while not candidates and time.monotonic()<deadline:
  devices=await BleakScanner.discover(timeout=3,return_adv=True)
  for d,a in devices.values():
   if a.local_name=='WR S4BL3' and any(x.startswith('00001826-') for x in a.service_uuids):
    candidates.append(d)
    print('TARGET',d.address,a.local_name,a.service_uuids,flush=True)
 if len(candidates)!=1:raise RuntimeError('Expected exactly one WR S4BL3 FTMS target')
 count=0
 def packet(_,data):
  nonlocal count
  count+=1
  if count<=5 or count%10==0:print('FTMS RX',count,data.hex(),flush=True)
 async with BleakClient(candidates[0],timeout=35,services=['00001826-0000-1000-8000-00805f9b34fb']) as client:
  print('FTMS LAPTOP CONNECTED',client.is_connected,flush=True)
  await client.start_notify('00002ad1-0000-1000-8000-00805f9b34fb',packet)
  await asyncio.sleep(25)
  await client.stop_notify('00002ad1-0000-1000-8000-00805f9b34fb')
  print('FTMS RECEIVED TOTAL',count,flush=True)
  if count<15:raise RuntimeError('Insufficient 1 Hz FTMS evidence')
asyncio.run(main())
