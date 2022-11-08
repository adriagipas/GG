/*
 * Copyright 2010-2012,2022 Adrià Giménez Pastor.
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
 *  ggmodule.c - Mòdul que implementa una GameGear en Python.
 *
 */


#include <Python.h>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "GG.h"




/**********/
/* MACROS */
/**********/

#define WIDTH 160
#define HEIGHT 144


#define CHECK_INITIALIZED        					\
  do {        								\
    if ( !_initialized )        					\
      {        								\
        PyErr_SetString ( GGError, "Module must be initialized" );	\
        return NULL;							\
      }        								\
  } while(0)


#define CHECK_ROM        						\
  do {        								\
    if ( _rom.banks ==NULL )        					\
      {        								\
        PyErr_SetString ( GGError, "There is no ROM inserted"		\
        		  " into the simulator" );			\
        return NULL;							\
      }        								\
  } while(0)




/*********/
/* TIPUS */
/*********/

enum { FALSE= 0, TRUE };




/*********/
/* ESTAT */
/*********/

/* Error. */
static PyObject *GGError;

/* Inicialitzat. */
static char _initialized;

/* Rom. */
static GG_Rom _rom;

/* Tracer. */
static struct
{
  PyObject *obj;
  int       has_cpu_step;
  int       has_mapper_changed;
  int       has_mem_access;
} _tracer;

/* Pantalla. */
static struct
{
  
  int          width;
  int          height;
  SDL_Surface *surface;
  GLuint       textureid;
  uint32_t     data[WIDTH*HEIGHT];
  
} _screen;


/* Paleta de colors. */
static uint32_t _palette[4096];


/* Control. */
static int _control;


/* Estat so. */
static struct
{
  
  double *buffer;
  char    silence;
  int     pos;
  int     size;
  double  ratio;
  double  pos2;
  
} _audio;
static volatile int _audio_full;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
has_method (
            PyObject   *obj,
            const char *name
            )
{
  
  PyObject *aux;
  int ret;
  
  if ( !PyObject_HasAttrString ( obj, name ) ) return 0;
  aux= PyObject_GetAttrString ( obj, name );
  ret= PyMethod_Check ( aux );
  Py_DECREF ( aux );
  
  return ret;
  
} /* end has_method */


static const char *
get_region (
            const int region
            )
{
  
  switch ( region )
    {
    case GG_ROM_SMS_JAPAN: return "SMS Japan";
    case GG_ROM_SMS_EXPORT: return "SMS Export";
    case GG_ROM_GG_JAPAN: return "GG Japan";
    case GG_ROM_GG_EXPORT: return "GG Export";
    case GG_ROM_GG_INTERNATIONAL: return "GG International";
    case GG_ROM_UNK:
    default: return "UNK";
    }
  
} /* end get_region */


static const char *
get_rom_size (
              const int size
              )
{
  
  static char buffer[6];
  
  
  if ( size == -1 ) return "UNK";
  else if ( size == 1024 ) return "1MB";
  else
    {
      sprintf ( buffer, "%dKB", size );
      return &(buffer[0]);
    }
  
} /* end get_rom_size */


static void
init_GL (void)
{
  
  /* Configuració per a 2D. */
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glViewport ( 0, 0, WIDTH, HEIGHT );
  glOrtho( 0, WIDTH, HEIGHT, 0, -1, 1 );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glDisable ( GL_DEPTH_TEST );
  glDisable ( GL_LIGHTING );
  glDisable ( GL_BLEND );
  glDisable ( GL_DITHER );
  
  /* Crea textura. */
  glEnable ( GL_TEXTURE_RECTANGLE_NV );
  glGenTextures ( 1, &_screen.textureid );
  glBindTexture ( GL_TEXTURE_RECTANGLE_NV, _screen.textureid );
  glTexParameteri( GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  
} /* end init_GL */


static void
init_palette (void)
{
  
  uint32_t color;
  double frac;
  int i;
  
  
  color= 0;
  frac= 255.0/15.0;
  for ( i= 0; i < 4096; ++i )
    {
      ((uint8_t *) &color)[0]= (uint8_t) (((i&0xF)*frac) + 0.5);
      ((uint8_t *) &color)[1]= (uint8_t) ((((i&0xF0)>>4)*frac) + 0.5);
      ((uint8_t *) &color)[2]= (uint8_t) ((((i&0xF00)>>8)*frac) + 0.5);
      _palette[i]= color;
    }
  
} /* end init_palette */


static void
screen_update (void)
{
  
  glClear ( GL_COLOR_BUFFER_BIT );
  glTexImage2D( GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA,
                WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                _screen.data );
  glBegin ( GL_QUADS );
  glTexCoord2i ( 0, HEIGHT );
  glVertex2i ( 0, HEIGHT );
  glTexCoord2i ( WIDTH, HEIGHT );
  glVertex2i ( WIDTH, HEIGHT );
  glTexCoord2i ( WIDTH, 0 );
  glVertex2i ( WIDTH, 0 );
  glTexCoord2i ( 0, 0 );
  glVertex2i ( 0, 0 );
  glEnd ();
  
  SDL_GL_SwapBuffers ();
  
} /* end scree_update */


static void
screen_clear (void)
{
  
  memset ( _screen.data, 0, sizeof(uint32_t)*23040 );
  screen_update ();
  
} /* end screen_clear */


static void
audio_callback (
        	void  *userdata,
        	Uint8 *stream,
        	int    len
        	)
{
  
  int i;
  
  
  assert ( _audio.size == len );
  if ( _audio_full )
    {
      for ( i= 0; i < len; ++i )
        stream[i]= 127 + (Uint8) ((128*_audio.buffer[i]) + 0.5);
      _audio_full= 0;
    }
  else
    for ( i= 0; i < len; ++i )
      stream[i]= _audio.silence;
  
} /* end audio_callback */


/* Torna 0 si tot ha anat bé. */
static const char *
init_audio (void)
{
  
  SDL_AudioSpec desired, obtained;
  
  
  /* Únic camp de l'estat que s'inicialitza abans. */
  _audio_full= 0;
  
  /* Inicialitza. */
  desired.freq= 44100;
  desired.format= AUDIO_U8;
  desired.channels= 2;
  desired.samples= 2048;
  desired.size= 4096;
  desired.callback= audio_callback;
  desired.userdata= NULL;
  if ( SDL_OpenAudio ( &desired, &obtained ) == -1 )
    return SDL_GetError ();
  if ( obtained.format != desired.format )
    {
      fprintf ( stderr, "Força format audio\n" );
      SDL_CloseAudio ();
      if ( SDL_OpenAudio ( &desired, NULL ) == -1 )
        return SDL_GetError ();
      obtained= desired;
    }
  
  /* Inicialitza estat. */
  _audio.buffer= (double *) malloc ( sizeof(double)*obtained.size );
  _audio.silence= (char) obtained.silence;
  _audio.pos= 0;
  _audio.size= obtained.size;
  if ( obtained.freq >= GG_PSG_SAMPLES_PER_SEC )
    {
      SDL_CloseAudio ();
      return "Freqüència massa gran";
    }
  _audio.ratio= GG_PSG_SAMPLES_PER_SEC / (double) obtained.freq;
  _audio.pos2= 0.0;
  
  return NULL;
  
} /* end init_audio */


static void
close_audio (void)
{
  
  SDL_CloseAudio ();
  free ( _audio.buffer );
  
} /* end close_audio */




/************/
/* FRONTEND */
/************/

static void
warning (
         void       *udata,
         const char *format,
         ...
         )
{
  
  va_list ap;
  
  
  va_start ( ap, format );
  fprintf ( stderr, "Warning: " );
  vfprintf ( stderr, format, ap );
  putc ( '\n', stderr );
  va_end ( ap );
  
} /* end warning */


static Z80u8 *
get_external_ram (
        	  void *udata
        	  )
{
  static Z80u8 sram[0x8000];
  return sram;
} /* end get_external_ram */


static void
update_screen (
               const int  fb[23040],
               void      *udata
               )
{
  
  int i;
  
  
  for ( i= 0; i < 23040; ++i )
    _screen.data[i]= _palette[fb[i]];
  screen_update ();
  
} /* end update_screen */


static void
check_signals (
               Z80_Bool *stop,
               void     *udata
               )
{
  
  SDL_Event event;
  
  
  *stop= Z80_FALSE;
  while ( SDL_PollEvent ( &event ) )
    switch ( event.type )
      {
      case SDL_ACTIVEEVENT:
        if ( event.active.state&SDL_APPINPUTFOCUS &&
             !event.active.gain )
          _control= 0x00;
        break;
      case SDL_VIDEOEXPOSE: screen_update (); break;
      case SDL_KEYDOWN:
        if ( event.key.keysym.mod&KMOD_CTRL )
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_q: *stop= Z80_TRUE; break;
              default: break;
              }
          }
        else
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_SPACE: _control|= GG_START; break;
              case SDLK_w: _control|= GG_UP; break;
              case SDLK_s: _control|= GG_DOWN; break;
              case SDLK_a: _control|= GG_LEFT; break;
              case SDLK_d: _control|= GG_RIGHT; break;
              case SDLK_o: _control|= GG_TL; break;
              case SDLK_p: _control|= GG_TR; break;
              default: break;
              }
          }
        break;
      case SDL_KEYUP:
        switch ( event.key.keysym.sym )
          {
          case SDLK_SPACE: _control&= ~GG_START; break;
          case SDLK_w: _control&= ~GG_UP; break;
          case SDLK_s: _control&= ~GG_DOWN; break;
          case SDLK_a: _control&= ~GG_LEFT; break;
          case SDLK_d: _control&= ~GG_RIGHT; break;
          case SDLK_o: _control&= ~GG_TL; break;
          case SDLK_p: _control&= ~GG_TR; break;
          default: break;
          }
        break;
      default: break;
      }
  
} /* end check_signals */


static int
check_buttons (
               void *udata
               )
{
  return _control;
} /* end check_buttons */


static void
play_sound (
            const double  left[GG_PSG_BUFFER_SIZE],
            const double  right[GG_PSG_BUFFER_SIZE],
            void         *udata
            )
{
  int nofull, j;
  
  
  for (;;)
    {
      
      while ( _audio_full ) SDL_Delay ( 1 );
      
      j= (int) (_audio.pos2 + 0.5);
      while ( (nofull= (_audio.pos != _audio.size)) &&
              j < GG_PSG_BUFFER_SIZE )
        {
          _audio.buffer[_audio.pos++]= left[j];
          _audio.buffer[_audio.pos++]= right[j];
          _audio.pos2+= _audio.ratio;
          j= (int) (_audio.pos2 + 0.5);
        }
      if ( !nofull )
        {
          _audio.pos= 0;
          _audio_full= 1;
        }
      if ( j >= GG_PSG_BUFFER_SIZE )
        {
          _audio.pos2-= GG_PSG_BUFFER_SIZE;
          break;
        }
      
    }
  
} /* end play_sound */


static void
mem_access (
            const GG_MemAccessType  type,
            const Z80u16            addr,
            const Z80u8             data,
            void                   *udata
            )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mem_access ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mem_access",
        		     "iHB", type, addr, data );
  ret= PyObject_CallMethod ( _tracer.obj, "mapper_changed", "" );
  Py_XDECREF ( ret );
  
} /* end mem_access */


static void
mapper_changed (
        	void *udata
        	)
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mapper_changed ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mapper_changed", "" );
  Py_XDECREF ( ret );
  
} /* end mapper_changed */


static void
cpu_step (
          const Z80_Step *step,
          const Z80u16    nextaddr,
          void           *udata
          )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_cpu_step ||
       PyErr_Occurred () != NULL ) return;
  switch ( step->type )
    {
    case Z80_STEP_INST:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step",
        			 "iHiiiy#(BbH(bH))(BbH(bH))",
        			 Z80_STEP_INST,
        			 nextaddr,
        			 step->val.inst.id.name,
        			 step->val.inst.id.op1,
        			 step->val.inst.id.op2,
        			 step->val.inst.bytes,
        			 step->val.inst.nbytes,
        			 step->val.inst.e1.byte,
        			 step->val.inst.e1.desp,
        			 step->val.inst.e1.addr_word,
        			 step->val.inst.e1.branch.desp,
        			 step->val.inst.e1.branch.addr,
        			 step->val.inst.e2.byte,
        			 step->val.inst.e2.desp,
        			 step->val.inst.e2.addr_word,
        			 step->val.inst.e2.branch.desp,
        			 step->val.inst.e2.branch.addr );
      break;
    case Z80_STEP_NMI:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step", "iH",
        			 Z80_STEP_NMI, nextaddr );
      break;
    case Z80_STEP_IRQ:
      ret= PyObject_CallMethod ( _tracer.obj, "cpu_step", "iHB",
        			 Z80_STEP_IRQ, nextaddr, step->val.bus );
      break;
    default: ret= NULL;
    }
  Py_XDECREF ( ret );
  
} /* end cpu_step */




/******************/
/* FUNCIONS MÒDUL */
/******************/

static PyObject *
GG_close (
          PyObject *self,
          PyObject *args
          )
{
  
  if ( !_initialized ) Py_RETURN_NONE;
  
  close_audio ();
  glDeleteTextures ( 1, &_screen.textureid );
  SDL_Quit ();
  if ( _rom.banks != NULL ) GG_rom_free ( _rom );
  _initialized= FALSE;
  Py_XDECREF ( _tracer.obj );
  
  Py_RETURN_NONE;
  
} /* end GG_close */


static PyObject *
GG_get_cram (
             PyObject *self,
             PyObject *args
             )
{
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  return PyBytes_FromStringAndSize ( (const char *) GG_vdp_get_cram (), 64 );
  
} /* end GG_get_cram */


static PyObject *
GG_get_vram (
             PyObject *self,
             PyObject *args
             )
{
  
  GG_VRAMState state;
  PyObject *dict, *aux;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  GG_vdp_get_vram ( &state );
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  aux= PyLong_FromLong ( state.nt_addr );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "NT", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  aux= PyLong_FromLong ( state.sat_addr );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "SAT", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  aux= PyBytes_FromStringAndSize ( (const char *) state.ram, 16*1024 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "RAM", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  
  return dict;
  
 error:
  Py_DECREF ( dict );
  return NULL;
  
} /* end GG_get_vram */


static PyObject *
GG_get_mapper_state (
        	     PyObject *self,
        	     PyObject *args
        	     )
{
  
  PyObject *dict, *aux;
  GG_MapperState state;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  GG_mem_get_mapper_state ( &state );
  
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  aux= PyLong_FromLong ( state.p0 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "area0", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.p1 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "area1", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.p2 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "area2", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.shift );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "shift", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  
  return dict;

 error:
  Py_DECREF ( dict );
  return NULL;  
  
} /* end GG_get_mapper_state */


static PyObject *
GG_get_rom (
            PyObject *self,
            PyObject *args
            )
{
  
  PyObject *dict, *aux, *aux2;
  int n;
  GG_RomHeader header;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  
  /* Número de Pàgines. */
  aux= PyLong_FromLong ( _rom.nbanks );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "nbanks", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  
  /* Pàgines. */
  aux= PyTuple_New ( _rom.nbanks );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "banks", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  for ( n= 0; n < _rom.nbanks; ++n )
    {
      aux2= PyBytes_FromStringAndSize ( ((const char *) _rom.banks[n]),
        				GG_BANK_SIZE );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, n, aux2 );
    }
  
  /* Capçalera. */
  if ( GG_rom_get_header ( &_rom, &header ) == 0 )
    {
      
      /* Checksum. */
      aux= PyLong_FromLong ( header.checksum );
      if ( aux == NULL ) goto error;
      if ( PyDict_SetItemString ( dict, "checksum", aux ) == -1 )
        { Py_XDECREF ( aux ); goto error; }
      Py_XDECREF ( aux );
      
      /* Product Code. */
      aux= PyLong_FromLong ( header.code );
      if ( aux == NULL ) goto error;
      if ( PyDict_SetItemString ( dict, "code", aux ) == -1 )
        { Py_XDECREF ( aux ); goto error; }
      Py_XDECREF ( aux );
      
      /* Version. */
      aux= PyLong_FromLong ( header.version );
      if ( aux == NULL ) goto error;
      if ( PyDict_SetItemString ( dict, "version", aux ) == -1 )
        { Py_XDECREF ( aux ); goto error; }
      Py_XDECREF ( aux );
      
      /* Region Code. */
      aux= PyUnicode_FromString ( get_region ( header.region ) );
      if ( aux == NULL ) goto error;
      if ( PyDict_SetItemString ( dict, "region", aux ) == -1 )
        { Py_XDECREF ( aux ); goto error; }
      Py_XDECREF ( aux );
      
      /* Rom Size. */
      aux= PyUnicode_FromString ( get_rom_size ( header.size ) );
      if ( aux == NULL ) goto error;
      if ( PyDict_SetItemString ( dict, "size", aux ) == -1 )
        { Py_XDECREF ( aux ); goto error; }
      Py_XDECREF ( aux );
      
    }
  
  return dict;
  
 error:
  Py_XDECREF ( dict );
  return NULL;
  
} /* end GG_get_rom */


static PyObject *
GG_init_module (
        	PyObject *self,
        	PyObject *args
        	)
{
  
  const char *err;
  
  
  if ( _initialized ) Py_RETURN_NONE;
  
  /* SDL */
  if ( SDL_Init ( SDL_INIT_VIDEO |
        	  SDL_INIT_NOPARACHUTE |
        	  SDL_INIT_AUDIO ) == -1 )
    {
      PyErr_SetString ( GGError, SDL_GetError () );
      return NULL;
    }
  _screen.width= WIDTH;
  _screen.height= HEIGHT;
  _screen.surface= SDL_SetVideoMode ( _screen.width, _screen.height, 32,
        			      SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER |
        			      SDL_OPENGL );
  if ( _screen.surface == NULL )
    {
      PyErr_SetString ( GGError, SDL_GetError () );
      SDL_Quit ();
      return NULL;
    }
  init_GL ();
  init_palette ();
  screen_clear ();
  SDL_WM_SetCaption ( "GG", "GG" );
  if ( (err= init_audio ()) != NULL )
    {
      PyErr_SetString ( GGError, err );
      SDL_Quit ();
      return NULL; 
    }
  
  /* ROM */
  _rom.banks= NULL;
  
  /* Tracer. */
  _tracer.obj= NULL;
  
  _initialized= TRUE;
    
  Py_RETURN_NONE;
  
} /* end GG_init_module */


static PyObject *
GG_loop_module (
        	PyObject *self,
        	PyObject *args
        	)
{
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  _audio_full= 0;
  SDL_PauseAudio ( 0 );
  GG_loop ();
  SDL_PauseAudio ( 1 );
  
  Py_RETURN_NONE;
  
} /* end GG_loop_module */


static PyObject *
GG_set_rom (
            PyObject *self,
            PyObject *args
            )
{
  
  static const GG_TraceCallbacks trace_callbacks=
    {
      mem_access,
      mapper_changed,
      cpu_step
    };
  static const GG_Frontend frontend=
    {
      warning,
      get_external_ram,
      update_screen,
      check_signals,
      check_buttons,
      play_sound,
      &trace_callbacks
    };
  
  PyObject *bytes;
  Py_ssize_t size, n;
  char *banks;
  const char *data;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O!", &PyBytes_Type, &bytes ) )
    return NULL;
  
  size= PyBytes_Size ( bytes );
  if ( size <= 0 || size%(GG_BANK_SIZE) != 0 )
    {
      PyErr_SetString ( GGError, "Invalid ROM size" );
      return NULL;
    }
  if ( _rom.banks != NULL ) GG_rom_free ( _rom );
  _rom.nbanks= size/(GG_BANK_SIZE);
  GG_rom_alloc ( _rom );
  if ( _rom.banks == NULL ) return PyErr_NoMemory ();
  data= PyBytes_AS_STRING ( bytes );
  banks= (char *) (_rom.banks);
  for ( n= 0; n < size; ++n ) banks[n]= data[n];
  
  /* Inicialitza el simulador. */
  screen_clear ();
  _control= 0;
  GG_init ( &_rom, &frontend, NULL );
  
  Py_RETURN_NONE;
  
} /* end GG_set_rom */


static PyObject *
GG_set_tracer (
               PyObject *self,
               PyObject *args
               )
{
  
  PyObject *aux;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O", &aux ) )
    return NULL;
  Py_XDECREF ( _tracer.obj );
  _tracer.obj= aux;
  Py_INCREF ( _tracer.obj );
  
  if ( _tracer.obj != NULL )
    {
      _tracer.has_mem_access= has_method ( _tracer.obj, "mem_access" );
      _tracer.has_cpu_step= has_method ( _tracer.obj, "cpu_step" );
      _tracer.has_mapper_changed= has_method ( _tracer.obj, "mapper_changed" );
    }
  
  Py_RETURN_NONE;
  
} /* end GG_set_tracer */


static PyObject *
GG_trace_module (
        	 PyObject *self,
        	 PyObject *args
        	 )
{
  
  int cc;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  SDL_PauseAudio ( 0 );
  cc= GG_trace ();
  SDL_PauseAudio ( 1 );
  if ( PyErr_Occurred () != NULL ) return NULL;
  
  return PyLong_FromLong ( cc );
  
} /* end GG_trace_module */




/************************/
/* INICIALITZACIÓ MÒDUL */
/************************/

static PyMethodDef GGMethods[]=
  {
    { "close", GG_close, METH_VARARGS,
      "Free module resources and close the module" },
    { "get_cram", GG_get_cram, METH_VARARGS,
      "Get a copy of the current vdp color ram" },
    { "get_vram", GG_get_vram, METH_VARARGS,
      "Get a copy of the current vdp ram" },
    { "get_mapper_state", GG_get_mapper_state, METH_VARARGS,
      "Get the current state of the memory mapper structured"
      " into a dictionary" },
    { "get_rom", GG_get_rom, METH_VARARGS,
      "Get the ROM structured into a dictionary" },
    { "init", GG_init_module, METH_VARARGS,
      "Initialize the module" },
    { "loop", GG_loop_module, METH_VARARGS,
      "Run the simulator into a loop and block" },
    { "set_rom", GG_set_rom, METH_VARARGS,
      "Set a ROM into the simulator. The ROM should be of type bytes" },
    { "set_tracer", GG_set_tracer, METH_VARARGS,
      "Set a python object to trace the execution. The object can"
      " implement one of these methods:\n"
      " - cpu_step: To trace the cpu execution. The first and second"
      " parameter are always the step identifier (INST|NMI|IRQ) and"
      " the next memory address. The other parameters depends on the"
      " identifier:\n"
      " -- INST: instruction id., operator 1 id., operator 2 id., bytes,"
      " tuple with extra info containing: (byte,desp,address/word,"
      "(branch.desp,branch.address))\n"
      " -- NMI: nothing\n"
      " -- IRQ: The bus value\n"
      " - mapper_changed: Called every time the memory mapper is changed."
      " No arguments are passed\n"
      " - mem_access: Called every time a RAM access is done. The type"
      " of access, the address inside the RAM and the transmitted data "
      "is passed as arguments" },
    { "trace", GG_trace_module, METH_VARARGS,
      "Executes the next instruction or interruption in trace mode" },
    { NULL, NULL, 0, NULL }
  };


static struct PyModuleDef GGmodule=
  {
    PyModuleDef_HEAD_INIT,
    "GG",
    NULL,
    -1,
    GGMethods
  };


PyMODINIT_FUNC
PyInit_GG (void)
{
  
  PyObject *m;
  
  
  m= PyModule_Create ( &GGmodule );
  if ( m == NULL ) return NULL;
  
  _initialized= FALSE;
  GGError= PyErr_NewException ( "GG.error", NULL, NULL );
  Py_INCREF ( GGError );
  PyModule_AddObject ( m, "error", GGError );
  
  /* Tipus de pasos. */
  PyModule_AddIntConstant ( m, "INST", Z80_STEP_INST );
  PyModule_AddIntConstant ( m, "NMI", Z80_STEP_NMI );
  PyModule_AddIntConstant ( m, "IRQ", Z80_STEP_IRQ );
  
  /* Mnemonics. */
  PyModule_AddIntConstant ( m, "UNK", Z80_UNK );
  PyModule_AddIntConstant ( m, "LD", Z80_LD );
  PyModule_AddIntConstant ( m, "PUSH", Z80_PUSH );
  PyModule_AddIntConstant ( m, "POP", Z80_POP );
  PyModule_AddIntConstant ( m, "EX", Z80_EX );
  PyModule_AddIntConstant ( m, "EXX", Z80_EXX );
  PyModule_AddIntConstant ( m, "LDI", Z80_LDI );
  PyModule_AddIntConstant ( m, "LDIR", Z80_LDIR );
  PyModule_AddIntConstant ( m, "LDD", Z80_LDD );
  PyModule_AddIntConstant ( m, "LDDR", Z80_LDDR );
  PyModule_AddIntConstant ( m, "CPI", Z80_CPI );
  PyModule_AddIntConstant ( m, "CPIR", Z80_CPIR );
  PyModule_AddIntConstant ( m, "CPD", Z80_CPD );
  PyModule_AddIntConstant ( m, "CPDR", Z80_CPDR );
  PyModule_AddIntConstant ( m, "ADD", Z80_ADD );
  PyModule_AddIntConstant ( m, "ADC", Z80_ADC );
  PyModule_AddIntConstant ( m, "SUB", Z80_SUB );
  PyModule_AddIntConstant ( m, "SBC", Z80_SBC );
  PyModule_AddIntConstant ( m, "AND", Z80_AND );
  PyModule_AddIntConstant ( m, "OR", Z80_OR );
  PyModule_AddIntConstant ( m, "XOR", Z80_XOR );
  PyModule_AddIntConstant ( m, "CP", Z80_CP );
  PyModule_AddIntConstant ( m, "INC", Z80_INC );
  PyModule_AddIntConstant ( m, "DEC", Z80_DEC );
  PyModule_AddIntConstant ( m, "DAA", Z80_DAA );
  PyModule_AddIntConstant ( m, "CPL", Z80_CPL );
  PyModule_AddIntConstant ( m, "NEG", Z80_NEG );
  PyModule_AddIntConstant ( m, "CCF", Z80_CCF );
  PyModule_AddIntConstant ( m, "SCF", Z80_SCF );
  PyModule_AddIntConstant ( m, "NOP", Z80_NOP );
  PyModule_AddIntConstant ( m, "HALT", Z80_HALT );
  PyModule_AddIntConstant ( m, "DI", Z80_DI );
  PyModule_AddIntConstant ( m, "EI", Z80_EI );
  PyModule_AddIntConstant ( m, "IM0", Z80_IM0 );
  PyModule_AddIntConstant ( m, "IM1", Z80_IM1 );
  PyModule_AddIntConstant ( m, "IM2", Z80_IM2 );
  PyModule_AddIntConstant ( m, "RLCA", Z80_RLCA );
  PyModule_AddIntConstant ( m, "RLA", Z80_RLA );
  PyModule_AddIntConstant ( m, "RRCA", Z80_RRCA );
  PyModule_AddIntConstant ( m, "RRA", Z80_RRA );
  PyModule_AddIntConstant ( m, "RLC", Z80_RLC );
  PyModule_AddIntConstant ( m, "RL", Z80_RL );
  PyModule_AddIntConstant ( m, "RRC", Z80_RRC );
  PyModule_AddIntConstant ( m, "RR", Z80_RR );
  PyModule_AddIntConstant ( m, "SLA", Z80_SLA );
  PyModule_AddIntConstant ( m, "SRA", Z80_SRA );
  PyModule_AddIntConstant ( m, "SRL", Z80_SRL );
  PyModule_AddIntConstant ( m, "RLD", Z80_RLD );
  PyModule_AddIntConstant ( m, "RRD", Z80_RRD );
  PyModule_AddIntConstant ( m, "BIT", Z80_BIT );
  PyModule_AddIntConstant ( m, "SET", Z80_SET );
  PyModule_AddIntConstant ( m, "RES", Z80_RES );
  PyModule_AddIntConstant ( m, "JP", Z80_JP );
  PyModule_AddIntConstant ( m, "JR", Z80_JR );
  PyModule_AddIntConstant ( m, "DJNZ", Z80_DJNZ );
  PyModule_AddIntConstant ( m, "CALL", Z80_CALL );
  PyModule_AddIntConstant ( m, "RET", Z80_RET );
  PyModule_AddIntConstant ( m, "RETI", Z80_RETI );
  PyModule_AddIntConstant ( m, "RETN", Z80_RETN );
  PyModule_AddIntConstant ( m, "RST00", Z80_RST00 );
  PyModule_AddIntConstant ( m, "RST08", Z80_RST08 );
  PyModule_AddIntConstant ( m, "RST10", Z80_RST10 );
  PyModule_AddIntConstant ( m, "RST18", Z80_RST18 );
  PyModule_AddIntConstant ( m, "RST20", Z80_RST20 );
  PyModule_AddIntConstant ( m, "RST28", Z80_RST28 );
  PyModule_AddIntConstant ( m, "RST30", Z80_RST30 );
  PyModule_AddIntConstant ( m, "RST38", Z80_RST38 );
  PyModule_AddIntConstant ( m, "IN", Z80_IN );
  PyModule_AddIntConstant ( m, "INI", Z80_INI );
  PyModule_AddIntConstant ( m, "INIR", Z80_INIR );
  PyModule_AddIntConstant ( m, "IND", Z80_IND );
  PyModule_AddIntConstant ( m, "INDR", Z80_INDR );
  PyModule_AddIntConstant ( m, "OUT", Z80_OUT );
  PyModule_AddIntConstant ( m, "OUTI", Z80_OUTI );
  PyModule_AddIntConstant ( m, "OTIR", Z80_OTIR );
  PyModule_AddIntConstant ( m, "OUTD", Z80_OUTD );
  PyModule_AddIntConstant ( m, "OTDR", Z80_OTDR );
  
  /* MODES */
  PyModule_AddIntConstant ( m, "NONE", Z80_NONE );
  PyModule_AddIntConstant ( m, "A", Z80_A );
  PyModule_AddIntConstant ( m, "B", Z80_B );
  PyModule_AddIntConstant ( m, "C", Z80_C );
  PyModule_AddIntConstant ( m, "D", Z80_D );
  PyModule_AddIntConstant ( m, "E", Z80_E );
  PyModule_AddIntConstant ( m, "H", Z80_H );
  PyModule_AddIntConstant ( m, "L", Z80_L );
  PyModule_AddIntConstant ( m, "I", Z80_I );
  PyModule_AddIntConstant ( m, "R", Z80_R );
  PyModule_AddIntConstant ( m, "BYTE", Z80_BYTE );
  PyModule_AddIntConstant ( m, "pHL", Z80_pHL );
  PyModule_AddIntConstant ( m, "pBC", Z80_pBC );
  PyModule_AddIntConstant ( m, "pDE", Z80_pDE );
  PyModule_AddIntConstant ( m, "pSP", Z80_pSP );
  PyModule_AddIntConstant ( m, "pIX", Z80_pIX );
  PyModule_AddIntConstant ( m, "pIY", Z80_pIY );
  PyModule_AddIntConstant ( m, "pIXd", Z80_pIXd );
  PyModule_AddIntConstant ( m, "pIYd", Z80_pIYd );
  PyModule_AddIntConstant ( m, "ADDR", Z80_ADDR );
  PyModule_AddIntConstant ( m, "BC", Z80_BC );
  PyModule_AddIntConstant ( m, "DE", Z80_DE );
  PyModule_AddIntConstant ( m, "HL", Z80_HL );
  PyModule_AddIntConstant ( m, "SP", Z80_SP );
  PyModule_AddIntConstant ( m, "IX", Z80_IX );
  PyModule_AddIntConstant ( m, "IXL", Z80_IXL );
  PyModule_AddIntConstant ( m, "IXH", Z80_IXH );
  PyModule_AddIntConstant ( m, "IY", Z80_IY );
  PyModule_AddIntConstant ( m, "IYL", Z80_IYL );
  PyModule_AddIntConstant ( m, "IYH", Z80_IYH );
  PyModule_AddIntConstant ( m, "AF", Z80_AF );
  PyModule_AddIntConstant ( m, "AF2", Z80_AF2 );
  PyModule_AddIntConstant ( m, "B0", Z80_B0 );
  PyModule_AddIntConstant ( m, "B1", Z80_B1 );
  PyModule_AddIntConstant ( m, "B2", Z80_B2 );
  PyModule_AddIntConstant ( m, "B3", Z80_B3 );
  PyModule_AddIntConstant ( m, "B4", Z80_B4 );
  PyModule_AddIntConstant ( m, "B5", Z80_B5 );
  PyModule_AddIntConstant ( m, "B6", Z80_B6 );
  PyModule_AddIntConstant ( m, "B7", Z80_B7 );
  PyModule_AddIntConstant ( m, "WORD", Z80_WORD );
  PyModule_AddIntConstant ( m, "F_NZ", Z80_F_NZ );
  PyModule_AddIntConstant ( m, "F_Z", Z80_F_Z );
  PyModule_AddIntConstant ( m, "F_NC", Z80_F_NC );
  PyModule_AddIntConstant ( m, "F_C", Z80_F_C );
  PyModule_AddIntConstant ( m, "F_PO", Z80_F_PO );
  PyModule_AddIntConstant ( m, "F_PE", Z80_F_PE );
  PyModule_AddIntConstant ( m, "F_P", Z80_F_P );
  PyModule_AddIntConstant ( m, "F_M", Z80_F_M );
  PyModule_AddIntConstant ( m, "BRANCH", Z80_BRANCH );
  PyModule_AddIntConstant ( m, "pB", Z80_pB );
  PyModule_AddIntConstant ( m, "pC", Z80_pC );
  PyModule_AddIntConstant ( m, "pD", Z80_pD );
  PyModule_AddIntConstant ( m, "pE", Z80_pE );
  PyModule_AddIntConstant ( m, "pH", Z80_pH );
  PyModule_AddIntConstant ( m, "pL", Z80_pL );
  PyModule_AddIntConstant ( m, "pA", Z80_pA );
  PyModule_AddIntConstant ( m, "pBYTE", Z80_pBYTE );
  
  /* TIPUS D'ACCESSOS A MEMÒRIA. */
  PyModule_AddIntConstant ( m, "READ", GG_READ );
  PyModule_AddIntConstant ( m, "WRITE", GG_WRITE );
  
  return m;
  
} /* end PyInit_GG */

void Z80_reti_signal (void) {}
