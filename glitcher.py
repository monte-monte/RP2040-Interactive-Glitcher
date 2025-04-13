import os
import sys
import logging
import serial
import time
import math
import json
import importlib
from dataclasses import dataclass
from argparse import ArgumentParser

@dataclass
class Config:
    serial: str
    delay_min: int
    delay_max: int
    length_min: int
    length_max: int
    step: int
    freq: int
    in_gpio: int
    out_gpio: int
    in_dir: int
    out_dir: int
    reset_out_pin: int
    vcc_ctl_pin: int
    gnd_ctl_pin: int
    power_cycle_mode: int
    power_cycle_delay: int
    trig: int
    mod: int
    impl: str
    chip_model: str
    output: str

time_counter = 0

current_path = os.path.dirname(os.path.realpath(__file__))
config_path = os.path.join(current_path, 'config.json')

parser = ArgumentParser()
parser.add_argument('-c', '--config', default=None, help='Path to config file')
parser.add_argument('-o', '--output', default='output.bin', help='Path to output file')
parser.add_argument('-s', '--serial', default='/dev/ttyACM0', help='Serial port for Pico')
parser.add_argument('-d', '--delay', default=[50600, 90000], nargs=2, help='Delay min - max in ns')
parser.add_argument('-l', '--length', default=[1150, 5160], nargs=2, help='Glitch pulse length min - max in ns')
parser.add_argument('-t', '--step', default=100, help='Incrementing step in ns')
parser.add_argument('-f', '--freq', default=200000, help='Frequency in KHz to run Pico at')

parser.add_argument('--in-gpio', dest='in_gpio', default=3, help='GPIO number for trigger input')
parser.add_argument('--out-gpio', dest='out_gpio', default=26, help='GPIO number glitch pulse output')
parser.add_argument('--in-dir', dest='in_dir', default=1, help='Direction of input trigger 0/1 (FALLING/RISING edge)')
parser.add_argument('--out-dir', dest='out_dir', default=1, help='Direction of output glitch pulse 0/1 (LOW/HIGH)')
parser.add_argument('--reset-gpio', dest='reset_out_gpio', default=None, help='GPIO number for reset output')
parser.add_argument('--vcc-gpio', dest='vcc_ctl_gpio', default=0, help='GPIO number for VCC control')
parser.add_argument('--gnd-gpio', dest='gnd_ctl_gpio', default=29, help='GPIO number for GND control')
parser.add_argument('--power-cycle-mode', dest='power_cycle_mode', default=0, help='Power cycle target after fail? (0 - No, 1 - VCC, 2 - GND, 3 - Both)')
parser.add_argument('--power-cycle-delay', dest='power_cycle_delay', default=50, help='Power cycle delay ms between OFF/ON')

parser.add_argument('--trig', dest='trig', default=1, help='Wait for input trigger? 0/1 (NO/YES)')
parser.add_argument('--mod', dest='mod', default=0, help='Use custom glitch mode')
parser.add_argument('-i', '--impl', default='stm8_mod', action="store", dest="impl", help='Validation implementation module for desired target')
parser.add_argument('--model', default='stm8s005?6', action="store", dest="model", help='Chip model, if needed')
args = parser.parse_args()

config = Config (
  serial = args.serial,
  delay_min = args.delay[0],
  delay_max = args.delay[1],
  length_min = args.length[0],
  length_max = args.length[1],
  step = args.step,
  freq = args.freq,
  in_gpio = args.in_gpio,
  out_gpio = args.out_gpio,
  in_dir = args.in_dir,
  out_dir = args.out_dir,
  reset_out_pin = args.reset_out_gpio,
  vcc_ctl_pin = args.vcc_ctl_gpio,
  gnd_ctl_pin = args.gnd_ctl_gpio,
  power_cycle_mode = args.power_cycle_mode,
  power_cycle_delay = args.power_cycle_delay,
  trig = args.trig,
  mod = args.mod,
  impl = args.impl,
  chip_model = args.model,
  output = args.output,
)

if args.config != None:
  try:
    with open(args.config) as f:
      CONFIG = json.load(f)
      for key, value in CONFIG.items():
        vars(config)[key] = value
  except Exception as e:
    logging.error('Something wrong with config file.')
    logging.exception(e)
    sys.exit(1)

config.output = os.path.realpath(config.output)

if len(config.output) > 256:
  print(f"Output path is too long\n{config.output}\nMake sure it's 256 characters or less")
  sys.exit(1)

if os.path.isfile(config.output):
  overwrite = input(f'Otput file already exists:\n{config.output}\nDo you want to overwrite it? (y/N)\n')
  if overwrite.upper() != 'Y':
    print('Backup your files!')
    sys.exit(0)

impl = importlib.import_module(f'impl.{config.impl}')

tty = serial.Serial(
  port=config.serial,
  baudrate=115200,
)

def power_cycle():
  if config.power_cycle_mode == 0:
    return
  elif config.power_cycle_mode == 1:
    tty.write(b'V')
  elif config.power_cycle_mode == 2:
    tty.write(b'G')
  elif config.power_cycle_mode == 3:
    tty.write(b'P')

  # time.sleep(config.power_delay/1000)

def glitch_prepare(delay1, delay2, silent = False, trigger = False):
  tty.write(b'?')
  reply = tty.readline().decode()
  if reply.split(",")[0] == 'PIO is still running':
    tty.write(b'f')

  string = f'#{delay1}-{delay2}\n'.encode()
  if trigger == True:
    string = string + b'T'
    
  tty.write(string)
  reply = tty.readline().decode()
  if silent != True:
    print(reply);  

def glitch_success(delay1, delay2):
  glitch_prepare(i, x)
  print('Glitch successfull!')
  print(f'It took {math.floor(time.time() - time_counter)}s')
  choice = input("Do you want to retrieve full firmware(Y), or continue(N)?")
  sys.exit(0)
  if choice.upper() != 'Y':
    return
  
  while True:
    if impl.download(config.chip_model, config.output) == 0:
      print('Firmware retrieved!')
      input("Press Enter to continue...")
      tty.close()
      sys.exit(0)

def glitch(delay1, delay2, retries, trigger):
  glitch_prepare(delay1, delay2, trigger = trigger)
  test = impl.validate(config.chip_model, config.output)
  if test == 0:
    glitch_success(delay1, delay2)
  elif test == 2:
    for n in range(0, retries):
      power_cycle()
      sys.stdout.write("\033[F\033[K")
      print(f'Retrying: {n+1}')
      glitch_prepare(i, x, silent = True, trigger = trigger)
      test = impl.validate(config.chip_model, config.output, True)
      if test == 0:
        glitch_success(delay1, delay2)
      elif test == 1:
        break
      if n == retries - 1:
        print('Max retries reached')

  return test

def init():
  mode = 0
  if config.in_dir == 1 and config.out_dir == 1:
    mode = 0
  elif config.in_dir == 1 and config.out_dir == 0:
    mode = 1
  elif config.in_dir == 0 and config.out_dir == 1:
    mode = 2
  elif config.in_dir == 0 and config.out_dir == 0:
    mode = 3
  if config.mod == 1:
    mode = 4

  string = f'#O{config.out_gpio};'
  if config.in_gpio != None:
    string = string + f'I{config.in_gpio};'
  if config.reset_out_pin != None:
    string = string + f'E{config.reset_out_pin};'
  if config.vcc_ctl_pin != None:
    string = string + f'V{config.vcc_ctl_pin};'
  if config.gnd_ctl_pin != None:
    string = string + f'G{config.gnd_ctl_pin};'
  if config.power_cycle_mode != 0:
    string = string + f'D{config.power_cycle_delay}'

  string = string + f'M{mode};C{config.freq}\n'
  print(string)
  tty.write(string.encode())
  time.sleep(0.5)

tty.write(b'r');
tty.close()
print('Resetting Pico', end='')
sys.stdout.flush()

for i in range(5):
  print('.', end='')
  sys.stdout.flush()
  time.sleep(0.5)

print('\n\n')
tty.open()
# tty.write(b'\n');
init();
while(True):
  msg = tty.readline().decode();
  if msg.splitlines()[0] == "Ready":
    break
  print(msg);
# input("Press Enter to continue...")
power_cycle()
# time.sleep(1.5)
time_counter = time.time()

while True:
  for i in range(config.delay_min, config.delay_max+1, config.step):
    for x in range(config.length_min, config.length_max+1, 50):
      for y in range(0, 10):
        test = glitch(i, x, 25, not(config.trig))

tty.close()
sys.exit(0)
