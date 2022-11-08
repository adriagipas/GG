from distutils.core import setup, Extension

module= Extension ( 'GG',
                    sources= [ 'ggmodule.c',
                               '../src/io.c',
                               '../src/control.c',
                               '../src/main.c',
                               '../src/mem.c',
                               '../src/psg.c',
                               '../src/rom.c',
                               '../src/vdp.c',
                               'Z80/src/z80.c',
                               'Z80/src/z80_dis.c' ],
                    depends= [ '../src/GG.h', 'Z80/src/Z80.h' ],
                    libraries= [ 'SDL', 'GL' ],
                    include_dirs= [ '../src', 'Z80/src' ] )

setup ( name= 'GG',
        version= '1.0',
        description= 'Game Gear simulator',
        ext_modules= [module] )
