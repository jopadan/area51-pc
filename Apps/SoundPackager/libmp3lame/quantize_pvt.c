/*
 *	quantize_pvt source file
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id: quantize_pvt.c,v 1.93 2002/09/21 00:31:24 markt Exp $ */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include "util.h"
#include "lame-analysis.h"
#include "tables.h"
#include "reservoir.h"
#include "quantize_pvt.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


#define NSATHSCALE 100 // Assuming dynamic range=96dB, this value should be 92

const char  slen1_tab [16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const char  slen2_tab [16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };


/*
  The following table is used to implement the scalefactor
  partitioning for MPEG2 as described in section
  2.4.3.2 of the IS. The indexing corresponds to the
  way the tables are presented in the IS:

  [table_number][row_in_table][column of nr_of_sfb]
*/
const int  nr_of_sfb_block [6] [3] [4] =
{
  {
    {6, 5, 5, 5},
    {9, 9, 9, 9},
    {6, 9, 9, 9}
  },
  {
    {6, 5, 7, 3},
    {9, 9, 12, 6},
    {6, 9, 12, 6}
  },
  {
    {11, 10, 0, 0},
    {18, 18, 0, 0},
    {15,18,0,0}
  },
  {
    {7, 7, 7, 0},
    {12, 12, 12, 0},
    {6, 15, 12, 0}
  },
  {
    {6, 6, 6, 3},
    {12, 9, 9, 6},
    {6, 12, 9, 6}
  },
  {
    {8, 8, 5, 0},
    {15,12,9,0},
    {6,18,9,0}
  }
};


/* Table B.6: layer3 preemphasis */
const char  pretab [SBMAX_l] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

/*
  Here are MPEG1 Table B.8 and MPEG2 Table B.1
  -- Layer III scalefactor bands. 
  Index into this using a method such as:
    idx  = fr_ps->header->sampling_frequency
           + (fr_ps->header->version * 3)
*/


const scalefac_struct sfBandIndex[9] =
{
  { /* Table B.2.b: 22.05 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,24,32,42,56,74,100,132,174,192}
  },
  { /* Table B.2.c: 24 kHz */                 /* docs: 332. mpg123(broken): 330 */
    {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278, 332, 394,464,540,576},
    {0,4,8,12,18,26,36,48,62,80,104,136,180,192}
  },
  { /* Table B.2.a: 16 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,26,36,48,62,80,104,134,174,192}
  },
  { /* Table B.8.b: 44.1 kHz */
    {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
    {0,4,8,12,16,22,30,40,52,66,84,106,136,192}
  },
  { /* Table B.8.c: 48 kHz */
    {0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
    {0,4,8,12,16,22,28,38,50,64,80,100,126,192}
  },
  { /* Table B.8.a: 32 kHz */
    {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
    {0,4,8,12,16,22,30,42,58,78,104,138,180,192}
  },
  { /* MPEG-2.5 11.025 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0/3,12/3,24/3,36/3,54/3,78/3,108/3,144/3,186/3,240/3,312/3,402/3,522/3,576/3}
  },
  { /* MPEG-2.5 12 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0/3,12/3,24/3,36/3,54/3,78/3,108/3,144/3,186/3,240/3,312/3,402/3,522/3,576/3}
  },
  { /* MPEG-2.5 8 kHz */
    {0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},
    {0/3,24/3,48/3,72/3,108/3,156/3,216/3,288/3,372/3,480/3,486/3,492/3,498/3,576/3}
  }
};



FLOAT8 pow20[Q_MAX];
FLOAT8 ipow20[Q_MAX];
FLOAT8 iipow20[Q_MAX];
FLOAT8 *iipow20_;
FLOAT8 pow43[PRECALC_SIZE];
/* initialized in first call to iteration_init */
FLOAT8 adj43asm[PRECALC_SIZE];
FLOAT8 adj43[PRECALC_SIZE];

/************************************************************************/
/*  initialization for iteration_loop */
/************************************************************************/
void
iteration_init( lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  III_side_info_t * const l3_side = &gfc->l3_side;
  int i;

  if ( gfc->iteration_init_init==0 ) {
    gfc->iteration_init_init=1;

    l3_side->main_data_begin = 0;
    compute_ath(gfp,gfc->ATH->l,gfc->ATH->s);

    pow43[0] = 0.0;
    for(i=1;i<PRECALC_SIZE;i++)
        pow43[i] = pow((FLOAT8)i, 4.0/3.0);

    adj43asm[0] = 0.0;
    for (i = 1; i < PRECALC_SIZE; i++)
      adj43asm[i] = i - 0.5 - pow(0.5 * (pow43[i - 1] + pow43[i]),0.75);
    for (i = 0; i < PRECALC_SIZE-1; i++)
	adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
    adj43[i] = 0.5;
    iipow20_ = &iipow20[210];
    for (i = 0; i < Q_MAX; i++) {
        iipow20[i] = pow(2.0, (double)(i - 210) * 0.1875);
	ipow20[i] = pow(2.0, (double)(i - 210) * -0.1875);
	pow20[i] = pow(2.0, (double)(i - 210) * 0.25);
    }
    huffman_init(gfc);
  }
}





/* 
compute the ATH for each scalefactor band 
cd range:  0..96db

Input:  3.3kHz signal  32767 amplitude  (3.3kHz is where ATH is smallest = -5db)
longblocks:  sfb=12   en0/bw=-11db    max_en0 = 1.3db
shortblocks: sfb=5           -9db              0db

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
longblocks:  amp=1      sfb=12   en0/bw=-103 db      max_en0 = -92db
            amp=32767   sfb=12           -12 db                 -1.4db 

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
shortblocks: amp=1      sfb=5   en0/bw= -99                    -86 
            amp=32767   sfb=5           -9  db                  4db 


MAX energy of largest wave at 3.3kHz = 1db
AVE energy of largest wave at 3.3kHz = -11db
Let's take AVE:  -11db = maximum signal in sfb=12.  
Dynamic range of CD: 96db.  Therefor energy of smallest audible wave 
in sfb=12  = -11  - 96 = -107db = ATH at 3.3kHz.  

ATH formula for this wave: -5db.  To adjust to LAME scaling, we need
ATH = ATH_formula  - 103  (db)
ATH = ATH * 2.5e-10      (ener)

*/

FLOAT8 ATHmdct( lame_global_flags *gfp, FLOAT8 f )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    FLOAT8 ath;
  
    ath = ATHformula( f , gfp );
	  
    if (gfc->nsPsy.use) {
        ath -= NSATHSCALE;
    } else {
        ath -= 114;
    }
    
    /*  modify the MDCT scaling for the ATH  */
    ath -= gfp->ATHlower;

    /* convert to energy */
    ath = pow( 10.0, ath/10.0 );
    return ath;
}
 

void compute_ath( lame_global_flags *gfp, FLOAT8 ATH_l[], FLOAT8 ATH_s[] )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, i, start, end;
    FLOAT8 ATH_f;
    FLOAT8 samp_freq = gfp->out_samplerate;

    for (sfb = 0; sfb < SBMAX_l; sfb++) {
        start = gfc->scalefac_band.l[ sfb ];
        end   = gfc->scalefac_band.l[ sfb+1 ];
        ATH_l[sfb]=FLOAT8_MAX;
        for (i = start ; i < end; i++) {
            FLOAT8 freq = i*samp_freq/(2*576);
            ATH_f = ATHmdct( gfp, freq );  /* freq in kHz */
            ATH_l[sfb] = Min( ATH_l[sfb], ATH_f );
        }
    }

    for (sfb = 0; sfb < SBMAX_s; sfb++){
        start = gfc->scalefac_band.s[ sfb ];
        end   = gfc->scalefac_band.s[ sfb+1 ];
        ATH_s[sfb] = FLOAT8_MAX;
        for (i = start ; i < end; i++) {
            FLOAT8 freq = i*samp_freq/(2*192);
            ATH_f = ATHmdct( gfp, freq );    /* freq in kHz */
            ATH_s[sfb] = Min( ATH_s[sfb], ATH_f );
        }
    }

    /*  no-ATH mode:
     *  reduce ATH to -200 dB
     */
    
    if (gfp->noATH) {
        for (sfb = 0; sfb < SBMAX_l; sfb++) {
            ATH_l[sfb] = 1E-37;
        }
        for (sfb = 0; sfb < SBMAX_s; sfb++) {
            ATH_s[sfb] = 1E-37;
        }
    }
    
    /*  work in progress, don't rely on it too much
     */
    gfc->ATH-> floor = 10. * log10( ATHmdct( gfp, -1. ) );
    
    /*
    {   FLOAT8 g=10000, t=1e30, x;
        for ( f = 100; f < 10000; f++ ) {
            x = ATHmdct( gfp, f );
            if ( t > x ) t = x, g = f;
        }
        printf("min=%g\n", g);
    }*/
}





/************************************************************************
 * allocate bits among 2 channels based on PE
 * mt 6/99
 * bugfixes rh 8/01: often allocated more than the allowed 4095 bits
 ************************************************************************/
int on_pe( lame_global_flags *gfp, FLOAT8 pe[][2], III_side_info_t *l3_side,
           int targ_bits[2], int mean_bits, int gr )
{
    lame_internal_flags * gfc = gfp->internal_flags;
    gr_info *   cod_info;
    int     extra_bits, tbits, bits;
    int     add_bits[2]; 
    int     max_bits;  /* maximum allowed bits for this granule */
    int     ch;

    /* allocate targ_bits for granule */
    ResvMaxBits( gfp, mean_bits, &tbits, &extra_bits );
    max_bits = tbits + extra_bits;
    mean_bits /= gfc->channels_out;
    if (max_bits > MAX_BITS) /* hard limit per granule */
        max_bits = MAX_BITS;
    
    for ( bits = 0, ch = 0; ch < gfc->channels_out; ++ch ) {
        /******************************************************************
         * allocate bits for each channel 
         ******************************************************************/
        cod_info = &l3_side->tt[gr][ch];
    
        targ_bits[ch] = Min( MAX_BITS, tbits/gfc->channels_out );
    
        if (gfc->nsPsy.use) {
            add_bits[ch] = targ_bits[ch] * pe[gr][ch] / 700.0 - targ_bits[ch];
        } 
        else {
            add_bits[ch] = (pe[gr][ch]-750) / 1.4;
            /* short blocks us a little extra, no matter what the pe */
            if ( cod_info->block_type == SHORT_TYPE ) {
	        if (add_bits[ch] < mean_bits/2) 
                    add_bits[ch] = mean_bits/2;
            }
        }

        /* at most increase bits by 1.5*average */
        if (add_bits[ch] > mean_bits*3/2)
            add_bits[ch] = mean_bits*3/2;
        if (add_bits[ch] < 0) 
            add_bits[ch] = 0;

        if (add_bits[ch]+targ_bits[ch] > MAX_BITS) 
	    add_bits[ch] = Max( 0, MAX_BITS-targ_bits[ch] );

        bits += add_bits[ch];
    }
    if (bits > extra_bits) {
        for ( ch = 0; ch < gfc->channels_out; ++ch ) {
            add_bits[ch] = extra_bits * add_bits[ch] / bits;
        }
    }

    for ( ch = 0; ch < gfc->channels_out; ++ch ) {
        targ_bits[ch] += add_bits[ch];
        extra_bits    -= add_bits[ch];
        assert( targ_bits[ch] <= MAX_BITS );
    }
    assert( max_bits <= MAX_BITS );
    return max_bits;
}




void reduce_side(int targ_bits[2],FLOAT8 ms_ener_ratio,int mean_bits,int max_bits)
{
  int move_bits;
  FLOAT fac;


  /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33  
   *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 */
  /* 75/25 split is fac=.5 */
  /* float fac = .50*(.5-ms_ener_ratio[gr])/.5;*/
  fac = .33*(.5-ms_ener_ratio)/.5;
  if (fac<0) fac=0;
  if (fac>.5) fac=.5;
  
    /* number of bits to move from side channel to mid channel */
    /*    move_bits = fac*targ_bits[1];  */
    move_bits = fac*.5*(targ_bits[0]+targ_bits[1]);  

    if (move_bits > MAX_BITS - targ_bits[0]) {
        move_bits = MAX_BITS - targ_bits[0];
    }
    if (move_bits<0) move_bits=0;
    
    if (targ_bits[1] >= 125) {
      /* dont reduce side channel below 125 bits */
      if (targ_bits[1]-move_bits > 125) {

	/* if mid channel already has 2x more than average, dont bother */
	/* mean_bits = bits per granule (for both channels) */
	if (targ_bits[0] < mean_bits)
	  targ_bits[0] += move_bits;
	targ_bits[1] -= move_bits;
      } else {
	targ_bits[0] += targ_bits[1] - 125;
	targ_bits[1] = 125;
      }
    }
    
    move_bits=targ_bits[0]+targ_bits[1];
    if (move_bits > max_bits) {
      targ_bits[0]=(max_bits*targ_bits[0])/move_bits;
      targ_bits[1]=(max_bits*targ_bits[1])/move_bits;
    }
    assert (targ_bits[0] <= MAX_BITS);
    assert (targ_bits[1] <= MAX_BITS);
}


/**
 *  Robert Hegemann 2001-04-27:
 *  this adjusts the ATH, keeping the original noise floor
 *  affects the higher frequencies more than the lower ones
 */

FLOAT8 athAdjust( FLOAT8 a, FLOAT8 x, FLOAT8 athFloor )
{
    /*  work in progress
     */
    FLOAT8 const o = 90.30873362;
    FLOAT8 const p = 94.82444863;
    FLOAT8 u = 10. * log10(x); 
    FLOAT8 v = a*a;
    FLOAT8 w = 0.0;   
    u -= athFloor;                                  // undo scaling
    if ( v > 1E-20 ) w = 1. + 10. * log10(v) / o;
    if ( w < 0  )    w = 0.; 
    u *= w; 
    u += athFloor + o-p;                            // redo scaling

    return pow( 10., 0.1*u );
}


/*************************************************************************/
/*            calc_xmin                                                  */
/*************************************************************************/

/*
  Calculate the allowed distortion for each scalefactor band,
  as determined by the psychoacoustic model.
  xmin(sb) = ratio(sb) * en(sb) / bw(sb)

  returns number of sfb's with energy > ATH
*/
int calc_xmin( 
        lame_global_flags *gfp,
        const III_psy_ratio * const ratio,
	const gr_info       * const cod_info, 
              III_psy_xmin  * const l3_xmin ) 
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, j=0, ath_over=0;
    FLOAT8 xmin, tmpATH;
    ATH_t * ATH = gfc->ATH;
    const FLOAT8 *xr = cod_info->xr;

    for (sfb = 0; sfb < cod_info->psy_lmax; sfb++) {
	FLOAT en0 = 0.0;
	int width, l;
	if ( gfp->VBR == vbr_rh || gfp->VBR == vbr_mtrh )
	    tmpATH = athAdjust( ATH->adjust, ATH->l[sfb], ATH->floor );
	else
	    tmpATH = ATH->adjust * ATH->l[sfb];

	width = gfc->scalefac_band.l[sfb+1] - gfc->scalefac_band.l[sfb];
	l = width;
	do {
	    en0 += xr[j] * xr[j];
	    j++;
	} while (--l > 0);

	/* why is it different from short blocks <?> */
	if ( !gfc->nsPsy.use ) en0 /= width;   

	xmin = tmpATH;
	if (!gfp->ATHonly) {
	    xmin = ratio->en.l[sfb];
	    if (xmin > 0.0)
		xmin = en0 * ratio->thm.l[sfb] * gfc->masking_lower / xmin;
	    if (xmin < tmpATH) 
		xmin = tmpATH;
	}
	/* why is it different from short blocks <?> */
	if ( !gfc->nsPsy.use ) {
	    xmin *= width;
	}
	else {
	    if      (sfb <=  6) xmin *= gfc->nsPsy.bass;
	    else if (sfb <= 13) xmin *= gfc->nsPsy.alto;
	    else if (sfb <= 20) xmin *= gfc->nsPsy.treble;
	    else                xmin *= gfc->nsPsy.sfb21;
	    if ((gfp->VBR == vbr_off || gfp->VBR == vbr_abr) && gfp->quality <= 1)
		xmin *= 0.001;
	}
	l3_xmin->l[sfb] = xmin;
	if (en0 > tmpATH) ath_over++;
    }   /* end of long block loop */

    for (sfb = cod_info->sfb_smin; sfb < cod_info->psy_smax; sfb++) {
	int width, b;
	if ( gfp->VBR == vbr_rh || gfp->VBR == vbr_mtrh )
	    tmpATH = athAdjust( ATH->adjust, ATH->s[sfb], ATH->floor );
	else
	    tmpATH = ATH->adjust * ATH->s[sfb];

	width = gfc->scalefac_band.s[sfb+1] - gfc->scalefac_band.s[sfb];
	for ( b = 0; b < 3; b++ ) {
	    FLOAT en0 = 0.0;
	    int l = width;
	    do {
		en0 += xr[j] * xr[j];
		j++;
	    } while (--l > 0);
	    en0 /= width;

	    xmin = tmpATH;
	    if (!gfp->ATHonly && !gfp->ATHshort) {
		xmin = ratio->en.s[sfb][b];
		if (xmin > 0.0)
		    xmin = en0 * ratio->thm.s[sfb][b] * gfc->masking_lower / xmin;
		if (xmin < tmpATH) 
		    xmin = tmpATH;
	    }
	    xmin *= width;

	    if (gfc->nsPsy.use) {
		if      (sfb <=  5) xmin *= gfc->nsPsy.bass;
		else if (sfb <= 10) xmin *= gfc->nsPsy.alto;
		else                xmin *= gfc->nsPsy.treble;
		if ((gfp->VBR == vbr_off || gfp->VBR == vbr_abr) && gfp->quality <= 1)
		    xmin *= 0.001;
	    }
	    l3_xmin->s[sfb][b] = xmin;
	    if (en0 > tmpATH) ath_over++;
	}   /* b */
	if (gfp->useTemporal) {
	    for ( b = 1; b < 3; b++ ) {
		xmin = l3_xmin->s[sfb][b] * (1.0 - gfc->decay)
		    + l3_xmin->s[sfb][b-1] * gfc->decay;
		if (l3_xmin->s[sfb][b] < xmin)
		    l3_xmin->s[sfb][b] = xmin;
	    }
        }   /* sfb */
    }   /* end of short block loop */

    return ath_over;
}







/*************************************************************************/
/*            calc_noise                                                 */
/*************************************************************************/

// -oo dB  =>  -1.00
// - 6 dB  =>  -0.97
// - 3 dB  =>  -0.80
// - 2 dB  =>  -0.64
// - 1 dB  =>  -0.38
//   0 dB  =>   0.00
// + 1 dB  =>  +0.49
// + 2 dB  =>  +1.06
// + 3 dB  =>  +1.68
// + 6 dB  =>  +3.69
// +10 dB  =>  +6.45

double penalties ( double noise )
{
    return log ( 0.368 + 0.632 * noise * noise * noise );
}

/*  mt 5/99:  Function: Improved calc_noise for a single channel   */

int  calc_noise( 
        const lame_internal_flags           * const gfc,
        const int                       ix [576],
        const gr_info           * const cod_info,
        const III_psy_xmin      * const l3_xmin, 
        const III_scalefac_t    * const scalefac,
              III_psy_xmin      * xfsf,
              calc_noise_result * const res )
{
    int sfb, l, i, over=0;
    FLOAT8 over_noise_db = 0;
    FLOAT8 tot_noise_db  = 0;     /*    0 dB relative to masking */
    FLOAT8 max_noise  = 1E-20; /* -200 dB relative to masking */
    double klemm_noise = 1E-37;
    int j = 0;

    for (sfb = 0; sfb < cod_info->psy_lmax; sfb++) {
	int s =
	    cod_info->global_gain
	    - ((scalefac->l[sfb] + (cod_info->preflag ? pretab[sfb] : 0))
	       << (cod_info->scalefac_scale + 1));
	FLOAT8 step;
	FLOAT8 noise = 0.0;

        if (s<0) {
            step = pow(2.0, (double)(s - 210) * 0.25);
        }else{
            /* use table lookup.  allegedly faster */
            step = POW20(s);
        }

	l = gfc->scalefac_band.l[sfb+1] - gfc->scalefac_band.l[sfb];
	do {
	    FLOAT8 temp = fabs(cod_info->xr[j]) - pow43[ix[j]] * step;
	    noise += temp * temp;
	    j++;
	} while (--l > 0);
	noise = xfsf->l[sfb] = noise / l3_xmin->l[sfb];
	max_noise=Max(max_noise,noise);
	klemm_noise += penalties (noise);

	noise = FAST_LOG10(Max(noise,1E-20));
	/* multiplying here is adding in dB, but can overflow */
	//tot_noise *= Max(noise, 1E-20);
	tot_noise_db += noise;

	if (noise > 0.0) {
	    over++;
	    /* multiplying here is adding in dB -but can overflow */
	    //over_noise *= noise;
	    over_noise_db += noise;
	}
    }

    for (sfb = cod_info->sfb_smin; sfb < cod_info->psy_smax; sfb++) {
	int width = gfc->scalefac_band.s[sfb+1] - gfc->scalefac_band.s[sfb];
	for ( i = 0; i < 3; i++ ) {
	    int s =
		cod_info->global_gain
		- (scalefac->s[sfb][i] << (cod_info->scalefac_scale + 1))
		- cod_info->subblock_gain[i] * 8;
	    FLOAT8 step = POW20(s);
	    FLOAT8 noise = 0.0;
	    l = width;
	    do {
		FLOAT8 temp;
		temp = pow43[ix[j]] * step - fabs(cod_info->xr[j]);
		noise += temp * temp;
		j++;
	    } while (--l > 0);
	    noise = xfsf->s[sfb][i]  = noise / l3_xmin->s[sfb][i];

	    max_noise    = Max(max_noise,noise);
	    klemm_noise += penalties (noise);

	    noise = FAST_LOG10(Max(noise,1E-20));
	    tot_noise_db += noise;

	    if (noise > 0.0) {
		over++;
		over_noise_db += noise;
	    }
	}
    }

    res->over_count = over;
    res->tot_noise   = 10.*tot_noise_db;
    res->over_noise  = 10.*over_noise_db;
    res->max_noise   = 10.*FAST_LOG10(max_noise);
    res->klemm_noise = klemm_noise;

    return over;
}














/************************************************************************
 *
 *  set_pinfo()
 *
 *  updates plotting data    
 *
 *  Mark Taylor 2000-??-??                
 *
 *  Robert Hegemann: moved noise/distortion calc into it
 *
 ************************************************************************/

static
void set_pinfo (
        lame_global_flags *gfp,
              gr_info        * const cod_info,
        const III_psy_ratio  * const ratio, 
        const III_scalefac_t * const scalefac,
        const int                    gr,
        const int                    ch )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int sfb;
    int j,i,l,start,end,bw;
    FLOAT8 en0,en1;
    FLOAT ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;


    III_psy_xmin l3_xmin;
    calc_noise_result noise;
    III_psy_xmin xfsf;

    calc_xmin (gfp, ratio, cod_info, &l3_xmin);
    calc_noise (gfc, cod_info->l3_enc, cod_info,
		&l3_xmin, scalefac, &xfsf, &noise);

    if (cod_info->block_type == SHORT_TYPE) {
        for (j=0, sfb = 0; sfb < SBMAX_s; sfb++ )  {
            start = gfc->scalefac_band.s[ sfb ];
            end   = gfc->scalefac_band.s[ sfb + 1 ];
            bw = end - start;
            for ( i = 0; i < 3; i++ ) {
                for ( en0 = 0.0, l = start; l < end; l++ ) {
                    en0 += cod_info->xr[j] * cod_info->xr[j];
                    ++j;
                }
                en0=Max(en0/bw,1e-20);


#if 0
{
    double tot1,tot2;
    if (sfb<SBMAX_s-1) {
        if (sfb==0) {
            tot1=0;
            tot2=0;
        }
        tot1 += en0;
        tot2 += ratio->en.s[sfb][i];

        DEBUGF("%i %i sfb=%i mdct=%f fft=%f  fft-mdct=%f db \n",
                gr,ch,sfb,
                10*log10(Max(1e-25,ratio->en.s[sfb][i])),
                10*log10(Max(1e-25,en0)),
                10*log10((Max(1e-25,en0)/Max(1e-25,ratio->en.s[sfb][i]))));

        if (sfb==SBMAX_s-2) {
            DEBUGF("%i %i toti %f %f ratio=%f db \n",gr,ch,
                    10*log10(Max(1e-25,tot2)),
                    10*log(Max(1e-25,tot1)),
                    10*log10(Max(1e-25,tot1)/(Max(1e-25,tot2))));

        }
    }
    /*
        masking: multiplied by en0/fft_energy
        average seems to be about -135db.
     */
}
#endif


                /* convert to MDCT units */
                en1=1e15;  /* scaling so it shows up on FFT plot */
                gfc->pinfo->xfsf_s[gr][ch][3*sfb+i] 
                    = en1*xfsf.s[sfb][i]*l3_xmin.s[sfb][i]/bw;
                gfc->pinfo->en_s[gr][ch][3*sfb+i] = en1*en0;

                if (ratio->en.s[sfb][i]>0)
                    en0 = en0/ratio->en.s[sfb][i];
                else
                    en0=0;
                if (gfp->ATHonly || gfp->ATHshort)
                    en0=0;

                gfc->pinfo->thr_s[gr][ch][3*sfb+i] = 
                        en1*Max(en0*ratio->thm.s[sfb][i],gfc->ATH->s[sfb]);

 
                /* there is no scalefactor bands >= SBPSY_s */
                if (sfb < SBPSY_s) {
                    gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]=
                                            -ifqstep*scalefac->s[sfb][i];
                } else {
                    gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]=0;
                }
                gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i] -=
                                             2*cod_info->subblock_gain[i];
            }
        }
    } else {
        for ( sfb = 0; sfb < SBMAX_l; sfb++ )   {
            start = gfc->scalefac_band.l[ sfb ];
            end   = gfc->scalefac_band.l[ sfb+1 ];
            bw = end - start;
            for ( en0 = 0.0, l = start; l < end; l++ ) 
                en0 += cod_info->xr[l] * cod_info->xr[l];
            en0/=bw;
      /*
    DEBUGF("diff  = %f \n",10*log10(Max(ratio[gr][ch].en.l[sfb],1e-20))
                            -(10*log10(en0)+150));
       */

#if 0
 {
    double tot1,tot2;
    if (sfb==0) {
        tot1=0;
        tot2=0;
    }
    tot1 += en0;
    tot2 += ratio->en.l[sfb];


    DEBUGF("%i %i sfb=%i mdct=%f fft=%f  fft-mdct=%f db \n",
            gr,ch,sfb,
            10*log10(Max(1e-25,ratio->en.l[sfb])),
            10*log10(Max(1e-25,en0)),
            10*log10((Max(1e-25,en0)/Max(1e-25,ratio->en.l[sfb]))));

    if (sfb==SBMAX_l-1) {
        DEBUGF("%i %i toti %f %f ratio=%f db \n",
            gr,ch,
            10*log10(Max(1e-25,tot2)),
            10*log(Max(1e-25,tot1)),
            10*log10(Max(1e-25,tot1)/(Max(1e-25,tot2))));
    }
    /*
        masking: multiplied by en0/fft_energy
        average seems to be about -147db.
     */
}
#endif


            /* convert to MDCT units */
            en1=1e15;  /* scaling so it shows up on FFT plot */
            gfc->pinfo->xfsf[gr][ch][sfb] =  en1*xfsf.l[sfb]*l3_xmin.l[sfb]/bw;
            gfc->pinfo->en[gr][ch][sfb] = en1*en0;
            if (ratio->en.l[sfb]>0)
                en0 = en0/ratio->en.l[sfb];
            else
                en0=0;
            if (gfp->ATHonly)
                en0=0;
            gfc->pinfo->thr[gr][ch][sfb] =
                             en1*Max(en0*ratio->thm.l[sfb],gfc->ATH->l[sfb]);

            /* there is no scalefactor bands >= SBPSY_l */
            if (sfb<SBPSY_l) {
                if (scalefac->l[sfb]<0)  /* scfsi! */
                    gfc->pinfo->LAMEsfb[gr][ch][sfb] =
                                            gfc->pinfo->LAMEsfb[0][ch][sfb];
                else
                    gfc->pinfo->LAMEsfb[gr][ch][sfb] = -ifqstep*scalefac->l[sfb];
            }else{
                gfc->pinfo->LAMEsfb[gr][ch][sfb] = 0;
            }

            if (cod_info->preflag && sfb>=11) 
                gfc->pinfo->LAMEsfb[gr][ch][sfb] -= ifqstep*pretab[sfb];
        } /* for sfb */
    } /* block type long */
    gfc->pinfo->LAMEqss     [gr][ch] = cod_info->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = cod_info->part2_3_length;
    gfc->pinfo->LAMEsfbits  [gr][ch] = cod_info->part2_length;

    gfc->pinfo->over      [gr][ch] = noise.over_count;
    gfc->pinfo->max_noise [gr][ch] = noise.max_noise;
    gfc->pinfo->over_noise[gr][ch] = noise.over_noise;
    gfc->pinfo->tot_noise [gr][ch] = noise.tot_noise;
}


/************************************************************************
 *
 *  set_frame_pinfo()
 *
 *  updates plotting data for a whole frame  
 *
 *  Robert Hegemann 2000-10-21                          
 *
 ************************************************************************/

void set_frame_pinfo( 
        lame_global_flags *gfp,
        III_psy_ratio   ratio    [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    unsigned int          sfb;
    int                   ch;
    int                   gr;
    III_scalefac_t        act_scalefac [2];
    int scsfi[2] = {0,0};
    
    
    gfc->masking_lower = 1.0;
    
    /* reconstruct the scalefactors in case SCSFI was used 
     */
    for (ch = 0; ch < gfc->channels_out; ch ++) {
        for (sfb = 0; sfb < SBMAX_l; sfb ++) {
            gr_info *cod_info = &gfc->l3_side.tt[0][ch];
            if (gfc->l3_side.tt[1][ch].scalefac.l[sfb] == -1) {/* scfsi */
                act_scalefac[ch].l[sfb] = cod_info->scalefac.l[sfb];
                scsfi[ch] = 1;
            } else {
                act_scalefac[ch].l[sfb] = gfc->l3_side.tt[1][ch].scalefac.l[sfb];
            }
        }
    }
    
    /* for every granule and channel patch l3_enc and set info
     */
    for (gr = 0; gr < gfc->mode_gr; gr ++) {
        for (ch = 0; ch < gfc->channels_out; ch ++) {
            gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
            
            if (gr == 1 && scsfi[ch]) 
                set_pinfo (gfp, cod_info, &ratio[gr][ch], &act_scalefac[ch],
			   gr, ch);
            else
                set_pinfo (gfp, cod_info, &ratio[gr][ch], &cod_info->scalefac,
			   gr, ch);
        } /* for ch */
    }    /* for gr */
}
        



/*********************************************************************
 * nonlinear quantization of xr 
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be 
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 * ASM optimization from 
 *    Mathew Hendry <scampi@dial.pipex.com> 11/1999
 *    Acy Stapp <AStapp@austin.rr.com> 11/1999
 *    Takehiro Tominaga <tominaga@isoternet.org> 11/1999
 * 9/00: ASM code removed in favor of IEEE754 hack.  If you need
 * the ASM code, check CVS circa Aug 2000.  
 *********************************************************************/


#ifdef TAKEHIRO_IEEE754_HACK

typedef union {
    float f;
    int i;
} fi_union;

#define MAGIC_FLOAT (65536*(128))
#define MAGIC_INT 0x4b000000

void quantize_xrpow(const FLOAT8 *xp, int *pi, FLOAT8 istep)
{
    /* quantize on xr^(3/4) instead of xr */
    int j;
    fi_union *fi;

    fi = (fi_union *)pi;
    for (j = 576 / 4 - 1; j >= 0; --j) {
	double x0 = istep * xp[0];
	double x1 = istep * xp[1];
	double x2 = istep * xp[2];
	double x3 = istep * xp[3];

	x0 += MAGIC_FLOAT; fi[0].f = x0;
	x1 += MAGIC_FLOAT; fi[1].f = x1;
	x2 += MAGIC_FLOAT; fi[2].f = x2;
	x3 += MAGIC_FLOAT; fi[3].f = x3;

	fi[0].f = x0 + (adj43asm - MAGIC_INT)[fi[0].i];
	fi[1].f = x1 + (adj43asm - MAGIC_INT)[fi[1].i];
	fi[2].f = x2 + (adj43asm - MAGIC_INT)[fi[2].i];
	fi[3].f = x3 + (adj43asm - MAGIC_INT)[fi[3].i];

	fi[0].i -= MAGIC_INT;
	fi[1].i -= MAGIC_INT;
	fi[2].i -= MAGIC_INT;
	fi[3].i -= MAGIC_INT;
	fi += 4;
	xp += 4;
    }
}

#  define ROUNDFAC -0.0946
void quantize_xrpow_ISO(const FLOAT8 *xp, int *pi, FLOAT8 istep)
{
    /* quantize on xr^(3/4) instead of xr */
    int j;
    fi_union *fi;

    fi = (fi_union *)pi;
    for (j=576/4 - 1;j>=0;j--) {
	fi[0].f = istep * xp[0] + (ROUNDFAC + MAGIC_FLOAT);
	fi[1].f = istep * xp[1] + (ROUNDFAC + MAGIC_FLOAT);
	fi[2].f = istep * xp[2] + (ROUNDFAC + MAGIC_FLOAT);
	fi[3].f = istep * xp[3] + (ROUNDFAC + MAGIC_FLOAT);

	fi[0].i -= MAGIC_INT;
	fi[1].i -= MAGIC_INT;
	fi[2].i -= MAGIC_INT;
	fi[3].i -= MAGIC_INT;
	fi+=4;
	xp+=4;
    }
}

#else

/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *
 * Note: using floor() or (int) is extermely slow. On machines where
 * the TAKEHIRO_IEEE754_HACK code above does not work, it is worthwile
 * to write some ASM for XRPOW_FTOI().  
 *********************************************************************/
#define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#define QUANTFAC(rx)  adj43[rx]
#define ROUNDFAC 0.4054


void quantize_xrpow(const FLOAT8 *xr, int *ix, FLOAT8 istep) {
    /* quantize on xr^(3/4) instead of xr */
    /* from Wilfried.Behne@t-online.de.  Reported to be 2x faster than 
       the above code (when not using ASM) on PowerPC */
    int j;

    for ( j = 576/8; j > 0; --j) {
	FLOAT8	x1, x2, x3, x4, x5, x6, x7, x8;
	int	rx1, rx2, rx3, rx4, rx5, rx6, rx7, rx8;
	x1 = *xr++ * istep;
	x2 = *xr++ * istep;
	XRPOW_FTOI(x1, rx1);
	x3 = *xr++ * istep;
	XRPOW_FTOI(x2, rx2);
	x4 = *xr++ * istep;
	XRPOW_FTOI(x3, rx3);
	x5 = *xr++ * istep;
	XRPOW_FTOI(x4, rx4);
	x6 = *xr++ * istep;
	XRPOW_FTOI(x5, rx5);
	x7 = *xr++ * istep;
	XRPOW_FTOI(x6, rx6);
	x8 = *xr++ * istep;
	XRPOW_FTOI(x7, rx7);
	x1 += QUANTFAC(rx1);
	XRPOW_FTOI(x8, rx8);
	x2 += QUANTFAC(rx2);
	XRPOW_FTOI(x1,*ix++);
	x3 += QUANTFAC(rx3);
	XRPOW_FTOI(x2,*ix++);
	x4 += QUANTFAC(rx4);
	XRPOW_FTOI(x3,*ix++);
	x5 += QUANTFAC(rx5);
	XRPOW_FTOI(x4,*ix++);
	x6 += QUANTFAC(rx6);
	XRPOW_FTOI(x5,*ix++);
	x7 += QUANTFAC(rx7);
	XRPOW_FTOI(x6,*ix++);
	x8 += QUANTFAC(rx8);
	XRPOW_FTOI(x7,*ix++);
	XRPOW_FTOI(x8,*ix++);
    }
}






void quantize_xrpow_ISO( const FLOAT8 *xr, int *ix, FLOAT8 istep )
{
    /* quantize on xr^(3/4) instead of xr */
    const FLOAT8 compareval0 = (1.0 - 0.4054)/istep;
    int j;
    /* depending on architecture, it may be worth calculating a few more
       compareval's.

       eg.  compareval1 = (2.0 - 0.4054/istep);
       .. and then after the first compare do this ...
       if compareval1>*xr then ix = 1;

       On a pentium166, it's only worth doing the one compare (as done here),
       as the second compare becomes more expensive than just calculating
       the value. Architectures with slow FP operations may want to add some
       more comparevals. try it and send your diffs statistically speaking

       73% of all xr*istep values give ix=0
       16% will give 1
       4%  will give 2
    */
    for (j=576;j>0;j--) {
	if (compareval0 > *xr) {
	    *(ix++) = 0;
	    xr++;
	} else {
	    /*    *(ix++) = (int)( istep*(*(xr++))  + 0.4054); */
	    XRPOW_FTOI(  istep*(*(xr++))  + ROUNDFAC , *(ix++) );
	}
    }
}

#endif
