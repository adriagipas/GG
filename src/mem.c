/*
 * Copyright 2010-2012,2015,2022 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/GG.
 *
 * adriagipas/GG is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/GG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/GG.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  mem.c - Implementació del mòdul MEM.
 *
 */


#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "GG.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;




/*********/
/* ESTAT */
/*********/

/* Funcions de lectura/escriptura. */
static Z80u8 (*_read) ( Z80u16 addr );
static void (*_write) ( Z80u16 addr, Z80u8 data );

/* Memòria. */
static Z80u8 _ram[8192 /*8K*/];

/* Memòria externa. */
static GG_GetExternalRAM *_get_external_ram;
static struct sram
{
  
  Z80u8    *mem;
  Z80_Bool  onboard;
  Z80_Bool  onslot2;
  Z80u8    *slot2;
  
} _sram;

/* ROM write enabled. */
/* Aparement els catutxos amb jocs no funcionen amb aquest bit, o
cadascun fa una cosa. Per tant millor no fer res. */
/*static Z80_Bool _rom_write_enabled;*/

/* Rom. */
static GG_Rom _rom;

/* Pàgines ROM. */
static int _p0;
static int _p1;
static int _p2;

/* Desplaçament. */
static int _shift;

/* Funció d'avisos. */
static void *_udata;

/* Traça. */
static GG_MemAccess *_mem_access;
static GG_MapperChanged *_mapper_changed;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static Z80u8
read_notrace (
              Z80u16 addr
              )
{
  
  int area;
  Z80u16 aux;
  
  
  area= (addr&0xC000)>>14;
  switch ( area )
    {
    case 0:
      aux= addr&0x3FFF;
      if ( aux < 0x400 ) return _rom.banks[0][aux];
      else               return _rom.banks[_p0][aux];
    case 1: return _rom.banks[_p1][addr&0x3FFF];
    case 2:
      if ( _sram.onslot2 ) return _sram.slot2[addr&0x3FFF];
      else                 return _rom.banks[_p2][addr&0x3FFF];
    case 3:
      if ( _sram.onboard ) return _sram.mem[addr&0x3FFF];
      else                 return _ram[addr&0x1FFF];
    }
  
  return 0x00; /* No es queixe. */
  
} /* end read_notrace */


static Z80u8
read_trace (
            Z80u16 addr
            )
{
  
  if ( _mem_access != NULL && addr >= 0xC000 && !_sram.onboard )
    _mem_access ( GG_READ, addr&0x1FFF, _ram[addr&0x1FFF], _udata );
  return read_notrace ( addr );
  
} /* end read_trace */


static void
write_notrace (
               Z80u16 addr,
               Z80u8  data
               )
{
  
  if ( addr < 0xC000 )
    {
      if ( /*_rom_write_enabled &&*/ _sram.onslot2 && addr >= 0x8000 )
        _sram.slot2[addr&0x3FFF]= data;
      return;
    }
  if ( _sram.onboard ) _sram.mem[addr&0x3FFF]= data;
  else                 _ram[addr&0x1FFF]= data;
  if ( addr < 0xFFFC ) return;
  if ( addr == 0xFFFC )
    {
      /*_rom_write_enabled= ((data&0x80)!=0);*/
      if ( data&0x10 )
        {
          if ( _sram.mem == NULL ) _sram.mem= _get_external_ram ( _udata );
          _sram.onboard= Z80_TRUE;
        }
      else _sram.onboard= Z80_FALSE;
      if ( data&0x08 )
        {
          if ( _sram.mem == NULL ) _sram.mem= _get_external_ram ( _udata );
          _sram.slot2= _sram.mem+((data&0x04)?0x4000:0);
          _sram.onslot2= Z80_TRUE;
        }
      else _sram.onslot2= Z80_FALSE;
      switch ( data&0x3 )
        {
        case 0: _shift= 0x00; break;
        case 1: _shift= 0x18; break;
        case 2: _shift= 0x10; break;
        case 3: _shift= 0x08; break;
        }
    }
  else if ( addr == 0xFFFD ) _p0= (data+_shift)%_rom.nbanks;
  else if ( addr == 0xFFFE ) _p1= (data+_shift)%_rom.nbanks;
  else if ( addr == 0xFFFF ) _p2= (data+_shift)%_rom.nbanks;
  
} /* end _write */


static void
write_trace (
             Z80u16 addr,
             Z80u8  data
             )
{
  
  if ( addr >= 0xC000 )
    {
      if ( _mem_access != NULL && !_sram.onboard )
        _mem_access ( GG_WRITE, addr&0x1FFF, data, _udata );
      if ( _mapper_changed && addr >= 0xFFFC )
        _mapper_changed ( _udata );
    }
  write_notrace ( addr, data );
  
} /* end write_trace */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
GG_mem_get_mapper_state (
        		 GG_MapperState *state
        		 )
{
  
  state->p0= _p0;
  state->p1= _p1;
  state->p2= _p2;
  state->shift= _shift;
  
} /* end GG_mem_get_mapper_state */


void
GG_mem_init (
             const GG_Rom      *rom,
             GG_GetExternalRAM *get_external_ram,
             GG_MemAccess      *mem_acces,
             GG_MapperChanged  *mapper_changed,
             void              *udata
             )
{
  
  assert ( rom->nbanks > 0 );
  
  _rom= *rom;
  GG_mem_init_state ();
  _get_external_ram= get_external_ram;
  _udata= udata;
  _mem_access= mem_acces;
  _mapper_changed= mapper_changed;
  GG_mem_set_mode_trace ( Z80_FALSE );
  
} /* end GG_mem_init */


void
GG_mem_init_state (void)
{
  
  memset ( _ram, 0, 8192 );
  if ( _rom.nbanks == 1 )
    _p0= _p1= _p2= 0;
  else if ( _rom.nbanks == 2 )
    {
      _p0= 0;
      _p1= _p2= 1;
    }
  else
    {
      _p0= 0;
      _p1= 1;
      _p2= 2;
    }
  _shift= 0;
  _sram.mem= NULL;
  _sram.slot2= NULL;
  _sram.onboard= _sram.onslot2= Z80_FALSE;
  /*_rom_write_enabled= Z80_TRUE;*/
  
} /* end GG_mem_init_state */


void
GG_mem_set_mode_trace (
        	       const Z80_Bool val
        	       )
{
  
  if ( val )
    {
      _read= read_trace;
      _write= write_trace;
    }
  else
    {
      _read= read_notrace;
      _write= write_notrace;
    }
  
} /* end GG_mem_set_mode_trace */


Z80u8
Z80_read (
          Z80u16 addr
          )
{
  return _read ( addr );
} /* end Z80_read */


void
Z80_write (
           Z80u16 addr,
           Z80u8  data
           )
{
  _write ( addr, data );
} /* end Z80_write */


int
GG_mem_save_state (
        	   FILE *f
        	   )
{

  SAVE ( _ram );
  SAVE ( _sram );
  if ( _sram.mem != NULL )
    {
      if ( fwrite ( _sram.mem, 32*1024, 1, f ) != 1 )
        return -1;
    }
  SAVE ( _rom.nbanks );
  SAVE ( _p0 );
  SAVE ( _p1 );
  SAVE ( _p2 );
  SAVE ( _shift );
  
  return 0;
  
} /* end GG_mem_save_state */


int
GG_mem_load_state (
        	   FILE *f
        	   )
{

  Z80u8 *tmp;
  GG_Rom rom_fk;
  
  
  LOAD ( _ram );
  LOAD ( _sram );
  CHECK ( !_sram.onboard || _sram.mem!=NULL );
  CHECK ( !_sram.onslot2 || _sram.slot2!=NULL );
  CHECK ( _sram.slot2==NULL || _sram.mem!=NULL );
  if ( _sram.mem != NULL )
    {
      tmp= _sram.mem;
      _sram.mem= _get_external_ram ( _udata );
      if ( _sram.slot2 != NULL )
        {
          _sram.slot2= _sram.mem + (_sram.slot2-tmp);
          CHECK ( _sram.slot2==_sram.mem || _sram.slot2==(_sram.mem+0x4000) );
        }
      if ( fread ( _sram.mem, 32*1024, 1, f ) != 1 )
        return -1;
    }
  LOAD ( rom_fk.nbanks );
  CHECK ( rom_fk.nbanks == _rom.nbanks );
  LOAD ( _p0 );
  CHECK ( _p0 >= 0 && _p0 < _rom.nbanks );
  LOAD ( _p1 );
  CHECK ( _p1 >= 0 && _p1 < _rom.nbanks );
  LOAD ( _p2 );
  CHECK ( _p2 >= 0 && _p2 < _rom.nbanks );
  LOAD ( _shift );
  CHECK ( _shift==0x00 || _shift==0x18 || _shift==0x10 || _shift==0x08 );
  
  return 0;
  
} /* end GG_mem_load_state */
