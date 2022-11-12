/*
 * Copyright 2011,2012,2015,2022 Adrià Giménez Pastor.
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
 *  main.c - Implementació de MAIN.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "GG.h"




/*************/
/* CONSTANTS */
/*************/

/* Un valor un poc arreu. Si cada T és correspon amb un cicle de
   rellotge que va a 3.3MHz approx, tenim que és comprova cada 1/100
   segons. */
static const int CCTOCHECK= 33000;

static const char GGSTATE[]= "GGSTATE\n";




/*********/
/* ESTAT */
/*********/

/* Per a saber si hi ha que parar. */
static Z80_Bool _stop;


/* Frontend. */
static GG_CheckSignals *_check;
static GG_Warning *_warning;
static void *_udata;


/* Callback per a la UCP. */
static GG_CPUStep *_cpu_step;




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
GG_init (
         const GG_Rom      *rom,
         const GG_Frontend *frontend,
         void              *udata
         )
{
  
  _check= frontend->check;
  _warning= frontend->warning;
  _udata= udata;
  _cpu_step= frontend->trace!=NULL ?
    frontend->trace->cpu_step:NULL;
  
  Z80_init ( frontend->warning, udata );
  GG_mem_init ( rom,
        	frontend->get_external_ram,
        	frontend->trace!=NULL ?
        	frontend->trace->mem_access:NULL,
        	frontend->trace!=NULL ?
        	frontend->trace->mapper_changed:NULL,
        	udata );
  GG_vdp_init ( frontend->update_screen, udata );
  GG_control_init ( frontend->check_buttons, udata );
  GG_psg_init ( frontend->play_sound, udata );
  
} /* end GG_init */


int
GG_iter (
         Z80_Bool *stop
         )
{

  static int CC= 0;
  int cc;
  
  
  cc= Z80_run ();
  GG_vdp_clock ( cc );
  GG_psg_clock ( cc );
  CC+= cc;
  if ( CC >= CCTOCHECK && _check != NULL )
    {
      CC-= CCTOCHECK;
      _check ( stop, _udata );
    }
  
  return cc;
  
} /* end GG_iter */


void
GG_loop (void)
{
  
  int cc, CC;
  
  
  _stop= Z80_FALSE;
  if ( _check == NULL )
    {
      while ( !_stop )
        {
          cc= Z80_run ();
          GG_vdp_clock ( cc );
          GG_psg_clock ( cc );
        }
    }
  else
    {
      CC= 0;
      for (;;)
        {
          cc= Z80_run ();
          GG_vdp_clock ( cc );
          GG_psg_clock ( cc );
          CC+= cc;
          if ( CC >= CCTOCHECK )
            {
              CC-= CCTOCHECK;
              _check ( &_stop, _udata );
              if ( _stop ) break;
            }
        }
    }
  _stop= Z80_FALSE;
  
} /* end GG_loop */


void
GG_stop (void)
{
  _stop= Z80_TRUE;
} /* end GG_stop */


int
GG_trace (void)
{
  
  int cc;
  Z80u16 addr;
  Z80_Step step;
  
  
  if ( _cpu_step != NULL )
    {
      addr= Z80_decode_next_step ( &step );
      _cpu_step ( &step, addr, _udata );
    }
  GG_mem_set_mode_trace ( Z80_TRUE );
  cc= Z80_run ();
  GG_vdp_clock ( cc );
  GG_psg_clock ( cc );
  GG_mem_set_mode_trace ( Z80_FALSE );
  
  return cc;
  
} /* end GG_trace */


int
GG_load_state (
               FILE *f
               )
{

  static char buf[sizeof(GGSTATE)];
  
  
  _stop= Z80_FALSE;
  
  /* GGSTATE. */
  if ( fread ( buf, sizeof(GGSTATE)-1, 1, f ) != 1 ) goto error;
  buf[sizeof(GGSTATE)-1]= '\0';
  if ( strcmp ( buf, GGSTATE ) ) goto error;
  
  /* Carrega. */
  if ( Z80_load_state ( f ) != 0 ) goto error;
  if ( GG_mem_load_state ( f ) != 0 ) goto error;
  if ( GG_vdp_load_state ( f ) != 0 ) goto error;
  if ( GG_psg_load_state ( f ) != 0 ) goto error;
  
  return 0;
  
 error:
  _warning ( _udata,
             "error al carregar l'estat del simulador des d'un fitxer" );
  Z80_init_state ();
  GG_mem_init_state ();
  GG_vdp_init_state ();
  GG_psg_init_state ();
  return -1;
  
} /* end GG_load_state */


int
GG_save_state (
               FILE *f
               )
{
  
  if ( fwrite ( GGSTATE, sizeof(GGSTATE)-1, 1, f ) != 1 ) return -1;
  if ( Z80_save_state ( f ) != 0 ) return -1;
  if ( GG_mem_save_state ( f ) != 0 ) return -1;
  if ( GG_vdp_save_state ( f ) != 0 ) return -1;
  if ( GG_psg_save_state ( f ) != 0 ) return -1;
  
  return 0;
  
} /* end GG_save_state */
