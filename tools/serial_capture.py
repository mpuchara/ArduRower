import argparse,time
from pathlib import Path
import serial
p=argparse.ArgumentParser()
p.add_argument('--port',default='COM6')
p.add_argument('--seconds',type=float,default=180)
p.add_argument('--log',default='work/device-live.log')
p.add_argument('--commands',default='work/device-command.txt')
a=p.parse_args()
s=serial.Serial(port=None,baudrate=115200,timeout=0.1)
s.dtr=False;s.rts=False;s.port=a.port;s.open()
deadline=time.monotonic()+a.seconds
last_command_mtime=0
Path(a.log).parent.mkdir(parents=True,exist_ok=True)
with open(a.log,'ab',buffering=0) as f:
 while time.monotonic()<deadline:
  if Path(a.log+'.stop').exists():break
  b=s.read(8192)
  if b:
   f.write(b);print(b.decode(errors='replace'),end='',flush=True)
  c=Path(a.commands)
  if c.exists():
   modified=c.stat().st_mtime_ns
   command=c.read_text().strip()
   if command and modified!=last_command_mtime:
    s.write((command+'\n').encode());last_command_mtime=modified
s.close()

