import os
import sys
import subprocess
import time

stm8flash = os.path.join(os.path.realpath('.'), 'stm8flash_retriever/stm8flash')

def validate(model, output_file, silent = False):
  proc = subprocess.run([stm8flash, '-c', 'stlinkv2', '-p', model, '-a', output_file], capture_output=True)
  
  if proc.returncode == 255:
    if silent != True:
      print("stm8flash failed: ", proc.returncode)
    return 2
  elif proc.returncode == 100:
    return 1
  else:
    time.sleep(1)
    print(proc.stderr.decode());
    return 0

def download(model, output_file, silent = False):
  return 0