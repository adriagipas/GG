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
 * along with adriagipas/GG.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  control.c - Implementació del mòdul CONTROL.
 *
 *  NOTA: NO ESTIC IMPLEMENTANT EL CONTROL EXT.
 *
 */


#include "GG.h"




/*********/
/* ESTAT */
/*********/

/* Callback. */
static void *_udata;
static GG_CheckButtons *_check_buttons;




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
GG_control_init (
        	 GG_CheckButtons *check_buttons,
        	 void            *udata
        	 )
{
  
  _udata= udata;
  _check_buttons= check_buttons;
  
} /* end GG_control_init */


Z80u8
GG_control_get_status1 (void)
{
  return (Z80u8) ~(_check_buttons ( _udata )&0x3F);
} /* GG_control_get_status1 */


Z80u8
GG_control_get_status_ext (void)
{
  return 0xFF;
} /* end GG_control_get_status_ext */


Z80u8
GG_control_get_status_start (void)
{
  return (_check_buttons ( _udata )&GG_START) ? 0x00 : 0x80;
} /* end GG_control_get_status_start */
