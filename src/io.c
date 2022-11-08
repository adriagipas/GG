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
 *  io.c - Implementació de 'Z80_io_read' i 'Z80_io_write'.
 *
 *  NOTES: ELS 7 PRIMERS PORTS NO ELS VAIG A SIMULAR, SIMPLEMENT
 *  TORNARÉ EL VALOR PER DEFECTE DE CADA PORT.
 *
 */


#include <stddef.h>
#include <stdlib.h>

#include "GG.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/
#include<stdio.h>
static void
memory_control (
        	const Z80u8 data
        	)
{
  
  /* NOTA: En la GameGear sols tenen efecte els bits 4 i 3. */
  if ( !(data&0x10) )
    printf ( "No s'ha implementat suport per a la 'Work RAM'\n" );
  if ( !(data&0x08) )
    printf ( "No s'ha implementat suport per a la BIOS\n" );
  
} /* end memory_control */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

Z80u8
Z80_io_read (
             Z80u8 port
             )
{

  if ( port < 0x07 )
    {
      switch ( port )
        {
        case 0: return GG_control_get_status_start()|0x60; /* Overseas i PAL. */
        case 1: return 0x7F;
        case 2: return 0xFF;
        case 3: return 0x00;
        case 4: return 0xFF;
        case 5: return 0x00;
        case 6: return 0xFF;
        }
      return 0x00; /* Per a què no es queixe. */
    }
  else if ( port < 0x40 ) return 0xFF;
  else if ( port < 0x80 )
    {
      if ( port&0x1 ) return GG_vdp_get_H ();
      else            return GG_vdp_get_V ();
    }
  else if ( port < 0xC0 )
    {
      if ( port&0x1 ) return GG_vdp_get_status ();
      else            return GG_vdp_read_data ();
    }
  else if ( (port&0xFE) == 0xC0 || (port&0xFE) == 0xDC )
    {
      if ( port&0x1 ) return GG_control_get_status_ext ();
      else            return GG_control_get_status1 ();
    }
  else return 0xFF;
  
} /* end Z80_io_read */


void
Z80_io_write (
              Z80u8 port,
              Z80u8 data
              )
{
  
  if ( port < 0x06 ) return;
  else if ( port == 0x06 ) GG_psg_stereo ( data );
  else if ( port < 0x40 )
    {
      /* No se molt bé que fa açò. */
      if ( port&0x1 ) return;/*printf ( "I/O control register (W)\n" );*/
      else            memory_control ( data );
    }
  else if ( port < 0x80 ) GG_psg_control ( data );
  else if ( port < 0xC0 )
    {
      if ( port&0x1 ) GG_vdp_control ( data );
      else            GG_vdp_write_data ( data );
    }
  
} /* end Z80_io_write */
