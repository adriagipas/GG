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
 *  GG.h - Simulador de la 'GameGear' escrit en ANSI C.
 *
 */

#ifndef __GG_H__
#define __GG_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "Z80.h"


/*********/
/* TIPUS */
/*********/

/* Funció per a metre avísos. */
typedef void 
(GG_Warning) (
              void       *udata,
              const char *format,
              ...
              );




/*******/
/* ROM */
/*******/

/* Grandària d'un 'bank' de rom. */
#define GG_BANK_SIZE 16384

/* 'Bank'. */
typedef Z80u8 GG_Bank[GG_BANK_SIZE];

/* Valor nul per al tipus 'Bank'. */
#define GG_BANK_NULL ((GG_Bank *) NULL)

/* Estructura per a guardar una ROM. */
typedef struct
{
  
  int      nbanks;    /* Número de 'banks's. */
  GG_Bank *banks;     /* 'Bank's. */
  
} GG_Rom;

/* Estructura per a emmagatzemar la informació de la capçalera d'una
 * ROM de manera accessible.
 */
typedef struct
{
  
  Z80u16 checksum;    /* Checksum. En les ROMs que no són d'exportació
        		 el checksum no sol ser correcte. */
  int    code;        /* Codi del producte. */
  char   version;     /* Versió. */
  enum
    {
      GG_ROM_SMS_JAPAN,
      GG_ROM_SMS_EXPORT,
      GG_ROM_GG_JAPAN,
      GG_ROM_GG_EXPORT,
      GG_ROM_GG_INTERNATIONAL,
      GG_ROM_UNK
    }    region;      /* Regió. */
  int    size;        /* Grandària de la ROM en KB. -1 indica
        		 grandària desconeguda. */
  
} GG_RomHeader;

/* Reserva memòria per als 'bank's d'una variable 'GG_Rom'. Si la
 * variable continua a NULL s'ha pdrouït un error.
 */
#define GG_rom_alloc(ROM)        					\
  ((ROM).banks= (GG_Bank *) malloc ( sizeof(GG_Bank)*(ROM).nbanks ))

/* Si s'ha reservat memòria en la ROM la llibera. */
#define GG_rom_free(ROM)        		   \
  do {        					   \
  if ( (ROM).banks != NULL ) free ( (ROM).banks ); \
  } while(0)

/* Obté la capçalera d'una ROM. Torna 0 si s'ha trobat la capçalera,
 * -1 en cas contrari.
 */
int
GG_rom_get_header (
        	   const GG_Rom *rom,
        	   GG_RomHeader *header
        	   );


/*******/
/* MEM */
/*******/
/* Mòdul que simula el mapa de memòria, i com es mapeja, de la GG. */

/* Tipus de la funció utilitzada per a demanar la memòria RAM
 * externa. La memòria RAM externa es supossa estàtica i de 32K.
 */
typedef Z80u8 * (GG_GetExternalRAM) (
        			     void *udata
        			     );

/* Tipus d'accessos a memòria RAM. */
typedef enum
  {
    GG_READ,
    GG_WRITE
  } GG_MemAccessType;

/* Tipus de la funció per a fer una traça dels accessos a
 * memòria RAM. Cada vegada que es produeix un accés a memòria es
 * crida. No inclou accessos a memòria SRAM.
 */
typedef void (GG_MemAccess) (
        		     const GG_MemAccessType  type,    /* Tipus. */
        		     const Z80u16            addr,    /* Adreça
        							 dins
        							 de la
        							 memòria
        							 RAM,
        							 es a
        							 dir
        							 en el
        							 rang
        							 [0000:1FFF]. */
        		     const Z80u8             data, /* Dades
        						      transmitides. */
        		     void                   *udata
        		     );

/* Tipus de la funció que es cridada cada vegada que la disposició de
 * la memòria canvia.
 */
typedef void (GG_MapperChanged) (
        			 void *udata
        			 );

/* Tipus per a emmagatzemar l'estat del mapejador de memòria. */
typedef struct
{
  
  int p0;       /* Pàgina en [$0000-$3ffff]. */
  int p1;       /* Pàgina en [$4000-$4ffff]. */
  int p2;       /* Pàgina en [$8000-$bffff]. */
  int shift;    /* Desplaçament actual. */
  
} GG_MapperState;

/* Obté en la variable indicada l'estat actual del mapejador de
 * memòria.
 */
void
GG_mem_get_mapper_state (
        		 GG_MapperState *state    /* Punter a
        					     l'estructura a
        					     plenar. */
        		 );

/* Inicialitza el mòdul. */
void
GG_mem_init (
             const GG_Rom      *rom,                 /* ROM. */
             GG_GetExternalRAM *get_external_ram,
             GG_MemAccess      *mem_acces,           /* Pot ser NULL. */
             GG_MapperChanged  *mapper_changed,      /* Pot ser NULL. */
             void              *udata
             );

/* Com GG_mem_init però no cal tornar a passar callbacks ni ROM. No
 * altera el mode traça.
 */
void
GG_mem_init_state (void);

/* Activa/Desactiva el mode traça en el mòdul de memòria. */
void
GG_mem_set_mode_trace (
        	       const Z80_Bool val
        	       );

int
GG_mem_save_state (
        	   FILE *f
        	   );

int
GG_mem_load_state (
        	   FILE *f
        	   );


/*******/
/* VDP */
/*******/
/* Mòdul que simula el xip gràfic de la GameGear. */

/* Tipus de la funció que actualitza la pantalla real. FB és el buffer
 * amb una imatge de 160x144, on cada valor és un sencer entre
 * [0,4095] amb formar BBBBGGGGRRRR.
 */
typedef void (GG_UpdateScreen) (
        			const int  fb[23040 /*160x144*/],
        			void      *udata
        			);

/* Estructura utilitzada per a accedir al contingut actual de la
 * memòria de vídeo.
 */
typedef struct
{
  
  const Z80u8 *ram;         /* Punter als 16K de memòria RAM de
        		       vídeo. */
  Z80u16       nt_addr;     /* Name Table Base Address. */
  Z80u16       sat_addr;    /* Sprite Attribute Table Base Address. */

} GG_VRAMState;

/* Alimenta el dispositiu amb cicles de rellotge de la UCP. */
void
GG_vdp_clock (
              const int cc
              );

/* Torna un punter a la memòria de color. */
const Z80u8 *
GG_vdp_get_cram (void);

/* Funció per accedir a l'estat actual de la memòria de vídeo. */
void
GG_vdp_get_vram (
        	 GG_VRAMState *state    /* Adreça a la variable
        				   d'estat. */
        	 );

/* Control. */
void
GG_vdp_control (
        	Z80u8 byte    /* Byte de control. */
        	);

/* Obté l'actual valor de H. */
Z80u8
GG_vdp_get_H (void);

/* Llig el registre d'estat. */
Z80u8
GG_vdp_get_status (void);

/* Obté l'actual valor de V. */
Z80u8
GG_vdp_get_V (void);

/* Inicialitza el mòdul. */
void
GG_vdp_init (
             GG_UpdateScreen *update_screen,
             void            *udata
             );

/* Com GG_vdp_init, però no cal passar altra volta l'estat. */
void
GG_vdp_init_state (void);

/* Llegeix un byte de dades. En teoria en certs casos hi ha que
 * esperar per a llegir dades, però s'entén que el programador ho fa
 * bé.
 */
Z80u8
GG_vdp_read_data (void);

/* El valor de H que es torne és l'actual. */
void
GG_vdp_set_H (void);

/* Escriu un byte de dades. En teoria en certs casos hi ha que esperar
 * per a llegir dades, però s'entén que el programador ho fa bé.
 */
void
GG_vdp_write_data (
        	   Z80u8 byte    /* Byte de dades. */
        	   );

int
GG_vdp_save_state (
        	   FILE *f
        	   );

int
GG_vdp_load_state (
        	   FILE *f
        	   );


/***********/
/* CONTROL */
/***********/
/* Mòdul que simula el control de la GameGear. */

/* Tipus per a indicar amb l'operador lògic OR els botons actualment
 * actius.
 */
typedef enum
  {
    GG_UP= 0x01,
    GG_DOWN= 0x02,
    GG_LEFT= 0x04,
    GG_RIGHT= 0x08,
    GG_TL= 0x10,
    GG_TR= 0x20,
    GG_START= 0x100
  } GG_Button;

/* Tipus d'una funció que obté l'estat dels botons. Un botó està
 * apretat si el seu bit corresponent (GG_Button) està actiu.
 */
typedef int (GG_CheckButtons) (
        		       void *udata
        		       );
        			 
/* Inicialitza el mòdul. */
void
GG_control_init (
        	 GG_CheckButtons *check_buttons,
        	 void            *udata
        	 );

/* Accedeix a l'estat del controlador local (1). */
Z80u8
GG_control_get_status1 (void);

/* Accedeix a l'estat del controlador extern/extensió. */
Z80u8
GG_control_get_status_ext (void);

/* Torna un byte tot a 0 menys el bit 7 on indica l'estat del botó
 * START segons les convencions de la Game Gear.
 */
Z80u8
GG_control_get_status_start (void);


/*******/
/* PSG */
/*******/
/* Mòdul que simula el xip de sò de la Game Gear. */

/* Número de mostres per segon que genera el xip. */
#define GG_PSG_SAMPLES_PER_SEC 223721.5625

/* Número de mostres que té cadascun dels buffers que genera el xip de
 * sò. Es poc més d'una centèsima de segon.
 */
#define GG_PSG_BUFFER_SIZE 2238

/* Tipus de la funció que actualitza es crida per a reproduir so. Es
 * proporcionen el canal de l'esquerra i el de la dreta. Cada mostra
 * està codificada en un valor en el rang [0,1].
 */
typedef void (GG_PlaySound) (
        		     const double  left[GG_PSG_BUFFER_SIZE],
        		     const double  right[GG_PSG_BUFFER_SIZE],
        		     void         *udata
        		     );

/* Alimenta el dispositiu amb cicles de rellotge de la UCP. */
void
GG_psg_clock (
              const int cc
              );

/* Configura el xip. */
void
GG_psg_control (
        	const Z80u8 data
        	);

/* Inicialitza el mòdul. */
void
GG_psg_init (
             GG_PlaySound *play_sound,
             void         *udata
             );

/* Com GG_psg_init però no cal passar callbacks. */
void
GG_psg_init_state (void);

/* Configura l'estéreo. */
void
GG_psg_stereo (
               Z80u8 data
               );

int
GG_psg_save_state (
        	   FILE *f
        	   );

int
GG_psg_load_state (
        	   FILE *f
        	   );


/********/
/* MAIN */
/********/
/* Funcions que un usuari normal deuria usar. */

/* Número aproxima de cicles per segon. */
#define GG_CICLES_PER_SEC 3579545

/* Tipus de funció amb la que el 'frontend' indica a la llibreria si
 * s'ha produït una senyal de parada. A més esta funció pot ser
 * emprada per el frontend per a tractar els events pendents.
 */
typedef void (GG_CheckSignals) (
        			Z80_Bool *stop,
        			void     *udata
        			);

/* Tipus de funció per a saber quin a sigut l'últim pas d'execució de
 * la UCP.
 */
typedef void (GG_CPUStep) (
        		   const Z80_Step *step,        /* Punter a
        						   pas
        						   d'execuió. */
        		   const Z80u16    nextaddr,    /* Següent
        						   adreça de
        						   memòria. */
        		   void           *udata
        		   );

/* No tots els camps tenen que ser distint de NULL. */
typedef struct
{
  
  GG_MemAccess     *mem_access;         /* Es crida cada vegada que es
        				   produïx un accés a
        				   memòria. */
  GG_MapperChanged *mapper_changed;     /* Es crida cada vegada que la
        				   disposició de la memòria
        				   canvia. */
  GG_CPUStep       *cpu_step;           /* Es crida en cada pas de la
        				   UCP. */
  
} GG_TraceCallbacks;

/* Conté la informació necessària per a comunicar-se amb el
 * 'frontend'.
 */
typedef struct
{
  
  GG_Warning              *warning;            /* Funció per a mostrar
        					  avisos. */
  GG_GetExternalRAM       *get_external_ram;   /* Per a obtindre la
        					  memòria externa. */
  GG_UpdateScreen         *update_screen;      /* Actualitza la
        					  pantalla. Es crida
        					  per a tots els
        					  frames. */
  GG_CheckSignals         *check;              /* Comprova si ha de
        					  parar i events
        					  externs. Pot ser
        					  NULL, en eixe cas el
        					  simulador
        					  s'executarà fins que
        					  es cride a
        					  'GG_stop'. */
  GG_CheckButtons         *check_buttons;      /* Comprova l'estat
        					  dels botons. */
  GG_PlaySound            *play_sound;         /* Reprodueix so. */
  const GG_TraceCallbacks *trace;              /* Pot ser NULL si no
        					  es van a gastar les
        					  funcions per a fer
        					  una traça. */
  
} GG_Frontend;

/* Inicialitza la llibreria, s'ha de cridar cada vegada que s'inserte
 * una nova rom.
 */
void
GG_init (
         const GG_Rom      *rom,         /* ROM. */
         const GG_Frontend *frontend,    /* Frontend. */
         void              *udata        /* Dades proporcionades per
        				    l'usuari que són pasades
        				    al 'frontend'. */
         );

/* Executa un cicle de la GameGear. Aquesta funció executa una
 * iteració de 'GG_loop' i torna els cicles de UCP emprats. Si
 * CHECKSIGNALS en el frontend no és NULL aleshores cada cert temps al
 * cridar a GG_iter es fa una comprovació de CHECKSIGNALS.  La funció
 * CHECKSIGNALS del frontend es crida amb una freqüència suficient per
 * a que el frontend tracte els seus events. La senyal stop de
 * CHECKSIGNALS és llegit en STOP si es crida a CHECKSIGNALS.
 */
int
GG_iter (
         Z80_Bool *stop
         );

/* Carrega l'estat de 'f'. Torna 0 si tot ha anat bé. S'espera que el
 * fitxer siga un fitxer d'estat vàlid de GameGear per a la ROM
 * actual. Si es produeix un error de lectura o es compromet la
 * integritat del simulador, aleshores es reiniciarà el simulador.
 */
int
GG_load_state (
               FILE *f
               );

/* Executa la GameGear. Aquesta funció es bloqueja fins que llig una
 * senyal de parada mitjançant CHECKSIGNALS o mitjançant GG_stop, si
 * es para es por tornar a cridar i continuarà on s'havia quedat. La
 * funció CHECKSIGNALS del frontend es crida amb una freqüència
 * suficient per a que el frontend tracte els seus events.
 */
void
GG_loop (void);


/* Escriu en 'f' l'estat de la màquina. Torna 0 si tot ha anat bé, -1
 * en cas contrari.
 */
int
GG_save_state (
               FILE *f
               );

/* Para a 'GG_loop'. */
void
GG_stop (void);

/* Executa els següent pas de UCP en mode traça. Tots aquelles
 * funcions de 'callback' que no són nul·les es cridaran si és el
 * cas. Torna el número de cicles executats en l'últim pas.
 */
int
GG_trace (void);


#endif /* __GG_H__ */
