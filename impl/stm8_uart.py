import sys
import subprocess
import serial
import time

uart = serial.Serial(
  port='/dev/ttyUSB0',
  baudrate=115200,
  dsrdtr = True,
)

def validate(model, output_file):
  uart.dtr = 1
  # time.sleep(0.1)
  uart.dtr = 0
  start = time.time_ns()
  test = uart.readline()
  end = time.time_ns()
  print(test, end-start)
  time.sleep(0.1)
  if test == b'170\r\n':
    return 1
  else:
    return 0

def download(model, output_file):
  print("Glitch seems successfull")
  input("Press Enter to continue...")
  # sys.exit()