#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import GG
import sys
from array import array

class Inst:
    
    __BYTE_OPTS= set([GG.BYTE, GG.pBYTE])
    __DESP_OPTS= set([GG.pIXd, GG.pIYd])
    __ADDR_WORD_OPTS= set([GG.ADDR, GG.WORD])
    
    __MNEMONIC= {
        GG.UNK : 'UNK ',
        GG.LD : 'LD  ',
        GG.PUSH : 'PUSH',
        GG.POP : 'POP ',
        GG.EX : 'EX  ',
        GG.EXX : 'EXX ',
        GG.LDI : 'LDI ',
        GG.LDIR : 'LDIR',
        GG.LDD : 'LDD ',
        GG.LDDR : 'LDDR',
        GG.CPI : 'CPI ',
        GG.CPIR : 'CPIR',
        GG.CPD : 'CPD ',
        GG.CPDR : 'CPDR',
        GG.ADD : 'ADD ',
        GG.ADC : 'ADC ',
        GG.SUB : 'SUB ',
        GG.SBC : 'SBC ',
        GG.AND : 'AND ',
        GG.OR : 'OR  ',
        GG.XOR : 'XOR ',
        GG.CP : 'CP  ',
        GG.INC : 'INC ',
        GG.DEC : 'DEC ',
        GG.DAA : 'DAA ',
        GG.CPL : 'CPL ',
        GG.NEG : 'NEG ',
        GG.CCF : 'CCF ',
        GG.SCF : 'SCF ',
        GG.NOP : 'NOP ',
        GG.HALT : 'HALT',
        GG.DI : 'DI  ',
        GG.EI : 'EI  ',
        GG.IM0 : 'IM   0',
        GG.IM1 : 'IM   1',
        GG.IM2 : 'IM   2',
        GG.RLCA : 'RLCA',
        GG.RLA : 'RLA ',
        GG.RRCA : 'RRCA',
        GG.RRA : 'RRA ',
        GG.RLC : 'RLC ',
        GG.RL : 'RL  ',
        GG.RRC : 'RRC ',
        GG.RR : 'RR  ',
        GG.SLA : 'SLA ',
        GG.SRA : 'SRA ',
        GG.SRL : 'SRL ',
        GG.RLD : 'RLD ',
        GG.RRD : 'RRD ',
        GG.BIT : 'BIT ',
        GG.SET : 'SET ',
        GG.RES : 'RES ',
        GG.JP : 'JP  ',
        GG.JR : 'JR  ',
        GG.DJNZ : 'DJNZ',
        GG.CALL : 'CALL',
        GG.RET : 'RET ',
        GG.RETI : 'RETI',
        GG.RETN : 'RETN',
        GG.RST00 : 'RST  00H',
        GG.RST08 : 'RST  08H',
        GG.RST10 : 'RST  10H',
        GG.RST18 : 'RST  18H',
        GG.RST20 : 'RST  20H',
        GG.RST28 : 'RST  28H',
        GG.RST30 : 'RST  30H',
        GG.RST38 : 'RST  38H',
        GG.IN : 'IN  ',
        GG.INI : 'INI ',
        GG.INIR : 'INIR',
        GG.IND : 'IND ',
        GG.INDR : 'INDR',
        GG.OUT : 'OUT ',
        GG.OUTI : 'OUTI',
        GG.OTIR : 'OTIR',
        GG.OUTD : 'OUTD',
        GG.OTDR : 'OTDR' }
    
    def __set_extra ( self, op, extra, i ):
        if op in Inst.__BYTE_OPTS: self.extra[i]= extra[0]
        elif op in Inst.__DESP_OPTS: self.extra[i]= extra[1]
        elif op in Inst.__ADDR_WORD_OPTS: self.extra[i]= extra[2]
        elif op == GG.BRANCH: self.extra[i]= (extra[3][0],extra[3][1])
    
    def __init__ ( self, args ):
        self.id= args[1]
        self.bytes= args[4]
        self.addr= (args[0]-len(self.bytes))&0xFFFF
        self.op1= args[2]
        self.op2= args[3]
        self.extra= [None,None]
        self.__set_extra ( self.op1, args[5], 0 )
        self.__set_extra ( self.op2, args[6], 1 )
        
    def __str_op ( self, op, i ):
        if op == GG.A : return ' A'
        elif op == GG.B : return ' B'
        elif op == GG.C : return ' C'
        elif op == GG.D : return ' D'
        elif op == GG.E : return ' E'
        elif op == GG.H : return ' H'
        elif op == GG.L : return ' L'
        elif op == GG.I : return ' I'
        elif op == GG.R : return ' R'
        elif op == GG.BYTE : return ' %02XH'%self.extra[i]
        elif op == GG.pHL : return ' (HL)'
        elif op == GG.pBC : return ' (BC)'
        elif op == GG.pDE : return ' (DE)'
        elif op == GG.pSP : return ' (SP)'
        elif op == GG.pIX : return ' (IX)'
        elif op == GG.pIY : return ' (IY)'
        elif op == GG.pIXd : return ' (IX%+d)'%self.extra[i]
        elif op == GG.pIYd : return ' (IY%+d)'%self.extra[i]
        elif op == GG.ADDR : return ' (%04XH)'%self.extra[i]
        elif op == GG.BC : return ' BC'
        elif op == GG.DE : return ' DE'
        elif op == GG.HL : return ' HL'
        elif op == GG.SP : return ' SP'
        elif op == GG.IX : return ' IX'
        elif op == GG.IXL : return ' IXL'
        elif op == GG.IXH : return ' IXH'
        elif op == GG.IY : return ' IY'
        elif op == GG.IYL : return ' IYL'
        elif op == GG.IYH : return ' IYH'
        elif op == GG.AF : return ' AF'
        elif op == GG.AF2 : return " AF'"
        elif op == GG.B0 : return ' 0'
        elif op == GG.B1 : return ' 1'
        elif op == GG.B2 : return ' 2'
        elif op == GG.B3 : return ' 3'
        elif op == GG.B4 : return ' 4'
        elif op == GG.B5 : return ' 5'
        elif op == GG.B6 : return ' 6'
        elif op == GG.B7 : return ' 7'
        elif op == GG.WORD : return ' %04XH'%self.extra[i]
        elif op == GG.F_NZ : return ' NZ'
        elif op == GG.F_Z : return ' Z'
        elif op == GG.F_NC : return ' NC'
        elif op == GG.F_C : return ' C'
        elif op == GG.F_PO : return ' PO'
        elif op == GG.F_PE : return ' PE'
        elif op == GG.F_P : return ' P'
        elif op == GG.F_M : return ' M'
        elif op == GG.BRANCH : return ' $%+d (%04XH)'%(self.extra[i][0],
                                                       self.extra[i][1])
        elif op == GG.pB : return ' (B)'
        elif op == GG.pC : return ' (C)'
        elif op == GG.pD : return ' (D)'
        elif op == GG.pE : return ' (E)'
        elif op == GG.pH : return ' (H)'
        elif op == GG.pL : return ' (L)'
        elif op == GG.pA : return ' (A)'
        elif op == GG.pBYTE : return ' (%02XH)'%self.extra[i]
        else: return str(op)
        
    def __str__ ( self ):
        ret= '%04X   '%self.addr
        for b in self.bytes: ret+= ' %02x'%b
        for n in range(len(self.bytes),4): ret+= '   '
        ret+= '    '+Inst.__MNEMONIC[self.id]
        if self.op1 != GG.NONE :
            ret+= self.__str_op ( self.op1, 0 )
        if self.op2 != GG.NONE :
            ret+= ','+self.__str_op ( self.op2, 1 )
        
        return ret

class Record:
    
    def __init__ ( self, inst ):
        self.inst= inst
        self.nexec= 0
        
class Tracer:
    
    def __init__ ( self ):
        self.next_addr= 0
        self.records= [None]*0x10000
        self.print_insts= False
        self.print_ram_access= False
        self.print_mapper_changed= False
        self.max_nexec= 0
        self.last_addr= None
        
    def enable_print_insts ( self, enabled ):
        self.print_insts= enabled

    def enable_print_ram_access(self,enabled):
        self.print_ram_access= enabled
    
    def dump_insts ( self ):
        aux= self.max_nexec
        prec= 0
        while aux != 0:
            prec+= 1
            aux//= 10
        prev= True
        i= 0
        while i < 0x10000:
            rec= self.records[i]
            if rec == None :
                prev= False
                i+= 1
            else:
                if not prev: print ( '\n' )
                print ( ('[%'+str(prec)+'d] %s')%(rec.nexec,rec.inst) )
                prev= True
                i+= len(rec.inst.bytes)
    
    def mem_access ( self, typ, addr, data ):
        if self.print_ram_access :
            if typ == GG.READ :
                print ( 'RAM[%04X] -> %02X'%(addr,data) )
            else:
                print ( 'RAM[%04X]= %02X'%(addr,data) )
    
    def cpu_step ( self, *args ):
        # NOTA!!!!: Al canviar de mapper es caga tot.
        if args[0] == GG.IRQ :
            if self.print_insts : print ( 'IRQ 00%02X'%args[1] )
            return
        elif args[0] == GG.NMI :
            print ( 'NMI' )
            return
        self.next_addr= args[1]
        inst= Inst ( args[1:] )
        self.last_addr= inst.addr
        aux= self.records[inst.addr]
        if aux == None :
            aux= self.records[inst.addr]= Record ( inst )
        aux.nexec+= 1
        if aux.nexec > self.max_nexec:
            self.max_nexec= aux.nexec
        if self.print_insts:
            print ( inst )
        
    def mapper_changed ( self ):
        if self.print_mapper_changed :
            print ( GG.get_mapper_state() )

class Color:
    
    @staticmethod
    def get ( r, g, b ):
        return (r<<16)|(g<<8)|b
    
    @staticmethod
    def get_components ( color ):
        return (color>>16,(color>>8)&0xff,color&0xff)

class Img:
    
    WHITE= Color.get ( 255, 255, 255 )
    
    def __init__ ( self, width, height ):
        self._width= width
        self._height= height
        self._v= []
        for i in range(0,height):
            self._v.append ( array('i',[Img.WHITE]*width) )
    
    def __getitem__ ( self, ind ):
        return self._v[ind]
    
    def write ( self, to ):
        if type(to) == str :
            to= open ( to, 'wt' )
        to.write ( 'P3\n ')
        to.write ( '%d %d\n'%(self._width,self._height) )
        to.write ( '255\n' )
        for r in self._v:
            for c in r:
                aux= Color.get_components ( c )
                to.write ( '%d %d %d\n'%(aux[0],aux[1],aux[2]) )

def cram2palette ( cram ):
    factor= 255.0/15.0
    ret= [ array ( 'i', [0]*16 ), array ( 'i', [0]*16 ) ]
    i= 0
    for j in range(0,2):
        aux= ret[j]
        for k in range(0,16):
            c= cram[i] | (cram[i+1]<<8)
            aux[k]= Color.get ( int((c&0xf)*factor),
                                int(((c>>4)&0xf)*factor),
                                int(((c>>8)&0xf)*factor) )
            i+= 2
    return ret

def palette2img ( pal ):
    ret= Img(128,16)
    for r in range(0,2):
        yoff= r*8
        for c in range(0,16):
            r2= yoff
            xoff= c*8
            color= pal[r][c]
            for j in range(0,8):
                aux= ret[r2]
                r2+= 1
                c2= xoff
                for i in range(0,8):
                    aux[c2]= color
                    c2+= 1
    return ret

PAL16= array ( 'i',
               [ Color.get(0,0,0),
                 Color.get(128,0,0),
                 Color.get(256,0,0),
                 Color.get(0,128,0),
                 Color.get(128,128,0),
                 Color.get(256,128,0),
                 Color.get(0,256,0),
                 Color.get(128,256,0),
                 Color.get(256,256,0),
                 Color.get(0,0,128),
                 Color.get(128,0,128),
                 Color.get(256,0,128),
                 Color.get(0,128,256),
                 Color.get(128,128,256),
                 Color.get(256,128,256),
                 Color.get(0,256,256) ] )

def bitplane2color ( bp0, bp1, bp2, bp3, pal, flip= False ):
    ret= array ( 'i', [0]*8 )
    if flip:
        for i in range(0,8):
            ret[i]= pal[(bp0&1)|((bp1<<1)&2)|((bp2<<2)&4)|((bp3<<3)&8)]
            bp0>>= 1; bp1>>= 1; bp2>>= 1; bp3>>= 1
    else:
        for i in range(0,8):
            ret[i]= pal[((bp0>>7)&1)|((bp1>>6)&2)|((bp2>>5)&4)|((bp3>>4)&8)]
            bp0<<= 1; bp1<<= 1; bp2<<= 1; bp3<<= 1
    return ret

def vram2img ( vram, pal ):
    ram= vram['RAM']
    ret= Img ( 256, 128 )
    i= 0
    yoff= 0
    for r in range(0,16):
        xoff= 0
        for c in range(0,32):
            r2= yoff
            for j in range(0,8):
                line= bitplane2color ( ram[i], ram[i+1],
                                       ram[i+2], ram[i+3],
                                       pal )
                i+= 4
                c2= xoff
                for k in range(0,8):
                    ret[r2][c2]= line[k]
                    c2+= 1
                r2+= 1
            xoff+= 8
        yoff+= 8
    return ret

def nt2img ( vram, pal, nt_addr= None ):
    ram= vram['RAM']
    nt= vram['NT'] if nt_addr == None else nt_addr
    ret= Img ( 256, 224 )
    yoff= 0
    for r in range(0,28):
        xoff= 0
        for c in range(0,32):
            w= ram[nt]|(ram[nt+1]<<8);
            nt+= 2
            pos= (w&0x1FF)<<5
            cpal= pal[(w&0x800)>>11]
            r2= yoff+8-1 if w&0x400 else yoff
            r2_inc= -1 if w&0x400 else 1
            for j in range(0,8):
                line= bitplane2color ( ram[pos], ram[pos+1],
                                       ram[pos+2], ram[pos+3],
                                       cpal, (w&0x200)!=0 )
                pos+= 4
                c2= xoff
                for i in range(0,8):
                    ret[r2][c2]= line[i]
                    c2+= 1
                r2+= r2_inc
            xoff+= 8
        yoff+= 8
    return ret

# Format emprat en M2LDTBL en el Columns.
def bpat2img(addr,bank):
    white= Color.get(255,255,255)
    black= Color.get(0,0,0)
    nbytes= bank[addr]|(bank[addr+1]<<8)
    addr+= 2
    ret= Img(8,nbytes)
    for r in range(0,nbytes):
        b= bank[addr]
        addr+= 1
        for c in range(0,8):
            ret[r][c]= black if b&0x80 else white
            b<<= 1
    return ret

# Decodifica patterns d'acord amb la rutina LDTBL del columns. Torna
# una vram de mentires amb les dades descodificades.
def ldtbl(addr,bank,vram,off=0):
    N= bank[addr]
    addr+= 1
    for n in range(0,N):
        to= off
        while 1:
            aux= bank[addr] 
            addr+= 1
            if aux == 0 : break
            nbytes= aux&0x7f
            for i in range(0,nbytes):
                vram[to]= bank[addr]
                to+= N
                if aux&0x80 : addr+= 1
            if not aux&0x80 : addr+= 1
        off+= 1

# Funció basada en la rutina BOXWT del Columns.
def boxwt(buff,vram,off,width,height):
    k= 0
    for j in range(0,height):
        aux= off
        for i in range(0,width):
            vram[aux]= buff[k]
            vram[aux+1]= buff[k+1]
            aux+= 2; k+= 2
        off+= 64

# Funció que carrega colors en una paleta, està basada en CLRSET0
def clrset(addr,bank,pal):
    factor= 255.0/15
    aux= pal[0]+pal[1]
    i= bank[addr]; addr+= 1
    N= int(bank[addr]/2); addr+= 1
    for n in range(0,N):
        aux[i]= Color.get ( int((bank[addr]&0xf)*factor),
                            int((bank[addr]>>4)*factor),
                            int((bank[addr+1]&0xf)*factor) )
        addr+= 2
        i+= 1
    k= 0
    for i in range(0,1):
        for j in range(0,16):
            pal[i][j]= aux[k]
            k+= 1

def ps_generate_tiles_generate(bank,addr,bp,H,L):
    for i in range(0,8):
        if H&0x80: bp[i]= L
        else:
            bp[i]= bank[addr]
            addr+= 1
        H<<= 1
    return addr

def ps_generate_tiles_generate2(bank,addr,bp,H,L,bp0):
    for i in range(0,8):
        if H&0x80: bp[i]= bp0[i]^L
        else:
            bp[i]= bank[addr]
            addr+= 1
        H<<= 1
    return addr

def ps_generate_tiles_fill_bp(bits,bank,addr,bps,i):
    if bits == 0x00 :
        addr= ps_generate_tiles_generate(bank,addr,bps[i],0xFF,0x00)
    elif bits == 0x40 :
        addr= ps_generate_tiles_generate(bank,addr,bps[i],0xFF,0xFF)
    elif bits == 0xC0 :
        addr= ps_generate_tiles_generate(bank,addr,bps[i],0x00,0x00)
    else: # bits == 0x80
        b= bank[addr]; addr+= 1
        if b < 3 : sys.exit('%x'%b)
        elif b >= 0x10 and b < 0x13:
            addr= ps_generate_tiles_generate2(bank,addr,bps[i],
                                              0xFF,0xFF,bps[b&0x3])
        elif b >= 0x20 and b < 0x23: sys.exit('%x'%b)
        elif b >= 0x40 and b < 0x43:
            b2= bank[addr]; addr+= 1
            addr= ps_generate_tiles_generate2(bank,addr,bps[i],
                                              b2,0xFF,bps[b&0x3])
        else:
            b2= bank[addr]; addr+= 1
            addr= ps_generate_tiles_generate(bank,addr,bps[i],b,b2)
    return addr

# Funció de PhantasyStar que genera tiles. Torna una imatge
def ps_generate_tiles(bank,addr,ntiles,pal=PAL16):
    ret= Img ( 8, 8*ntiles )
    bps= [ array('i',[0]*8), array('i',[0]*8),
           array('i',[0]*8), array('i',[0]*8) ]
    r= 0
    for n in range(0,ntiles):
        b= bank[addr]; addr+= 1
        for i in range(0,4):
            addr= ps_generate_tiles_fill_bp ( b&0xC0, bank, addr, bps, i )
            b<<= 2
        for l in range(0,8):
            line= bitplane2color ( bps[0][l], bps[1][l],
                                   bps[2][l], bps[3][l], pal )
            for c in range(0,8): ret[r][c]= line[c]
            r+= 1
    return ret

GG.init()
#GG.set_rom ( open( '/home/adria/romdb/Sega Game Pack 4 in 1 [GG]/Sega 4 in 1 Pack (UE).gg','rb' ).read() )
#GG.set_rom ( open( '/home/adria/jocs/ROMS/Columns (UE) [!].gg','rb' ).read() )
#GG.set_rom ( open( '/home/adria/jocs/ROMS/Sonic the Hedgehog (JUE).gg','rb' ).read() )
#GG.set_rom ( open( '/home/adria/jocs/ROMS/Megaman (JUE).gg','rb' ).read() )
GG.set_rom ( open('/home/adria/COLJOCS/GG/NBA Jam Tournament Edition/imgs/NBA Jam Tournament Edition (JUE).gg','rb').read() )
rom= GG.get_rom()
#print ( 'Checksum: %d'%rom['checksum'] )
#print ( 'Product code: %d'%rom['code'] )
#print ( 'Version: %d'%rom['version'] )
#print ( 'Region code: '+rom['region'] )
#print ( 'ROM size: '+rom['size'] )

## PhantasyStar Tiles
#ps_generate_tiles(rom['banks'][0xA],0x0000,25).write('ps_tiles.pnm')
#sys.exit(0)

#GG.loop()
#sys.exit(0)
t= Tracer()
t.enable_print_insts ( True )
#t.enable_print_ram_access ( True )
GG.set_tracer ( t )
for i in range(0,1000000): GG.trace()
#GG.trace()
#while t.last_addr != 0x20B3 : GG.trace()
#for i in range(0,10) : GG.trace()
#for i in rom['banks'][0][0x2013:0x2013+140]:print(hex(i))
#for i in rom['banks'][1][0x3BC4:0x3BC4+20]:print(hex(i))
#for i in rom['banks'][2][0x0000:0x0000+10]:print(hex(i))
#for i in rom['banks'][0][0x0028:0x0028+10]:print(hex(i))
#print(GG.get_mapper_state())
#print([hex(x) for x in rom['banks'][15][0x07C3:0x07C3+50]])
#for i in range(0,250000): GG.trace()
#t.dump_insts()
#cram= GG.get_cram()
#vram= GG.get_vram()
#pal= cram2palette(cram)
#palette2img(pal).write('palette.ppm')
#vram2img(vram,PAL16).write('patterns.ppm')
#nt2img(vram,pal).write('image.ppm')
#GG.close()
sys.exit(0)


t= Tracer()
#t.enable_print_insts ( True )
GG.set_tracer ( t )
#while t.last_addr != 0x1015:
#    GG.trace()
#for i in range(0,30): GG.trace()
#for i in range(0,100):
#    while t.last_addr != 0x01F2 :
#        GG.trace()
#    GG.trace()
t.dump_insts()
rom= GG.get_rom()

## MDLOGO
bpat2img(0x3DAB,rom['banks'][0]).write ( 'ascii.ppm' )
vram0= { 'RAM' : [0]*16*1024 }
exbuf= [0]*1024
ldtbl(0x218,rom['banks'][1],vram0['RAM'],0)
ldtbl(0x1EC,rom['banks'][1],exbuf)
boxwt(exbuf,vram0['RAM'],0x3AD8,8,3)
vram2img(vram0,PAL16).write('patterns0.ppm')
mpal= [ array('i',[0]*16), array('i',[0]*16)]
clrset(0x210,rom['banks'][1],mpal)
palette2img(mpal).write ( 'sega_in_pal.ppm' )
pal= [array('i',[0]*16), array('i',[0]*16)]
for n in range(0,16):
    aux= (15-n)*16
    for i in range(0,1):
        for j in range(0,16):
            r,g,b= Color.get_components(mpal[i][j])
            r-= aux
            if r < 0 : r= 0
            g-= aux
            if g < 0 : g= 0
            b-= aux
            if b < 0 : b= 0
            pal[i][j]= Color.get ( r, g, b )
    palette2img(pal).write ( 'sega_in_pal_%02d_.ppm'%n )
    nt2img(vram0,pal,nt_addr=0x3800).write('sega_in_%02d.ppm'%n)

## ADVERTISE
bpat2img(0x3D5F,rom['banks'][0]).write ( 'ya.ppm' )
vram0= { 'RAM' : [0]*16*1024 }
exbuf= [0]*1024
ldtbl(0x394,rom['banks'][1],vram0['RAM'],0)
ldtbl(0x1020,rom['banks'][1],exbuf)
boxwt(exbuf,vram0['RAM'],0x390C,20,8)
vram2img(vram0,PAL16).write('patterns1.ppm')
mpal= [ array('i',[0]*16), array('i',[0]*16)]
clrset(0x3D69,rom['banks'][0],mpal)
palette2img(mpal).write ( 'title_pal.ppm' )
nt2img(vram0,[PAL16],nt_addr=0x3800).write('title.ppm')

#print([hex(x) for x in rom['banks'][1][0x282E:0x282E+50]])
#print([hex(x) for x in rom['banks'][0][0x0295:0x0295+10]])
#print([hex(x) for x in rom['banks'][0][0x327:0x327+5]])
#aux= [hex(x) for x in rom['banks'][1][0x2A14:0x2A14+256]]
#print ( aux )
#print ( aux[0x20] )
GG.close()
