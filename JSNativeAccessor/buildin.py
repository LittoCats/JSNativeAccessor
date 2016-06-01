#!/usr/bin/python
#encoding=utf-8

import os
import sys
import commands
import zlib

pwd = os.path.split(sys.argv[0])[0]
os.chdir(pwd)

# 编译 coffee 文件
def compile_coffee(src):
  cmd = "coffee -cpb " + src + " | uglifyjs -m"
  return commands.getoutput(cmd)

def write_file(content, dst):
  f = open(dst, 'w')
  f.write(content)
  f.close()
  

def main():
  js = compile_coffee('JSNativeAccessor.coffee')
  bs = ','.join([hex(b) for b in bytearray(js)])
  content = "static char BuildInScript[] = {" + bs + ",0x00};"

  write_file(content, 'JSNativeAccessor')


if __name__ == '__main__':
  main()