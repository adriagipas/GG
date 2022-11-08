import GG
import sys

if len(sys.argv)!=2:
    sys.exit('%s <ROM>'%sys.argv[0])
rom_fn= sys.argv[1]

GG.init()
with open(rom_fn,'rb') as f:
    GG.set_rom(f.read())
rom= GG.get_rom()
print ( 'Checksum: %d'%rom['checksum'] )
print ( 'Product code: %d'%rom['code'] )
print ( 'Version: %d'%rom['version'] )
print ( 'Region code: '+rom['region'] )
print ( 'ROM size: '+rom['size'] )
GG.loop()
GG.close()
