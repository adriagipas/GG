/*
 * Copyright 2011,2022 Adrià Giménez Pastor.
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
 * along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  rom.c - Implementa funcions relacionades amb la ROM.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "GG.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
get_region (
            const unsigned char code
            )
{
  
  switch ( code )
    {
    case 3: return GG_ROM_SMS_JAPAN;
    case 4: return GG_ROM_SMS_EXPORT;
    case 5: return GG_ROM_GG_JAPAN;
    case 6: return GG_ROM_GG_EXPORT;
    case 7: return GG_ROM_GG_INTERNATIONAL;
    default: return GG_ROM_UNK;
    }
  
} /* end get_region */


static int
get_rom_size (
              const unsigned char code
              )
{
  
  switch ( code )
    {
    case 0x0: return 256;
    case 0x1: return 512;
    case 0x2: return 1024;
    case 0xa: return 8;
    case 0xb: return 16;
    case 0xc: return 32;
    case 0xd: return 48;
    case 0xe: return 64;
    case 0xf: return 128;
    default: return -1;
    }
  
} /* end get_rom_size */


/* Torna NULL si no hi ha. */
static const Z80u8 *
search_rom_header (
        	   const GG_Rom *rom
        	   )
{
  
  const char *p;
  
  
  if ( rom->banks == 0 ) return NULL;
  p= (const char *) rom->banks;
  if ( !strncmp ( "TMR SEGA", p+0x1ff0, 8 ) )
    return (const Z80u8 *) p+0x1ff0;
  if ( !strncmp ( "TMR SEGA", p+0x3ff0, 8 ) )
    return (const Z80u8 *) p+0x3ff0;
  if ( rom->nbanks > 1 && !strncmp ( "TMR SEGA", p+0x7ff0, 8 ) )
    return (const Z80u8 *) p+0x7ff0;
  
  return NULL;
  
} /* end search_rom_header */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

int
GG_rom_get_header (
        	   const GG_Rom *rom,
        	   GG_RomHeader *header
        	   )
{
  
  const Z80u8 *mem;
  
  
  mem= search_rom_header ( rom );
  if ( mem == NULL ) return -1;
  
  /* Checksum. */
  header->checksum= mem[0xa] | (((Z80u16) mem[0xb])<<8);
      
  /* Product Code. */
  header->code=
    (mem[0xc]&0xf) +
    (mem[0xc]>>4)*10 +
    (mem[0xd]&0xf)*100 +
    (mem[0xd]>>4)*1000 +
    (mem[0xe]>>4)*10000;
  
  /* Version. */
  header->version= (char) (mem[0xe]&0xf);
  
  /* Region Code. */
  header->region= get_region ( mem[0xf]>>4 );
  
  /* Rom Size. */
  header->size= get_rom_size ( mem[0xf]&0xf );
  
  return 0;
  
} /* end GG_rom_get_header */
