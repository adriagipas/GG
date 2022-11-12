/*
 * Copyright 2010-2013,2015,2022 Adrià Giménez Pastor.
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
 *  vdp.c - Implementació del el xip gràfic VDP.
 *
 *  NOTES:
 *
 *    En principi una GG funciona com una NTSC. Encara que es poden
 *    programar molts modes de moment sols accepte el mode estàndar:
 *    NTSC, 256x192 (Però visibles sols 160x144) i 262 scanlines -->
 *    M4=1 (M2=0||(M2=1&&M3==0&&M1==0))
 *
 *    LLISTA DE REGISTRES QUE SEGONS LA DOCUMENTACIÓ OFICIAL DEURIEN DE TINDRE
 *    UN VALOR FIXE:
 *      - M4: TRUE
 *      - M2: TRUE
 *      - M1: FALSE
 *      - M3: FALSE
 *      - DSIZE: FALSE
 *
 *    Els flags de col·lissió i excés de sprites en línia, es calculen
 *    a una precisió de nivell de línia.
 *
 *    No he trobat ninguna documentació clara sobre quan ocorren les
 *    coses en meitat d'una línia i com afecta el canvi de coses a
 *    meitat de línia. A continuació les meues suposicions en funció
 *    de com funcionen els jocs meus:
 *
 *    - Els timings estan calculats tenint en compter la documentació oficial.
 *    - El canvia de BLANK sols té efecte quan s'incrementa V. Per tant latch.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "GG.h"
#include "Z80.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)        					\
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)        					\
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)        			\
  if ( !(COND) ) return -1;

#define FFLAG 0x80
#define S9FLAG 0x40
#define CFLAG 0x20


#define INC_ADDR ++_addr; _addr&= 0x3FFF


#define COUNTSPERLINE 171
#define COUNTSTORENDERLINE 111
/* 0x93->0xE9...0xF3[-->INT<--]0xF4. */
#define COUNTSTOILINE 159


/* MACROS DE 'render_line_bg'. */
#define VFLIP 0x400
#define HFLIP 0x200
#define PALETTE 0x800
#define PRIO 0x1000

#define GET_NEXT_NT        			\
  addr= addr_row|addr_col;        		\
  NT= _vram[addr];        			\
  NT|= ((Z80u16) _vram[addr|0x1])<<8;        	\
  if ( (addr_col+= 2) == 64 ) addr_col= 0

/* La taula de patrons és tota la memòria. */
#define CALC_ADDR_PAT        			\
  addr_pat= (NT&0x1FF)<<5;        		\
  addr_pat|= (NT&VFLIP)?sel_bp_flip:sel_bp

#define GET_NEXT_BP        			\
  if ( NT&HFLIP )        			\
    {        					\
      bp0|= reverse_byte ( _vram[addr_pat] );        \
      bp1|= reverse_byte ( _vram[addr_pat|1] );        \
      bp2|= reverse_byte ( _vram[addr_pat|2] );        \
      bp3|= reverse_byte ( _vram[addr_pat|3] );        \
    }        					\
  else        					\
    {        					\
      bp0|= _vram[addr_pat];        		\
      bp1|= _vram[addr_pat|1];        		\
      bp2|= _vram[addr_pat|2];        		\
      bp3|= _vram[addr_pat|3];        		\
    }        					\
  bp4|= (NT&PALETTE) ? 0xFF : 0x00;        	\
  p|= (NT&PRIO) ? 0xFF : 0x00

#define RENDER_LINE_BG_INIT        			     \
  GET_NEXT_NT;        					     \
  CALC_ADDR_PAT;        				     \
  if ( NT&HFLIP )        				     \
    {        						     \
      bp0= ((Z80u16) reverse_byte ( _vram[addr_pat] ))<<8;   \
      bp1= ((Z80u16) reverse_byte ( _vram[addr_pat|1] ))<<8; \
      bp2= ((Z80u16) reverse_byte ( _vram[addr_pat|2] ))<<8; \
      bp3= ((Z80u16) reverse_byte ( _vram[addr_pat|3] ))<<8; \
    }        						     \
  else        						     \
    {        						     \
      bp0= ((Z80u16) _vram[addr_pat])<<8;        	     \
      bp1= ((Z80u16) _vram[addr_pat|1])<<8;        	     \
      bp2= ((Z80u16) _vram[addr_pat|2])<<8;        	     \
      bp3= ((Z80u16) _vram[addr_pat|3])<<8;        	     \
    }        						     \
  bp4= (NT&PALETTE) ? 0xFF00 : 0x0000;        		     \
  p= (NT&PRIO) ? 0xFF00 : 0x0000

#define RENDER_LINE_BG_BODY        		\
  GET_NEXT_NT;        				\
  CALC_ADDR_PAT;        			\
  GET_NEXT_BP;        				\
  for ( j= 0; j < 8; ++j, ++x )        		\
    {        					\
      _render.line_bg[x]=        		\
        ((bp0>>desp0)&0x01) |			\
        ((bp1>>desp1)&0x02) |			\
        ((bp2>>desp2)&0x04) |			\
        ((bp3>>desp3)&0x08) |			\
        ((bp4>>desp4)&0x10);			\
      _render.prior_bg[x]= (p>>desp0)&0x1;        \
      bp0<<= 1; bp1<<= 1;        		\
      bp2<<= 1; bp3<<= 1;        		\
      bp4<<= 1; p<<= 1;        			\
    }


/* El real és 8. Amb 64 no despareixen els sprites. */
#define NUM_SPRITES 64


#define RENDER_LINE_SPR_INCB        		\
  b0<<= 1; b1<<= 1; b2<<= 1; b3<<= 1


#define RENDER_LINE_SPR_GCOLOR        					\
  color= 0x10|(b0>>7)|((b1>>6)&0x2)|((b2>>5)&0x4)|((b3>>4)&0x08)




/*********/
/* ESTAT */
/*********/

/* Memòria. */
static Z80u8 _vram[16384 /*16K*/];
static Z80u8 _cram[64 /*32 words*/];


/* Registre d'estat. */
static Z80u8 _status;


/* 'Flag' de control i buffers. */
static Z80_Bool _control_flag;
static Z80u16 _addr;
static Z80u8 _aux_byte;
static int _code;
static Z80u8 _buffer;
static Z80u8 _cram_latch;


/* Valor de H fixat. */
static Z80u8 _H;


/* Flag que indica que s'ha produït una interrupció a nivell de
   línia. */
static Z80_Bool _line_int_pending_flag;


/* Registres. Molts registres sols tenen sentit en mode 4. */
static struct
{
  
  /* R0 */
  Z80_Bool M2;             /* Must be TRUE for M1/M3 to change screen
        		      height in Mode 4. Otherwise has no
        		      effect. */
  Z80_Bool M4;             /* TRUE: Use mode 4, FALSE: Use TMS9918. */
  Z80_Bool EC;             /* TRUE: The horizontal position of all
        		      sprites shifts eight dots to the
        		      left. */
  Z80_Bool IE1;            /* TRUE: Line interrupt enable. */
  Z80_Bool MVS;            /* TRUE: The two cells at the right end of
        		      the LCD screen are not scrolled in the
        		      vertical direction. */
  
  /* R1 */
  Z80_Bool DSIZE;          /* TRUE: Sprite pixels are doubled in
        		      size. */
  Z80_Bool SIZE;           /* FALSE: Normal, TRUE: The sprite size
        		      becomes 8x16 dots. In this case, the top
        		      seven bits of the character No. are
        		      enabled. En mode TMS9918 sprites
        		      16x16. */
  Z80_Bool M3;             /* TRUE: Selects 240-line screen for Mode 4
        		      if M2=TRUE, else has no effect. */
  Z80_Bool M1;             /* TRUE: Selects 224-line screen for Mode 4
        		      if M2=TRUE, else has no effect. */
  Z80_Bool IE;             /* TRUE: Frame interrupt enable. */
  Z80_Bool BLANK;          /* FALSE: Nothing is displayed on the
        		      screen. In this case, the backdrop color
        		      is displayed, and the wait used for VDP
        		      is unnecessary. TRUE: An image is
        		      displayed on the screen. */
  Z80_Bool BLANKl;         /* 'Latch' que es gasta en meitat de línia. */
  
  /* R2 */
  Z80u16   nt_addr;        /* Name Table Base Address. NOTA: If the
        		      224 or 240-line displays are being used,
        		      only bits 3 and 2 select the table
        		      address like so: 0 0->0700;0 1->1700;1
        		      0->2700;1 1->3700. */
  
  /* R5 */
  Z80u16   sat_addr;        /* Sprite Attribute Table Base Address. */
  
  /* R6 */
  Z80u16   spg_addr;        /* Sprite Pattern Generator Base
        		       Address. */
  
  /* R7 */
  Z80u8    ob_color;        /* Overscan/Backdrop Color. */
  
  /* R8 */
  int      col;             /* Columna inicial. */
  int      fx;              /* Fine X. */
  int      coll;            /* 'Latch' que es gasta en meitat de
        		       línia. */
  int      fxl;             /* 'Latch' que es gasta en meitat de
        		       línia. */
  
  /* R9 */
  int      row;             /* Fila inicial. */
  int      fy;              /* Fine Y. */
  int      row_tmp;         /* Valor temporal. */
  int      fy_tmp;          /* Valor temporal. */
  
  /* R10 */
  int      line_counter;    /* Line counter. */
  
} _regs;


/* Comptadors i cicles per processar. */
static struct
{
  
  int H;           /* Va de 0-170, 171 en total. */
  int V;           /* Va de 0-261, 262 en total. */
  int cc;          /* Cada cicle són 4 comptes. */
  int cctoLInt;    /* Cicles que falten per a la següent interrupció a
        	      nivell de línia. */
  int cctoFInt;    /* Cicles que falten per a la següent interrupció a
        	      nivell de frame. */
  
} _timing;


/* Comptador per a l'interrupció a nivell de línia. */
static int _line_int_counter;


/* Estat renderitzat. */
static struct
{
  
  int  fb[23040 /*160x144*/];    /* Frame buffer. */
  int  lines;                    /* Número de línies reals
        			    renderitzades. */
  int *p;                        /* Apunta al següent píxel a renderitzar. */
  int  line_bg[160];             /* Línia amb el fons dibuixat. */
  int  prior_bg[160];            /* La prioritat de cada píxel. */
  int  line_spr[256];            /* Línia amb els sprites. Per a poder
        			    calcular la col·lissió es
        			    renderitzat tota la línia. */
  
} _render;


/* Sprit buffer. */
static struct
{
  
  int      N;                 /* Número de sprites en el buffer. */
  Z80_Bool SIZE;              /* Valor de SIZE que es va fer
        			 l'evaluació. */
  Z80_Bool DSIZE;             /* Valor de DSIZE quan es va fer
        			 l'evaluació. */
  struct
  {
    int    ind;     /* Número de sprite. */
    Z80u16 baddr;   /* Part de l'adreça per a elegir el bitmap. */
  }        v[NUM_SPRITES];    /* Contingut del buffer. */
  
} _spr_buffer;


/* Dades de l'usari. */
static GG_UpdateScreen *_update_screen;
static void *_udata;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static Z80u8
reverse_byte (
              Z80u8 byte
              )
{
  
  int i;
  Z80u8 ret;
  
  
  ret= 0;
  for ( i= 0; i < 8; ++i )
    {
      ret<<= 1;
      if ( byte&0x1 ) ret|= 0x1;
      byte>>= 1;
    }
  
  return ret;
  
} /* end reverse_byte */


static void
render_line_bg (void)
{
  
  int i, x, j, row, sel_bp, sel_bp_flip, desp0, desp1, desp2, desp3, desp4;
  Z80u16 addr_row, addr_col, addr, NT, addr_pat, bp0, bp1, bp2, bp3, bp4, p;
  
  
  /* Calcula fila (i adreça base) de la 'name table'. */
  row= _render.lines + ((_regs.row<<3)|_regs.fy);
  if ( row >= 224 ) row-= 224; /* EN UN ALTRE MODE ÉS 256. */
  /* NOTA: '_regs.nt_addr' sempre és 00XX X000 0000 0000. */
  addr_row= ((row&0xF8)<<3) | _regs.nt_addr;
  
  /* Selector de bitmap dins d'un patró. */
  sel_bp= (row&0x7)<<2;
  sel_bp_flip= 28/*7<<2*/-sel_bp;
  
  /* Columna anterior a la inicial. */
  addr_col= ((32-_regs.col)+(6-1))<<1;
  if ( addr_col >= 64 ) addr_col-= 64;
  
  /* Desplaçaments. */
  desp0= 7 + _regs.fx;
  desp1= desp0 - 1;
  desp2= desp1 - 1;
  desp3= desp2 - 1;
  desp4= desp3 - 1;
  
  /* Inicialitza el renderitzat. Al principi de cada iteració en la
     part alta tindrà els bitplanes de la columna anterior (si no hi
     haguera fine scrolling) i en la baixa els de l'actual. */
  /* NOTA: En un bitplane el bit més alt és el de l'esquerra en la
     pantalla. */
  RENDER_LINE_BG_INIT;
  
  /* Zona amb Vertical Scroll. */
  for ( x= 0, i= 6; i < 24; ++i ) { RENDER_LINE_BG_BODY }
  
  /* Zona que pot tindre Vertical Scroll a 0. Si és el cas recalcule
     l'adreça base de la 'name table' i torne a llegir l'últim
     tile. */
  if ( _regs.MVS )
    {
      addr_row= ((_render.lines&0xF8)<<3) | _regs.nt_addr;
      if ( addr_col == 0 ) addr_col= 62;
      else addr_col-= 2;
      RENDER_LINE_BG_INIT;
    }
  for ( ; i < 26; ++i ) { RENDER_LINE_BG_BODY }
  
} /* end render_line_bg */


static void
sat_evaluation (
        	int line
        	)
{
  
  int i, diff, diff2, aux;
  Z80u16 addr;
  Z80u8 y;
  
  
  _spr_buffer.N= 0;
  _spr_buffer.SIZE= _regs.SIZE;
  _spr_buffer.DSIZE= _regs.DSIZE;
  aux= _regs.DSIZE ? 1 : 0;
  addr= _regs.sat_addr;
  diff2= _regs.SIZE ? 16 : 8;
  if ( _regs.DSIZE ) diff2<<= 1;
  for ( i= 0; i < 64; ++i, ++addr )
    {
      y= _vram[addr];
      if ( y == 0xD0 ) break; /* Sols en mode 192. */
      diff= (line+(((int) (y^0xFF))|0x100)+1)&0x1FF;
      if ( diff < diff2 )
        {
          if ( _spr_buffer.N == 8 ) _status|= S9FLAG;
          if ( _spr_buffer.N == NUM_SPRITES ) break;
          _spr_buffer.v[_spr_buffer.N].ind= i;
          _spr_buffer.v[_spr_buffer.N].baddr= ((Z80u16) (diff>>aux))<<2;
          ++_spr_buffer.N;
        }
    }
  
} /* end sat_evaluation */


static void
render_line_spr (void)
{
  
  int n, i, x, color;
  Z80u8 mask, b0, b1, b2, b3;
  Z80u16 sat_addr, aux, addr_pat;
  
  
  /* NOTA: 0x10 és el color transparent per als sprites. */
  for ( n= 0; n < 256; ++n ) _render.line_spr[n]= 0x10;
  sat_addr= _regs.sat_addr + 128;
  mask= _spr_buffer.SIZE ? 0xFE : 0xFF;
  for ( n= _spr_buffer.N-1; n >= 0; --n )
    {
      
      aux= sat_addr + (((Z80u16) (_spr_buffer.v[n].ind))<<1);
      
      /* Llig patró. */
      addr_pat=
        _regs.spg_addr | /* Selecciona el conjunt de patterns. */
        (((Z80u16) (_vram[aux+1]&mask))<<5) | /* Selecciona un pattern. */
        _spr_buffer.v[n].baddr; /* Selecciona el bitplane. */
      b0= _vram[addr_pat]; b1= _vram[addr_pat+1];
      b2= _vram[addr_pat+2]; b3= _vram[addr_pat+3];
      
      /* Llig x. */
      x= (int) _vram[aux];
      if ( _regs.EC ) x-= 8;
      
      /* Renderitza. */
      if ( _spr_buffer.DSIZE )
        {
          for ( i= 0; x < 0; ++x, ++i )
            if ( i&0x1 ) { RENDER_LINE_SPR_INCB; }
          RENDER_LINE_SPR_GCOLOR; /*  L'últim color.*/
          for ( ; i < 16 && x < 256; ++i, ++x  )
            {
              if ( i&0x1 ) { RENDER_LINE_SPR_INCB; }
              else { RENDER_LINE_SPR_GCOLOR; }
              if ( color != 0x10 )
        	{
        	  if ( _render.line_spr[x] != 0x10 )
        	    _status|= CFLAG;
        	  _render.line_spr[x]= color;
        	}
            }
        }
      else
        {
          for ( i= 0; x < 0; ++x, ++i ) { RENDER_LINE_SPR_INCB; }
          for ( ; i < 8 && x < 256; ++i, ++x )
            {
              RENDER_LINE_SPR_GCOLOR;
              RENDER_LINE_SPR_INCB;
              if ( color != 0x10 )
        	{
        	  if ( _render.line_spr[x] != 0x10 )
        	    _status|= CFLAG;
        	  _render.line_spr[x]= /*(n%15)+1*/color;
        	}
            }
        }
      
    }
  
} /* render_line_spr */


static void
render_line (void)
{
  
  int x, color, color_bg, color_spr, aux, i;
  
  
  if ( _regs.BLANK )
    {
      color= ((int) _regs.ob_color|0x10)<<1;
      aux= (_cram[color] | (((int) _cram[color|0x1])<<8))&0xFFF;
      for ( x= 0; x < 160; ++x ) *(_render.p++)= aux;
    }
  else
    {
      render_line_bg ();
      render_line_spr ();
      for ( x= 0, i= 48; x < 160; ++x, ++i )
        {
          color_bg= _render.line_bg[x];
          color_spr= _render.line_spr[i];
          /* NOTA: Els sprites sempre gasten la paleta 1, per tant
             0x10 és el color transparent per als sprites. */
          color=
            (color_spr==0x10 ||
             ((color_bg&0xF)!=0x00 &&  _render.prior_bg[x])) ?
            color_bg : color_spr;
          if ( color == 0 ) color= color_bg;
          color<<= 1;
          *(_render.p++)=
            (_cram[color] | (((int) _cram[color|0x1])<<8))&0xFFF;
        }
    }
  sat_evaluation ( _render.lines );
  ++_render.lines;
  _regs.BLANK= _regs.BLANKl;
  _regs.col= _regs.coll;
  _regs.fx= _regs.fxl;
  
} /* end render_line */


/* LINES pot ser 0. */
static void
render_lines (
              const int lines
              )
{
  
  int i;
  
  
  for ( i= 0; i < lines; ++i )
    render_line ();
  
} /* end render_lines */


static void
init_render (void)
{
  
  _render.p= &(_render.fb[0]);
  _render.lines= 24;
  
} /* end init_render */


static void
init_active_display (void)
{
  
  _regs.row= _regs.row_tmp;
  _regs.fy= _regs.fy_tmp;
  
} /* end init_active_display */


static int
calc_icounts (
              const int Vb,
              const int Hb,
              const int Ve,
              const int He
              )
{
  return Ve - Vb + (He>=COUNTSTOILINE) - (Hb>=COUNTSTOILINE);
} /* end calc_icounts */


/* Com 'calc_icounts' però Ve >= 192. */
static int
calc_icounts_b (
        	const int Vb,
        	const int Hb
        	)
{
  return 191 - Vb + 1 - (Hb>=COUNTSTOILINE);
} /* end calc_icounts_b */


static void
update_irq (void)
{
  
  Z80_IRQ ( ((_regs.IE && (_status&FFLAG)) ||
             (_regs.IE1 && _line_int_pending_flag)),
            0xFF );
  
} /* end update_irq */


static void
run_icounts (
             const int Vb,
             const int Hb,
             const int Ve,
             const int He
             )
{
  
  int icounts;
  
  
  icounts= calc_icounts ( Vb, Hb, Ve, He );
  if ( icounts > _line_int_counter )
    {
      _line_int_pending_flag= Z80_TRUE;
      update_irq ();
      icounts-= (_line_int_counter+1);
      _line_int_counter=
        _regs.line_counter - icounts%(_regs.line_counter+1);
    }
  else _line_int_counter-= icounts;
  
} /* end run_icounts */


static void
run_first_icount (
        	  const int He
        	  )
{
  
  if ( _line_int_counter == 0 )
    {
      _line_int_pending_flag= Z80_TRUE;
      update_irq ();
      _line_int_counter= _regs.line_counter;
    }
  else --_line_int_counter;
  
} /* end run_first_icount */


/* Com run_icounts, però sabent que hem aplegat al tope d'aquesta
   iteració, i no cal ni actualitzar el comptador de línies ni tampoc
   els cicles que falten per a la següent.  IMPORTANT: Si no
   s'actualitza el número de cicles que falten per a la següent
   interrupció ací, per què el número de comptes del registres pot
   variar en qualsevol moment fins al principi de la següent
   iteració. */
static void
run_icounts_end (
        	 const int Vb,
        	 const int Hb
        	 )
{
  
    int icounts;
  
  
  icounts= calc_icounts_b ( Vb, Hb );
  if ( icounts > _line_int_counter )
    {
      _line_int_pending_flag= Z80_TRUE;
      update_irq ();
    }
  
} /* end run_icounts_end */


/* Aquesta funció executa 'sat_evaluation' per a les línies
   [begin,end]. */
static void
run_sat_evaluations (
        	     const int begin,
        	     const int end
        	     )
{
  
  int i;
  
  
  for ( i= begin; i <= end; ++i )
    sat_evaluation ( i );
  
} /* end run_sat_evaluations */

/* Aquesta funció executa 'sat_evaluation' per a les línies no
   visibles del principi. */
static void
run_sat_evaluations_begin (
        		   int       Vb,
        		   const int Hb,
        		   int       Ve,
        		   const int He
        		   )
{
  
  Vb+= (Hb>=COUNTSTORENDERLINE);
  Ve-= (He<COUNTSTORENDERLINE);
  if ( Ve > 23 ) Ve= 23;
  run_sat_evaluations ( Vb, Ve );
  
} /* end run_sat_evaluations_begin */


/* Aquesta funció executa 'sat_evaluation' per a les línies no
   visibles del final. */
static void
run_sat_evaluations_end (
        		 int       Vb,
        		 const int Hb,
        		 int       Ve,
        		 const int He
        		 )
{
  
  Vb+= (Hb>=COUNTSTORENDERLINE);
  Ve-= (He<COUNTSTORENDERLINE);
  if ( Vb < 168 ) Vb= 168;
  if ( Ve > 191 ) Ve= 191;
  run_sat_evaluations ( Vb, Ve );
  
} /* end run_sat_evaluations_end */


static void
run_last_part (
               const int Ve,
               const int He
               )
{
  
  if ( Ve != 262 )
    {
      if ( He >= COUNTSTOILINE ) run_first_icount ( He );
    }
  else
    {
      run_first_icount ( COUNTSPERLINE );
      init_active_display ();
    }
  
} /* end run_last_part */


static void
run_post_update_prev261 (
        		 const int      Ve,
        		 const int      He,
        		 const Z80_Bool last_update_not_done
        		 )
{
  
  if ( Ve < 261 )
    {
      /* Esta actualització sols té rellevància en la línia 260,
         però en les altres línies no molesta. */
      if ( last_update_not_done && He >= COUNTSTOILINE )
        _line_int_counter= _regs.line_counter;
    }
  else
    {
      if ( last_update_not_done ) _line_int_counter= _regs.line_counter;
      run_last_part ( Ve, He );
    }
  
} /* end run_post_update_prev261 */


static void
run_frame_interrupt (
        	     const int Ve,
        	     const int He
        	     )
{
  
   _status|= FFLAG;
   update_irq ();
   
} /* end run_frame_interrupt */


static void
run_post_update_prev192b (
        		  const int Vb,
        		  const int Hb,
        		  const int Ve,
        		  const int He
        		  )
{
  
  if ( Ve < 192 )
    {
      run_icounts ( Vb, Hb, Ve, He );
      if ( He >= COUNTSTOILINE ) run_frame_interrupt ( Ve, He );
    }
  else
    {
      run_frame_interrupt ( Ve, He );
      run_icounts_end ( Vb, Hb );
      run_post_update_prev261 ( Ve, He, Z80_TRUE );
    }
  
} /* end run_post_update_prev192b */


static void
run_post_update_prev192 (
        		 const int Vb,
        		 const int Hb,
        		 const int Ve,
        		 const int He
        		 )
{
  
  run_sat_evaluations_end ( Vb, Hb, Ve, He );
  if ( Ve < 191 ) run_icounts ( Vb, Hb, Ve, He );
  else run_post_update_prev192b ( Vb, Hb, Ve, He );
  
} /* end run_post_update_prev192 */


/* S'executa des de (Vb,Hb) a (Ve,He). Es supossa que són tots valors
 * vàlids.
 *
 * Aquestes són les diferents zones d'interés que vaig a tractar.
 *
 * [0-24[ - Estic en el Active display, però encara no he començat a
 *          renderitzar ninguna línia visible.
 * [24-168[ - Estic en la zona de renderitzat de línies visibles.
 * [168-192[ - Part final de l'Active display.
 * [192-260[ - Part final en la qual no es renderitza res.
 * [260-261] - No es renderitza res, però es fa la primera
 * actualització rellevant del comptador de línia.
 * [261-262[ - Primera línia on es decrementa el comptador de línia.
 * [262] - Inidica inici de nou frame.
 */
static void
run (
     const int Vb,
     const int Hb,
     const int Ve,
     const int He
     )
{
  
  int lines;
  
  
  /* Encara no s'ha iniciat el renderitzat. */
  if ( Vb < 24 )
    {
      run_sat_evaluations_begin ( Vb, Hb, Ve, He );
      if ( Ve < 24 ) run_icounts ( Vb, Hb, Ve, He  );
      else
        {
          init_render ();
          if ( Ve < 168 )
            {
              run_icounts ( Vb, Hb, Ve, He );
              lines= Ve - 24 + (He>=COUNTSTORENDERLINE);
              render_lines ( lines );
            }
          else
            {
              render_lines ( 144 );
              _update_screen ( _render.fb, _udata );
              run_post_update_prev192 ( Vb, Hb, Ve, He );
            }
        }
    }
  
  /* Encara no s'ha acabat de renderitzar la zona visible. */
  else if ( Vb < 168 )
    {
      if ( Ve < 168 )
        {
          run_icounts ( Vb, Hb, Ve, He );
          lines= Ve - Vb + (He>=COUNTSTORENDERLINE) - (Hb>=COUNTSTORENDERLINE);
          render_lines ( lines );
        }
      else
        {
          lines= 167 - Vb + 1 - (Hb>=COUNTSTORENDERLINE);
          render_lines ( lines );
          _update_screen ( _render.fb, _udata );
          run_post_update_prev192 ( Vb, Hb, Ve, He );
        }
    }
  
  /* Ja està tot renderitzar però encara pot passar una interrupció a
     nivell de línia i segur que encara no s'ha produït la interrupció
     de final de frame. */
  else if ( Vb < 191 ) run_post_update_prev192 ( Vb, Hb, Ve, He );
  
  /* Ja està tot renderitzat i pot ser quede encara la interrupció de
     frame i alguna de línia, o pot ser que no quede res. */
  else if ( Vb < 192 )
    {
      if ( Hb >= COUNTSTOILINE ) run_post_update_prev261 ( Ve, He, Z80_TRUE );
      else
        {
          run_sat_evaluations_end ( Vb, Hb, Ve, He );
          run_post_update_prev192b ( Vb, Hb, Ve, He );
        }
    }
  
  /* Ja està tot renderitzat i segur que encara no hem fet l'updateig
     rellevant del comptador. */
  else if ( Vb < 260 ) run_post_update_prev261 ( Ve, He, Z80_TRUE );
  
  /* Ja està tot renderitzat, i pot ser hi hasca que updatejar el
     comptador o pot ser que no. */
  else if ( Vb < 261 )
    run_post_update_prev261 ( Ve, He, Hb < COUNTSTOILINE );
  
  /* Començe en l'última línia. */
  else if ( Vb < 262 )
    {
      if ( Hb < COUNTSTOILINE ) run_last_part ( Ve, He );
      else if ( Ve == 262 ) init_active_display ();
    }
  
  /* Crec que açò mai passa. */
  else init_active_display ();
  
} /* end run */


/* Esta funció assumeix que _timing.V i _timing.H estan
   actualitzats. */
static void
update_cctoLInt (void)
{
  
  int V, H, counts;
  
  
  /* Per no marejar transforme l'espai, de manera que les
     interrupcions es produeixquen al principi de cada línia del nou
     espai i s'incrementen al principi de les línies [0-192]. */
  if ( _timing.H >= COUNTSTOILINE )
    {
      V= _timing.V==261 ? 0 : _timing.V+1;
      H= _timing.H-COUNTSTOILINE;
    }
  else
    {
      V= _timing.V;
      H= COUNTSPERLINE - (COUNTSTOILINE-_timing.H);
    }
  
  /* Si estic en la zona de no interrupció, o estic en la zona de
     interrupció pero no tinc prou comptes, aleshores número de cicles
     necessaris per a realitzar el primer count (aplegar al final del
     tot), més counts restants d'acord al valor del registre. Si no,
     tinc que fer els counts que queden. */
  counts= _line_int_counter+1;
  if ( V >= 192 || (V+counts) > 192 )
    counts= (262-V)+_regs.line_counter;
  
  _timing.cctoLInt= 4 * ((counts*COUNTSPERLINE) - H);
  
} /* end update_cctoLInt */


static void
clock (void)
{
  
  int newH, newV;
  

  /* NOTA:
   * Count = 4/3CC
   * En _timing.cc tinc guardat els 1/3CC.
   * 171Count= 1Línia
   */
  _timing.cctoLInt-= _timing.cc;
  _timing.cctoFInt-= _timing.cc;
  newV= _timing.V + (_timing.cc/(COUNTSPERLINE*4));
  _timing.cc%= COUNTSPERLINE*4;
  newH= _timing.H + (_timing.cc/4);
  if ( newH >= COUNTSPERLINE ) { ++newV; newH-= COUNTSPERLINE; }
  _timing.cc%= 4;
  _timing.cctoLInt+= _timing.cc;
  _timing.cctoFInt+= _timing.cc;
  /* Si els ccto*Int ixen positius vol dir que no s'aplegarà a niguna
     interrupció i que per a la pròxima falten exactament eixos
     cicles. Si ixen negatius hi ha que reinicialitzar-los. */
  while ( newV >= 262 )
    {
      run ( _timing.V, _timing.H, 262, 0 );
      newV-= 262;
      _timing.V= _timing.H= 0;
    }
  run ( _timing.V, _timing.H, newV, newH );
  _timing.V= newV;
  _timing.H= newH;
  if ( _timing.cctoLInt <= 0 ) update_cctoLInt ();
  if ( _timing.cctoFInt <= 0 )
    {
      if ( _timing.V > 191 || (_timing.V == 191 && _timing.H >= COUNTSTOILINE) )
        _timing.cctoFInt=
          4*((262-(_timing.V-191))*COUNTSPERLINE + COUNTSTOILINE - _timing.H);
      else
        _timing.cctoFInt=
          4*((191-_timing.V)*COUNTSPERLINE + COUNTSTOILINE - _timing.H);
    }

} /* end clock */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
GG_vdp_clock (
              const int cc
              )
{
  
  /* Em guarde el número 1/3CC. */
  _timing.cc+= 3*cc;
  if ( (!_line_int_pending_flag &&
        _timing.cc >= _timing.cctoLInt) ||
       _timing.cc >= _timing.cctoFInt  )
    clock ();
  
} /* end GG_vdp_clock */


void
GG_vdp_control (
        	Z80u8 byte
        	)
{
  
  /* NOTA: Molts dubtes. La referència emprada és msvdp-20021112.txt,
     i en menor medida la documentació oficial.  L'exemple de la
     documentació oficial de la pàgina 25/40 no l'entenc. */
  if ( _control_flag )
    {
      _addr|= ((Z80u16)(byte&0x3f))<<8;
      switch ( (_code= byte>>6) )
        {
        case 0: /* Reading from VRAM. */
          _buffer= _vram[_addr];
          INC_ADDR;
          break;
        case 2: /* Writing to VDP registers. */
          clock (); /* Ací es modifica l'estat. */
          switch ( byte&0xf )
            {
            case 0: /* Mode Control No. 1. */
              _regs.MVS= ((_aux_byte&0x80)!=0);
              _regs.IE1= ((_aux_byte&0x10)!=0);
              _regs.EC= ((_aux_byte&0x08)!=0);
              _regs.M4= ((_aux_byte&0x04)!=0);
              _regs.M2= ((_aux_byte&0x02)!=0);
              update_irq ();
              break;
            case 1: /* Mode Control No. 2. */
              _regs.BLANKl= ((_aux_byte&0x40)==0);
              /* Si estic en la zona actual i l'actual línia ja s'ha
        	 renderitzat no cal fer latch, aquest valor serà el
        	 que s'aplicarà a la següent línia si no es canvia. Si
        	 he passat de la zona activa aleshroes sempre
        	 s'actualitza. */
              if ( _timing.H >= COUNTSTORENDERLINE || _timing.V > 191 ||
        	   _timing.V < 24 )
        	_regs.BLANK= _regs.BLANKl;
              _regs.IE= ((_aux_byte&0x20)!=0);
              _regs.M1= ((_aux_byte&0x10)!=0);
              _regs.M3= ((_aux_byte&0x08)!=0);
              _regs.SIZE= ((_aux_byte&0x02)!=0);
              _regs.DSIZE= ((_aux_byte&0x01)!=0);
              update_irq ();
              break;
            case 2: /* Name Table Base Address. */
              _regs.nt_addr= ((Z80u16) (_aux_byte&0xE))<<10;
              break;
            case 3: break;
            case 4: break;
            case 5: /* Sprite Attribute Table Base Address. */
              _regs.sat_addr= ((Z80u16) (_aux_byte&0x7E))<<7;
              break;
            case 6: /* Sprite Pattern Generator Base Address. */
              _regs.spg_addr= ((Z80u16) (_aux_byte&0x4))<<11;
              break;
            case 7: /* Overscan/Backdrop Color. */
              _regs.ob_color= _aux_byte&0xF;
              break;
            case 8: /* Background X Scroll. */
              _regs.coll= (_aux_byte&0xF8)>>3;
              _regs.fxl= _aux_byte&0x7;
              if ( _timing.H >= COUNTSTORENDERLINE || _timing.V > 191 ||
        	   _timing.V < 24 )
        	{
        	  _regs.col= _regs.coll;
        	  _regs.fx= _regs.fxl;
        	}
              break;
            case 9: /* Background Y Scroll. */
              _regs.row_tmp= (_aux_byte&0xF8)>>3;
              _regs.fy_tmp= _aux_byte&0x7;
              break;
            case 10: /* Line counter. */
              _regs.line_counter= _aux_byte;
              update_cctoLInt ();
              break;
            default: break;
            }
          break;
        default: break;
        }
      _control_flag= Z80_FALSE;
    }
  else
    {
      _addr= _aux_byte= byte;
      _control_flag= Z80_TRUE;
    }
  
} /* end GG_vdp_control */


const Z80u8 *
GG_vdp_get_cram (void)
{
  return &(_cram[0]);
} /* end GG_vdp_get_cram */


void
GG_vdp_get_vram (
        	 GG_VRAMState *state
        	 )
{
  
  state->ram= &(_vram[0]);
  state->nt_addr= _regs.nt_addr;
  state->sat_addr= _regs.sat_addr;
  
} /* end GG_vdp_get_vram */


Z80u8
GG_vdp_get_H (void)
{
  return _H;
} /* end GG_vdp_get_H */


Z80u8
GG_vdp_get_status (void)
{
  
  Z80u8 ret;
  
  
  clock ();
  ret= _status;
  _status= 0x00;
  _control_flag= Z80_FALSE;
  _line_int_pending_flag= Z80_FALSE;
  update_irq ();
  
  return ret;
  
} /* end GG_vdp_get_status */


/* DE MOMENT SOLS SOPORTE NTSC, 256X192. */
Z80u8
GG_vdp_get_V (void)
{
  
  Z80u8 aux;
  
  
  clock ();
  aux= (_timing.H>=COUNTSTOILINE) ? _timing.V+1 : _timing.V;
  if ( _timing.V <= 0xDA ) return (Z80u8) aux;
  else                     return (Z80u8) (aux-6);
  
} /* end GG_vdp_get_V */


void
GG_vdp_init (
             GG_UpdateScreen *update_screen,
             void            *udata
             )
{

  GG_vdp_init_state ();
  _update_screen= update_screen;
  _udata= udata;
  
} /* end GG_vdp_init */


void
GG_vdp_init_state (void)
{
  
  memset ( _vram, 0, 16384 );
  memset ( _cram, 0, 64 );
  _status= 0x00;
  _control_flag= Z80_FALSE;
  _addr= 0x0000;
  _code= 0;
  _buffer= 0x00;
  _cram_latch= 0x00;
  
  /* Temporització. */
  _timing.H= _timing.V= _timing.cc= 0;
  _timing.cctoLInt= 4*COUNTSTOILINE;
  /* NOTA!!! Açò pot variar segons el mode. */
  _timing.cctoFInt= 4 * (191*COUNTSPERLINE + COUNTSTOILINE);
  
  _H= 0x00;
  
  _line_int_pending_flag= Z80_FALSE;
  
  /* REGISTRES */
  
  /* R0 */
  _regs.M2= _regs.M4= _regs.EC= _regs.IE1= _regs.MVS= Z80_FALSE;
  
  /* R1 */
  _regs.DSIZE= _regs.SIZE= _regs.M3= _regs.M1= _regs.IE= _regs.BLANK= Z80_FALSE;
  _regs.BLANKl= Z80_FALSE;
  
  /* R2 */
  _regs.nt_addr= 0;
  
  /* R5 */
  _regs.sat_addr= 0;
  
  /* R6 */
  _regs.spg_addr= 0;
  
  /* R7 */
  _regs.ob_color= 0;
  
  /* R8 */
  _regs.col= _regs.fx= 0;
  _regs.coll= _regs.fxl= 0;
  
  /* R9 */
  _regs.row= _regs.fy= _regs.row_tmp= _regs.fy_tmp= 0;
  
  /* R10 */
  _regs.line_counter= 1;
  
  _line_int_counter= _regs.line_counter;
  
  /* Renderitzat. */
  memset ( _render.fb, 0, 23040*sizeof(int) );
  memset ( _render.line_bg, 0, 160*sizeof(int) );
  memset ( _render.prior_bg, 0, 160*sizeof(int) );
  memset ( _render.line_spr, 0, 256*sizeof(int) );
  _render.p= &(_render.fb[0]);
  _render.lines= 24;
  
  /* Sprite buffer. */
  _spr_buffer.N= 0;
  
} /* end GG_vdp_init_state */


Z80u8
GG_vdp_read_data (void)
{
  
  Z80u8 ret;
  
  
  ret= _buffer;
  _buffer= _vram[_addr];
  INC_ADDR;
  _control_flag= Z80_FALSE;
  
  return ret;
  
} /* end GG_vdp_read_data */


void
GG_vdp_set_H (void)
{
  
  clock ();
  /* Implementació basad en la documentació oficial. H és un comptador
     de dots (2 dots == 1 count) de 9 bits, però es tornen sols els 8
     bits superiors. Entenc que es tornen els counts. */
  _H= (Z80u8) _timing.H;
  /* El següent codi està basat en 'msvdp-20021112.txt', però no li
     acabe de trobar el sentit. A més el H counter conta dots, però jo
     estic guardant counts (2 dots). */
  /*
  if ( _timing.H < 136 )      _H= (Z80u8) _timing.H;
  else if ( _timing.H < 141 ) _H= (Z80u8) (_timing.H-1);
  else if ( _timing.H < 240 ) _H= (Z80u8) (_timing.H-2);
  else if ( _timing.H < 242 ) _H= (Z80u8) (_timing.H-3);
  else if ( _timing.H < 250 ) _H= (Z80u8) (_timing.H-4);
  else if ( _timing.H < 255 ) _H= (Z80u8) (_timing.H-5);
  else                        _H= (Z80u8) (_timing.H-6);
  */
  
} /* end GG_vdp_set_H */


void
GG_vdp_write_data (
        	   Z80u8 byte
        	   )
{
  
  Z80u16 addr;
  
  
  clock ();
  _control_flag= Z80_FALSE;
  _buffer= byte;
  if ( _code == 3 )
    {
      if ( _addr&0x1 ) /* Imparell. */
        {
          addr= _addr&0x3f;
          _cram[addr]= byte&0xF;
          _cram[addr-1]= _cram_latch;
        }
      else _cram_latch= byte;
    }
  else _vram[_addr]= byte;
  INC_ADDR;
  
} /* end GG_vdp_write_data */


int
GG_vdp_save_state (
        	   FILE *f
        	   )
{

  int *aux;
  size_t ret;
  

  SAVE ( _vram );
  SAVE ( _cram );
  SAVE ( _status );
  SAVE ( _control_flag );
  SAVE ( _addr );
  SAVE ( _aux_byte );
  SAVE ( _code );
  SAVE ( _buffer );
  SAVE ( _cram_latch );
  SAVE ( _H );
  SAVE ( _line_int_pending_flag );
  SAVE ( _regs );
  SAVE ( _timing );
  SAVE ( _line_int_counter );

  /* En render es fa un tractament especial del punter.  */
  aux= _render.p;
  _render.p= (void *) (_render.p-&(_render.fb[0]));
  ret= fwrite ( &_render, sizeof(_render), 1, f );
  _render.p= aux;
  if ( ret != 1 ) return -1;
  
  SAVE ( _spr_buffer );
  
  return 0;
  
} /* end GG_vdp_save_state */


int
GG_vdp_load_state (
        	   FILE *f
        	   )
{

  int n;

  
  LOAD ( _vram );
  LOAD ( _cram );
  LOAD ( _status );
  LOAD ( _control_flag );
  LOAD ( _addr );
  CHECK ( (_addr&0x3FFF) == _addr );
  LOAD ( _aux_byte );
  LOAD ( _code );
  LOAD ( _buffer );
  LOAD ( _cram_latch );
  LOAD ( _H );
  LOAD ( _line_int_pending_flag );
  LOAD ( _regs );
  CHECK ( (_regs.nt_addr&0x3800) == _regs.nt_addr );
  CHECK ( (_regs.sat_addr&0x3F00) == _regs.sat_addr );
  CHECK ( (_regs.spg_addr&0x2000) == _regs.spg_addr );
  CHECK ( (_regs.ob_color&0xF) == _regs.ob_color );
  CHECK ( (_regs.col&0x1F) == _regs.col );
  CHECK ( (_regs.coll&0x1F) == _regs.coll );
  CHECK ( (_regs.fx&0x7) == _regs.fx );
  CHECK ( (_regs.fxl&0x7) == _regs.fxl );
  CHECK ( (_regs.row&0x1F) == _regs.row );
  CHECK ( (_regs.row_tmp&0x1F) == _regs.row_tmp );
  CHECK ( (_regs.fy&0x7) == _regs.fy );
  CHECK ( (_regs.fy_tmp&0x7) == _regs.fy_tmp );
  CHECK ( _regs.line_counter >= 0 );
  LOAD ( _timing );
  CHECK ( _timing.H < COUNTSPERLINE );
  CHECK ( _timing.V < 262 );
  CHECK ( _timing.cc >= 0 );
  LOAD ( _line_int_counter );
  CHECK ( _line_int_counter >= 0 );
  
  /* En render es fa un tractament especial del punter.  */
  LOAD ( _render );
  _render.p= &(_render.fb[0]) + (ptrdiff_t) _render.p;
  CHECK ( ((&(_render.fb[0])) - _render.p) <= 160*144 );
  CHECK ( (&(_render.fb[0]) + (_render.lines-24)*160) == _render.p );
  for ( n= 0; n < 160*144; ++n )
    if ( _render.fb[n] < 0 || _render.fb[n] > 4095 )
      return -1;
  
  LOAD ( _spr_buffer );
  CHECK ( _spr_buffer.N < NUM_SPRITES );
  for ( n= 0; n < _spr_buffer.N; ++n )
    {
      CHECK ( _spr_buffer.v[n].ind >= 0 && _spr_buffer.v[n].ind < 64 );
      if ( _regs.DSIZE )
        {
          CHECK ( (_spr_buffer.v[n].baddr&0x3FE)==_spr_buffer.v[n].baddr );
        }
      else
        {
          CHECK ( (_spr_buffer.v[n].baddr&0x7FC)==_spr_buffer.v[n].baddr );
        }
    }
  
  return 0;
  
} /* end GG_vdp_load_state */
