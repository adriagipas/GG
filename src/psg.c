/*
 * Copyright 2011,2015,2022 Adrià Giménez Pastor.
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
 *  psg.c - Implementació del xip de sò PSG.
 *
 *  NOTA: EN CONTROL HE COMENTAT UN TROÇ DE CODI TEORICAMENT BO!!! AÇÒ
 *  FA QUE DESAPAREGA UN SOROLL EN EL JOC DE TENIS. PERÒ CREC QUE HI
 *  HA UNA ERRADA EN ALGUN ALTRE LLOC.
 *
 */


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
/* TIPUS */
/*********/

typedef struct
{
  
  Z80u16   reg;           /* Valor que es carrega en el comptador. 10 bits. */
  Z80u16   counter;       /* Comptador. 10 bits. */
  Z80u8    out;           /* Eixida del comptador. 1 bit. */
  Z80u8    vol;           /* Volumen. 4 bits. */
  
} tone_channel_t;




/*************/
/* CONSTANTS */
/*************/

static const double _volume_table[16]=
  {
    0.25, 0.198582058681, 0.15773933612, 0.125296808407,
    0.0995267926384, 0.0790569415042, 0.0627971607877, 0.0498815578742,
    0.0396223298115, 0.0314731352949, 0.025, 0.0198582058681,
    0.015773933612, 0.0125296808407, 0.00995267926384, 0.0
  };




/*********/
/* ESTAT */
/*********/

/* Indica el canal i component a actualitzar. */
static int _latch_channel;
static enum { VOL, DATA } _latch_type;

/* Estat dels canals de to. */
static tone_channel_t _tone_channels[3];

/* Estat del canal de soroll. */
static struct
{
  
  Z80u8    sel_len;    /* Selecciona el valor amb el que updatejar el
        		  comptador. */
  Z80_Bool white;      /* White noise/Periodic noise. */
  Z80u16   counter;    /* Comptador. */
  Z80u16   shift;      /* Shift register. */
  Z80u8    vol;        /* Volumen. 4 bits. */
  Z80u8    out;        /* Eixida del comptador. 1 bit. */
  Z80u8    reg;        /* Últim valor que es va carregar al
        		  comptador. Açò és per a poder ser coherent
        		  amb els tone. */
  
} _noise_channel;

/* Buffers per a cada canal. Açò es abans de convertir al valor
   real. */
static Z80u8 _buffer[4][GG_PSG_BUFFER_SIZE];

/* Comptadors i cicles per processar. */
static struct
{
  
  int pos;          /* Següent sample a generar, on 0 és el primer. */
  int cc;           /* Cicles de UCP acumulats. */
  int cctoFrame;    /* Cicles que falten per a plenar el buffer. */
  
} _timing;

/* Buffers d'eixida. */
static double _left[GG_PSG_BUFFER_SIZE];
static double _right[GG_PSG_BUFFER_SIZE];

/* Mascares dels canals. */
static int _left_mask;
static int _right_mask;

/* Callback. */
static GG_PlaySound *_play_sound;
static void *_udata;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

/* Torna el nou estat del canal. */
static tone_channel_t
render_tone_channel (
        	     tone_channel_t  channel,
        	     Z80u8           buffer[GG_PSG_BUFFER_SIZE],
        	     const int       begin,
        	     const int       end
        	     )
{
  
  int i;
  Z80u8 vol;
  
  
  if ( channel.reg <= 1 ) vol= channel.vol;
  else vol= channel.out ? channel.vol : 0xF;
  for ( i= begin; i < end; ++i )
    {
      if ( channel.counter == 0 || --channel.counter == 0 )
        {
          if ( channel.reg <= 1 ) vol= channel.vol;
          else vol= (channel.out^= 0x1) ? channel.vol : 0xF;
          channel.counter= channel.reg;
        }
      buffer[i]= vol;
    }
  
  return channel;
  
} /* end render_tone_channel */


static void
render_noise_channel (
        	      Z80u8     buffer[GG_PSG_BUFFER_SIZE],
        	      const int begin,
        	      const int end
        	      )
{
  
  /* NOTA: L'eixida del comptador ho faig exactament com en el
     tone. No se si en el tone està bé, però soc coherent. */
  /* Realment el volumen no seria l'últim bit, sinó el bit que hem
     llevat, però tenint en compter que és soroll no crec que importe
     molt. */

  Z80u8 vol;
  int i;
  Z80_Bool clk;
  
  
  vol= (_noise_channel.shift&0x80) ? _noise_channel.vol : 0xF;
  for ( i= begin; i < end; ++i )
    {
      if ( _noise_channel.counter == 0 || --_noise_channel.counter == 0 )
        {
          if ( _noise_channel.reg <= 1 ) clk= (_noise_channel.out == 0);
          else                           clk= ((_noise_channel.out^= 1)==1);
          if ( clk )
            {
              _noise_channel.shift=
        	(_noise_channel.shift<<1) |
        	(_noise_channel.white ?
        	 (((_noise_channel.shift>>15)^(_noise_channel.shift>>12))&0x1) :
        	 (_noise_channel.shift>>15));
              vol= (_noise_channel.shift&0x80) ? _noise_channel.vol : 0xF;
            }
          switch ( _noise_channel.sel_len )
            {
            case 0: _noise_channel.reg= 0x10; break;
            case 1: _noise_channel.reg= 0x20; break;
            case 2: _noise_channel.reg= 0x40; break;
            case 3: _noise_channel.reg= _tone_channels[2].reg; break;
            default: break;
            }
          _noise_channel.counter= _noise_channel.reg;
        }
      buffer[i]= vol;
    }
  
} /* end render_noise_channel */


static void
join_channels (
               int     mask,
               double *channel
               )
{
  
  int sel, i, j;
  const Z80u8 *buffer;
  
  
  for ( i= 0; i < GG_PSG_BUFFER_SIZE; ++i ) channel[i]= 0.0;
  for ( sel= 0x1, j= 0; j < 4; sel<<= 1, ++j )
    if ( mask&sel )
      {
        buffer= _buffer[j];
        for ( i= 0; i < GG_PSG_BUFFER_SIZE; ++i )
          channel[i]+= _volume_table[buffer[i]];
      }
  
} /* end join_channels */


static void
run (
     const int begin,
     const int end
     )
{
  
  int i;
  
  
  for ( i= 0; i < 3; ++i )
    _tone_channels[i]=
      render_tone_channel ( _tone_channels[i], _buffer[i], begin, end );
  render_noise_channel ( _buffer[3], begin, end );
  
  if ( end == GG_PSG_BUFFER_SIZE )
    {
      join_channels ( _left_mask, _left );
      join_channels ( _right_mask, _right );
      _play_sound ( _left, _right, _udata );
    }
  
} /* end run */


static void
clock (void)
{
  
  int npos;
  
  
  /*
   * NOTA: 16 cicles de UCP és una mostra.
   */
  _timing.cctoFrame-= _timing.cc;
  npos= _timing.pos + _timing.cc/16;
  _timing.cc%= 16;
  _timing.cctoFrame+= _timing.cc;
  while ( npos >= GG_PSG_BUFFER_SIZE )
    {
      run ( _timing.pos, GG_PSG_BUFFER_SIZE );
      npos-= GG_PSG_BUFFER_SIZE;
      _timing.pos= 0;
    }
  run ( _timing.pos, npos );
  _timing.pos= npos;
  if ( _timing.cctoFrame <= 0 )
    _timing.cctoFrame= (GG_PSG_BUFFER_SIZE-_timing.pos)*16;
  
} /* end clock */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
GG_psg_clock (
              const int cc
              )
{
  
  if ( (_timing.cc+= cc) >= _timing.cctoFrame )
    clock ();
  
} /* end GG_psg_clock */


void
GG_psg_control (
        	const Z80u8 data
        	)
{
  
  clock ();
  
  /* LATCH/DATA byte. */
  if ( data&0x80 )
    {
      _latch_channel= (data>>5)&0x3;
      _latch_type= (data&0x10)?VOL:DATA;
      if ( _latch_type == DATA )
        {
          if ( _latch_channel != 3 )
            {
              _tone_channels[_latch_channel].reg&= 0xFFF0;
              _tone_channels[_latch_channel].reg|= data&0xF;
            }
          else
            {
              _noise_channel.sel_len= data&0x3;
              _noise_channel.white= ((data&0x4)!=0);
              if ( !_noise_channel.white ) _noise_channel.shift= 0x80;
            }
        }
      else
        {
          if ( _latch_channel != 3 )
            _tone_channels[_latch_channel].vol= data&0xF;
          else _noise_channel.vol= data&0xF;
        }
    }
  
  /* DATA byte. */
  else
    {
      if ( _latch_type == DATA )
        {
          if ( _latch_channel != 3 )
            {
              _tone_channels[_latch_channel].reg&= 0x000F;
              _tone_channels[_latch_channel].reg|= ((Z80u16) (data&0x3F))<<4;
            }
          else
            {
              _noise_channel.sel_len= data&0x3;
              _noise_channel.white= ((data&0x4)!=0);
              if ( !_noise_channel.white ) _noise_channel.shift= 0x80;
            }
        }
      else
        {
          /* XAPUÇA!!!!!!! SEGONS LA DOCUMENTACIÓ AÇÒ ESTAVA BÉ. */
          /*
          if ( _latch_channel != 3 )
            _tone_channels[_latch_channel].vol= data&0xF;
          else _noise_channel.vol= data&0xF;
          */
        }
    }
  
} /* end GG_psg_control */


void
GG_psg_init (
             GG_PlaySound *play_sound,
             void         *udata
             )
{

  GG_psg_init_state ();
  _play_sound= play_sound;
  _udata= udata;
  
} /* end GG_psg_init */


void
GG_psg_init_state (void)
{
  
  int i;
  
  
  _latch_channel= 0;
  _latch_type= DATA;
  
  /* Canals de to. */
  for ( i= 0; i < 3; ++i )
    {
      _tone_channels[i].reg= 0x00;
      _tone_channels[i].counter= 0x00;
      _tone_channels[i].out= 0;
      _tone_channels[i].vol= 0xF;
    }
  
  /* Canal de soroll. */
  _noise_channel.sel_len= 0;
  _noise_channel.white= Z80_FALSE;
  _noise_channel.reg= _noise_channel.counter= 0x10;
  _noise_channel.shift= 0x80;
  _noise_channel.vol= 0xF;
  _noise_channel.out= 0;
  
  /* Buffers. */
  for ( i= 0; i < 4; ++i )
    memset ( _buffer[i], 0xf, GG_PSG_BUFFER_SIZE );
  
  /* Timing. */
  _timing.pos= 0;
  _timing.cc= 0;
  _timing.cctoFrame= GG_PSG_BUFFER_SIZE*16;
  
  /* Buffers d'eixida. */
  memset ( _left, 0, sizeof(_left) );
  memset ( _right, 0, sizeof(_right) );
  
  /* Màscares. */
  _left_mask= 0xf;
  _right_mask= 0xf;
  
} /* end GG_psg_init_state */


void
GG_psg_stereo (
               Z80u8 data
               )
{
  
  clock ();
  _right_mask= data&0xf;
  _left_mask= data>>4;
  
} /* end GG_psg_stereo */


int
GG_psg_save_state (
        	   FILE *f
        	   )
{

  SAVE ( _latch_channel );
  SAVE ( _latch_type );
  SAVE ( _tone_channels );
  SAVE ( _noise_channel );
  SAVE ( _buffer );
  SAVE ( _timing );
  SAVE ( _left );
  SAVE ( _right );
  SAVE ( _left_mask );
  SAVE ( _right_mask );
  
  return 0;
  
} /* end GG_psg_save_state */


int
GG_psg_load_state (
        	   FILE *f
        	   )
{

  int i;
  const Z80u8 *p;
  
  
  LOAD ( _latch_channel );
  CHECK ( _latch_channel >= 0 && _latch_channel <= 3 );
  LOAD ( _latch_type );
  LOAD ( _tone_channels );
  for ( i= 0; i < 3; ++i )
    {
      CHECK ( (_tone_channels[i].vol&0xF) == _tone_channels[i].vol );
    }
  LOAD ( _noise_channel );
  CHECK ( (_noise_channel.vol&0xF) == _noise_channel.vol );
  LOAD ( _buffer );
  for ( p= &(_buffer[0][0]), i= 0; i < 4*GG_PSG_BUFFER_SIZE; ++i, ++p )
    if ( (*p&0xF) != *p )
      return -1;
  LOAD ( _timing );
  CHECK ( _timing.pos >= 0 && _timing.pos < GG_PSG_BUFFER_SIZE );
  CHECK ( _timing.cc >= 0 );
  LOAD ( _left );
  for ( i= 0; i < GG_PSG_BUFFER_SIZE; ++i )
    if ( _left[i] < 0.0 || _left[i] > 1.0 )
      return -1;
  LOAD ( _right );
  for ( i= 0; i < GG_PSG_BUFFER_SIZE; ++i )
    if ( _right[i] < 0.0 || _right[i] > 1.0 )
      return -1;
  LOAD ( _left_mask );
  LOAD ( _right_mask );
  
  return 0;
  
} /* end GG_psg_load_state */
