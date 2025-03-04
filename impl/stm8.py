import os
import sys
import subprocess

stm8flash = os.path.join(os.path.realpath('.'), 'stm8flash/stm8flash')

def validate(model, output_file, silent = False):
  proc = subprocess.run([stm8flash, '-c', 'stlinkv2', '-p', model, '-r', output_file, '-b', '16'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
  
  if proc.returncode:
    if silent != True:
      print("stm8flash failed: ", proc.returncode)
    return 2

  f = open(output_file,"rb")
  out = f.read(16)
  if out == b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00':
    return 1
  else:
    return 0

def download(model, output_file, silent = False):
  proc = subprocess.run([stm8flash, '-c', 'stlinkv2', '-p', model, '-r', output_file], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  if proc.returncode:
    if silent != True:
      print("stm8flash failed to download flash: ", proc.returncode)
    return 2
  
  f = open(output_file,"rb")
  out = f.read(16)

  if out == b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00':
    return 1
  else:
    return 0