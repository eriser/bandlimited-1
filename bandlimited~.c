/**
 
 
Apache License 2.0

bandlimited~
    Copyright [2010] Paulo Casaes

      This product includes software developed at
      Github (https://github.com/pcasaes/bandlimited).
 
 -- 
 https://github.com/pcasaes/bandlimited
 mailto:pcasaes@gmail.com
 
 v 0.93
 */

#include "m_pd.h"
#include <math.h>
#include <string.h>
#include "bandlimited_defs.h"
#include "bandlimited_util.h"






static t_class *bandlimited_class;

typedef struct _bandlimited
	{
		t_object x_obj;
		
		//phasor
		double x_phase;
		float x_f;      /* scalar frequency */
		

		
		
		
		//bandlimited
		float s_nq;
		float cutoff;
		unsigned int max_harmonics;
		int approximate;
		t_symbol *type;
		
		
		
		//type
		t_float (*generator)(unsigned int, t_float, t_float);
		
		
		
		
	} t_bandlimited;

#define bandlimited_read(q,w) bandlimited_read4((q),(w))




/*
 * This variables sets the sin function being used. When building 
 * the wave tables the real sin function should be used. When generating
 * a signal the 4point interpolation sin wavetable function should be used.
 *
 * param t_float phase
 *
 * return double evaluation of sin function
 */
static double (*bandlimited_sin)(t_float);

/*
 * This function performs sin(2pi *x) using the real sin function.
 *
 * param t_float phase
 *
 * return double evaluation of sin function
 */
static double bandlimited_sin_real(t_float p) {
	return sin((2.0 * BANDLIMITED_PI)*p);
}


/*
 * This function performs sin(2pi * x) using 4-point interpolation on top of a wavetable.
 *
 * param t_float phase
 *
 * return double evaluation of sin function
 */
static double bandlimited_sin_4point(t_float p) {
	return bandlimited_read(bandlimited_sin_table, p);
}


/*
 * calculates the wavetable position that is nearest to the number of 
 * harmonics specified
 *
 * param unsigned int max harmonics to lookup
 *
 * return unsigned int wavetable position + 1
 */
static inline unsigned int bandlimited_harmpos(unsigned int max_harmonics) {
	
	unsigned int pos =  rint((1.0f*max_harmonics)/BANDLIMITED_INCREMENT);
	if(pos > BANDLIMITED_HAMSIZE)
		pos = BANDLIMITED_HAMSIZE;
	else if(pos == 0)
		pos=1;
	
	return pos;
	
}

/*
 * calculates the wavetable position that is nearest to and below the number of 
 * harmonics specified
 *
 * param unsigned int max harmonics to lookup
 *
 * return unsigned int wavetable position + 1
 */
static inline unsigned int bandlimited_harmposfloor(unsigned int max_harmonics) {
	
	unsigned int pos =  (unsigned int)fmin(floor((1.0f*max_harmonics)/BANDLIMITED_INCREMENT),BANDLIMITED_HAMSIZE);
	if(pos > BANDLIMITED_HAMSIZE)
		pos = BANDLIMITED_HAMSIZE;
	else if(pos ==0 )
		pos=1;
	
	return pos;
	
}



/*
 * This function calculates the harmonic components for a square wave on
 * phase p from start to max_harmonics
 *
 * param unsigned int starting harmonic
 * param unsigned int stopping harmonic
 * param t_float phase
 *
 * return t_float the calculated wave component
 */
static t_float bandlimited_squarepart(unsigned int start, unsigned int max_harmonics, t_float p) {
	unsigned  int i;
	double sum=0.0f;
	
	
	for(i = start; i <= max_harmonics; i += 2) {
		
		sum += bandlimited_sin(p * i )/i;
	}
	
	return  4.0f *sum / BANDLIMITED_PI;
}
							   

/*
 * This function generates a normalized square wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_square(unsigned int max_harmonics, t_float p, t_float dutycycle) {

	unsigned int pos = bandlimited_harmpos(max_harmonics);
	
	unsigned int nearest = (pos) * BANDLIMITED_INCREMENT;
	
	double sum;
	sum = bandlimited_read(bandlimited_square_table[pos-1], p);
	
	
	if(max_harmonics > nearest)
		sum += bandlimited_squarepart(nearest+1, max_harmonics, p);
	else if(max_harmonics < nearest)
		sum -= bandlimited_squarepart( max_harmonics%2 == 0 ? max_harmonics+1 : max_harmonics, nearest-1, p);
	
	return  sum;
	
}

/*
 * This function generates a normalized square wave approximate to the maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_square_aprox(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	
	unsigned int pos = bandlimited_harmposfloor(max_harmonics);
	

	
	t_float sum;
	sum = bandlimited_read(bandlimited_square_table[pos-1], p);
	
	
	return sum;
	
}



/*
 * This function calculates the harmonic components for a triangle wave on
 * phase p from start to max_harmonics
 *
 * param unsigned int starting harmonic
 * param unsigned int stopping harmonic
 * param t_float phase
 *
 * return t_float the calculated wave component
 */
static t_float bandlimited_trianglepart(unsigned int start, unsigned int max_harmonics, t_float p) {
	unsigned int i;
	double sum=0.0f;
	
	
	for(i = start; i <= max_harmonics; i += 2) {
		
		//sum += (powf(-1.0f, (i-1)/2.0f) * bandlimited_sin(p * i))/powf(i, 2.0f) ;
		sum += (bandlimited_sin(p * i)/powf(i, 2.0f)) * (i%4==3 ? -1 : 1 );
		
	}
	
	return  8.0f * sum /BANDLIMITED_PISQ;
}

/*
 * This function generates a normalized triangle wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_triangle(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	unsigned int pos = bandlimited_harmpos(max_harmonics);
	
	unsigned int nearest = (pos) * BANDLIMITED_INCREMENT;
	
	double sum;
	sum = bandlimited_read(bandlimited_triangle_table[pos-1], p);
	
	
	if(max_harmonics > nearest)
		sum += bandlimited_trianglepart  (nearest+1, max_harmonics, p);
	else if(max_harmonics < nearest)
		sum -= bandlimited_trianglepart( max_harmonics%2 == 0 ? max_harmonics+1 : max_harmonics, nearest-1, p);
	
	return sum;	
	
	
}

/*
 * This function generates a normalized triangle wave approximate to the maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_triangle_aprox(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	unsigned int pos = bandlimited_harmposfloor(max_harmonics);
	

	
	double sum;
	sum = bandlimited_read(bandlimited_triangle_table[pos-1], p);
	

	
	return sum;	
	
	
}

/*
 * This function calculates the harmonic components for a sawtooth wave on
 * phase p from start to max_harmonics
 *
 * param unsigned int starting harmonic
 * param unsigned int stopping harmonic
 * param t_float phase
 *
 * return t_float the calculated wave component
 */
static inline t_float bandlimited_sawwavepart(unsigned int start, unsigned int max_harmonics, t_float p) {
	unsigned int i;
	double sum=0.0f;
	
	for(i = start; i <= max_harmonics; i++) {
		
		sum += bandlimited_sin(p * i)/i;
	}
	
	return  2.0f * sum/BANDLIMITED_PI;
}

/*
 * This function generates a non normalized sawtooth wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static inline t_float bandlimited_sawwave(unsigned int max_harmonics, t_float p) {

	unsigned int pos = bandlimited_harmpos(max_harmonics);
	
	unsigned int nearest = (pos) * BANDLIMITED_INCREMENT;
	
	double sum;
	sum = bandlimited_read(bandlimited_sawwave_table[pos-1], p);
	
	
	if(max_harmonics > nearest)
		sum += bandlimited_sawwavepart  (nearest+1, max_harmonics, p);
	else if(max_harmonics < nearest)
		sum -= bandlimited_sawwavepart(max_harmonics, nearest-1, p);
	
	return  sum;	
	
	
}

/*
 * This function generates a normalized saw wave approximate to the maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static inline t_float bandlimited_sawwave_aprox(unsigned int max_harmonics, t_float p) {
	
	unsigned int pos = bandlimited_harmposfloor(max_harmonics);
	

	
	double sum;
	sum = bandlimited_read(bandlimited_sawwave_table[pos-1], p);
	

	
	return  sum;	
	
	
}

/*
 * This function generates a normalized sawtooth wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_saw(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return -1.0f * bandlimited_sawwave(max_harmonics,  p);
}

/*
 * This function generates a normalized sawtooth wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_saw_aprox(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return -1.0f * bandlimited_sawwave_aprox(max_harmonics,  p);
}

/*
 * This function generates a normalized reverse sawtooth wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_rsaw(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return  bandlimited_sawwave(max_harmonics, p);
}

/*
 * This function generates a normalized reverse sawtooth wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_rsaw_aprox(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return  bandlimited_sawwave_aprox(max_harmonics, p);
}


/*
 * This function generates a normalized pulse wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 * param t_float dutycycle
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_pulse(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return (bandlimited_saw(max_harmonics, p, dutycycle) - bandlimited_saw(max_harmonics, p + dutycycle, dutycycle)) -2.0f* (0.5f - dutycycle);
}

/*
 * This function generates a normalized pulse wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 * param t_float dutycycle
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_pulse_aprox(unsigned int max_harmonics, t_float p, t_float dutycycle) {
	return (bandlimited_saw_aprox(max_harmonics, p, dutycycle) - bandlimited_saw_aprox(max_harmonics, p + dutycycle, dutycycle)) -2.0f* (0.5f - dutycycle);
								   
}


/*
 * This function calculates the harmonic components for a sawtooth-triangle wave on
 * phase p from start to max_harmonics
 *
 * param unsigned int starting harmonic
 * param unsigned int stopping harmonic
 * param t_float phase
 *
 * return t_float the calculated wave component
 */
static t_float bandlimited_sawtrianglepart(unsigned int start, unsigned int max_harmonics, t_float p) {
	
	unsigned int i;
	double sumt=0.0f;
	double sums=0.0f;
	double sinc;
	
	
	for(i = start; i <= max_harmonics; i ++) {
		sinc = bandlimited_sin(p * i);
		if(i%2 == 1)
			sumt += (sinc/powf(i, 2.0f)) * (i%4==3 ? -1 : 1 );
		sums += sinc/i;
		
	}
	
	return  2.0f * ((4.0f *  sumt / BANDLIMITED_PI) -  sums ) / BANDLIMITED_PI ;
	
}

/*
 * This function generates a normalized sawtooth-triangle wave with a maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_sawtriangle( unsigned int max_harmonics, t_float p, t_float dutycycle) {

	unsigned int pos = bandlimited_harmpos(max_harmonics);
	
	unsigned int nearest = (pos) * BANDLIMITED_INCREMENT;
	
	t_float sum;
	sum = bandlimited_read(bandlimited_sawtriangle_table[pos-1], p);
	
	
	if(max_harmonics > nearest)
		sum += bandlimited_sawtrianglepart(nearest+1, max_harmonics, p);
	else if(max_harmonics < nearest)
		sum -= bandlimited_sawtrianglepart(max_harmonics, nearest-1, p);
	
	return  sum;	
	
	
}


/*
 * This function generates a normalized saw-triangle wave approximate to the maximum number
 * of harmonics at a certain phase.
 *
 * param unsigned int maxium number of generated harmonics
 * param t_float phase
 *
 * return t_float the calculated wave
 */
static t_float bandlimited_sawtriangle_aprox( unsigned int max_harmonics, t_float p, t_float dutycycle) {
	
	unsigned int pos = bandlimited_harmposfloor(max_harmonics);
	

	
	t_float sum;
	sum = bandlimited_read(bandlimited_sawtriangle_table[pos-1], p);

	
	return  sum;	
	
	
}


/*
 * This function generates the sin(2pi * x) wavetable.
 *
 */
static void bandlimited_dmaketable(void)
{
    int i;
    float *fp, phase, phsinc = (2.0f * BANDLIMITED_PI) / BANDLIMITED_TABSIZE;
    union tabfudge tf;
    
    if (bandlimited_sin_table) return;
    bandlimited_sin_table = (float *)getbytes(sizeof(float) * (BANDLIMITED_TABSIZE+3));
    for (i = BANDLIMITED_TABSIZE+3, fp = (bandlimited_sin_table), phase = -phsinc; i--;
		 fp++, phase += phsinc)
		*fp = sin(phase);
	
	/* here we check at startup whether the byte alignment
	 is as we declared it.  If not, the code has to be
	 recompiled the other way. */
    tf.tf_d = UNITBIT32 + 0.5;
    if ((unsigned)tf.tf_i[LOWOFFSET] != 0x80000000)
        bug("bandlimited~: unexpected machine alignment");
	

}

/*
 * This function is called when the last bandlimited~ object
 * is deleted. It clears up the meory used by all wavetables.
 *
 */
static void bandlimited_delete(t_bandlimited *x) {
	int i;
	
	if(--bandlimited_count == 0l) {
		post("bandlimited~: deleting look up tables");
		freebytes(bandlimited_sin_table, sizeof(float) * (BANDLIMITED_TABSIZE+3));
		bandlimited_sin_table=0;
		
		
		for(i =0; i < BANDLIMITED_HAMSIZE; i ++) {
			freebytes(bandlimited_sawwave_table[i], sizeof(float) * (BANDLIMITED_TABSIZE+3));
			freebytes(bandlimited_triangle_table[i], sizeof(float) * (BANDLIMITED_TABSIZE+3));
			freebytes(bandlimited_square_table[i], sizeof(float) * (BANDLIMITED_TABSIZE+3));
			freebytes(bandlimited_sawtriangle_table[i], sizeof(float) * (BANDLIMITED_TABSIZE+3));
		}	  
		freebytes(bandlimited_sawwave_table, sizeof(float *) * BANDLIMITED_HAMSIZE);
		bandlimited_sawwave_table=0;
		freebytes(bandlimited_triangle_table, sizeof(float *) * BANDLIMITED_HAMSIZE);
		bandlimited_triangle_table=0;
		freebytes(bandlimited_square_table, sizeof(float *) * BANDLIMITED_HAMSIZE);
		bandlimited_square_table=0;
		freebytes(bandlimited_sawtriangle_table, sizeof(float *) * BANDLIMITED_HAMSIZE);
		bandlimited_sawtriangle_table=0;
		
		
	}
}

/*
 * This function generates a waveform wavetable with a certain number
 * of harmonics
 *
 * param float** pointer to all wavetables of a certain waveform
 * param unsigned int position of the wavetable in the first parameter
 *						number of harmonics is this value plus 1 times BANDLIMITED_INCREMENT
 * param t_float *(unsigned int, unsigned int, t_float) pointer to waveform part function
 *
 */
static void bandlimited_dmakewavetable(float **table, unsigned int pos, t_float (*part)(unsigned int, unsigned int, t_float))
{
    int i;
    t_float *fp, phase, phsinc = (1.0f) / (BANDLIMITED_TABSIZE);
    union tabfudge tf;
	unsigned int max_harmonics =  (pos+1) * BANDLIMITED_INCREMENT;
	unsigned int max_harmonics0;
	t_float *previous = pos==0? 0: table[pos-1];

	table[pos] = (float *)getbytes(sizeof(float) * (BANDLIMITED_TABSIZE+3));

	
    
	if(previous) {

		max_harmonics0 = max_harmonics-BANDLIMITED_INCREMENT+1;
		for (i = BANDLIMITED_TABSIZE+3 , fp = (table[pos]), phase = -phsinc; i--;
			 fp++, phase += phsinc,previous++) {
			*fp = part(max_harmonics0,max_harmonics, phase) + *previous;
		}
	} else {
		for (i = BANDLIMITED_TABSIZE+3 , fp = (table[pos]), phase = -phsinc; i--;
			 fp++, phase += phsinc)
			*fp = part(1,max_harmonics, phase);
	}
	
	/* here we check at startup whether the byte alignment
	 is as we declared it.  If not, the code has to be
	 recompiled the other way. */
    tf.tf_d = UNITBIT32 + 0.5;
    if ((unsigned)tf.tf_i[LOWOFFSET] != 0x80000000)
        bug("bandlimited~: unexpected machine alignment");
	
	


	
}

/*
 * This function sets the waveform type.
 *
 * param t_bandlimited* pointer to the bandlimited~ object
 * param t_symbol * symbol of the waveform: square, triangle, saw, rsaw, sawtriangle
 *
 * return int 0 on sucess, 1 on failute (invalid type)
 */
static inline int bandlimited_typeset(t_bandlimited *x, t_symbol *type) {
	x->type=type;
	if(strcmp(GETSTRING(type), "saw") == 0) {
		x->generator=  x->approximate ? &bandlimited_saw_aprox : &bandlimited_saw;
	} else if(strcmp(GETSTRING(type), "rsaw") == 0) {
		x->generator=  x->approximate ? &bandlimited_rsaw_aprox : &bandlimited_rsaw;
	} else if(strcmp(GETSTRING(type), "square") == 0) {
		x->generator=  x->approximate ? &bandlimited_square_aprox : &bandlimited_square;
	} else if(strcmp(GETSTRING(type), "triangle") == 0) {
		x->generator=  x->approximate ? &bandlimited_triangle_aprox : &bandlimited_triangle;
	} else if(strcmp(GETSTRING(type), "sawtriangle") == 0) {
		x->generator=  x->approximate ? &bandlimited_sawtriangle_aprox : &bandlimited_sawtriangle;
	} else if(strcmp(GETSTRING(type), "pulse") == 0) {
		x->generator=  x->approximate ? &bandlimited_pulse_aprox : &bandlimited_pulse;
	} else {
		goto type_unknown;
		
	}
	return 0;
type_unknown:
	return 1;
}

static void bandlimited_dmakealltables(void) {
	unsigned int i;
	
	post("bandlimited~: creating look up tables");
	bandlimited_sin = &bandlimited_sin_real;
   	bandlimited_dmaketable();
	
   	bandlimited_sawwave_table = (float **)getbytes(sizeof(float *) * BANDLIMITED_HAMSIZE);
   	bandlimited_triangle_table = (float **)getbytes(sizeof(float *) * BANDLIMITED_HAMSIZE);
   	bandlimited_square_table = (float **)getbytes(sizeof(float *) * BANDLIMITED_HAMSIZE);
   	bandlimited_sawtriangle_table = (float **)getbytes(sizeof(float *) * BANDLIMITED_HAMSIZE);

							   
   	for(i =0; i < BANDLIMITED_HAMSIZE; i ++) {
		bandlimited_dmakewavetable(bandlimited_sawwave_table,i, bandlimited_sawwavepart);

		bandlimited_dmakewavetable(bandlimited_triangle_table,i,bandlimited_trianglepart);

		bandlimited_dmakewavetable(bandlimited_square_table,i, bandlimited_squarepart);

		bandlimited_dmakewavetable(bandlimited_sawtriangle_table,i, bandlimited_sawtrianglepart);

	}
	bandlimited_sin = &bandlimited_sin_4point;
	
}


static void *bandlimited_new( t_symbol *s, int argc, t_atom *argv) {
	t_bandlimited *x;// = (t_bandlimited *)pd_new(bandlimited_class);
	t_float  f;
	t_symbol *type;
	t_float  max_harmonics;
	t_float	cutoff;
	int approximate;
	
	if(argc == 0) {
		error("bandlimited~: missing first argument: type (saw, rsaw, square, triangle, pulse)");
		goto new_error;
	} else if(!ISSYMBOL(argv[0])) {
		error("bandlimited~: first argument must be a symbol: type (saw, rsaw, square, triangle, pulse)");
		goto new_error;
	} 
	type = atom_getsymbol(&argv[0]);
	
	if(argc > 1) {
		if(!ISFLOAT(argv[1])) {
			error("bandlimited~: second argument must be a float: initial frequency");
			goto new_error;
		}
		f = atom_getfloat(&argv[1]);
	} else {
		f=0;
	}
	if(argc > 2) {
		if(!ISFLOAT(argv[2])) {
			error("bandlimited~: third argument must be a float: max number of generated harmonics (default %d)", BANDLIMITED_MAXHARMONICS);
			goto new_error;
		}
		max_harmonics = atom_getfloat(&argv[2]);
		if(max_harmonics <= 0)
			max_harmonics = BANDLIMITED_MAXHARMONICS;
	} else {
		max_harmonics=BANDLIMITED_MAXHARMONICS;
	}
	if(argc > 3) {
		if(!ISFLOAT(argv[3])) {
			error("bandlimited~: third argument must be a float: cutoff frequency (default nyquist limit)");
			goto new_error;
		}
		cutoff = atom_getfloat(&argv[3]);
	} else {
		cutoff=0;
	}
	if(argc > 4) {
		if(!ISFLOAT(argv[4])) {
			error("bandlimited~: forth argument must be a float: approximate waveform (default 0, off)");
			goto new_error;
		}
		approximate = atom_getfloat(&argv[3]);
	} else {
		approximate=0;
	}
	
	
    x = (t_bandlimited *)pd_new(bandlimited_class);
    
    if(bandlimited_count++ == 0l) {
		bandlimited_dmakealltables();
		
    }
    
	x->cutoff=cutoff;
    x->x_f = f;
	x->s_nq=0;
	x->max_harmonics=max_harmonics;
	x->approximate=approximate;
	if(bandlimited_typeset(x, type) == 1) {
		error("bandlimited~: Uknown type %s, using saw", GETSTRING(type));
		x->generator=  &bandlimited_saw;
	}
    x->x_phase = 0;

	
	
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ft1"));
	
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal,  &s_signal);
	
    outlet_new(&x->x_obj, gensym("signal"));
	
    return (x);
	
new_error:
	return 0;
}

static void bandlimited_ft1(t_bandlimited *x, t_float f)
{
    x->x_phase =   f;
}

static void bandlimited_cutoff(t_bandlimited *x, t_float f)
{
	if(x->s_nq != 0 && f > x->s_nq) 
		error("bandlimited~: %f is greater than the nyquist limit %f, ignoring", f, x->s_nq);
	else if(f < 1 )
		x->cutoff = x->s_nq-1;
	else 
		x->cutoff = f;
}



static void bandlimited_max(t_bandlimited *x, t_float f)
{
	int val = (int)f;
	if(val < 1) {
		val = BANDLIMITED_MAXHARMONICS;
	}
	else if(val > BANDLIMITED_MAXHARMONICS) 
		post("bandlimited~: maximum number of harmonics %d might be too high. you are warned", val);
	x->max_harmonics = val;
	
}

static void bandlimited_approximate(t_bandlimited *x, t_float f)
{
	int last = x->approximate;
	x->approximate = f? 1: 0;
	if(x->approximate != last)
		bandlimited_typeset(x,x->type); 

	
}

#ifdef DEBUG
static void bandlimited_testsine(t_bandlimited *x, t_float f) {
	post("bandlimited~: linear sin(2pi %f) = %f", f, bandlimited_sin_lin(f));
	post("bandlimited~: 4point sin(2pi %f) = %f", f, bandlimited_sin(f));
	post("bandlimited~:   real sin(2pi %f) = %f",  f, sin(2.0*BANDLIMITED_PI*f));
}

static void bandlimited_print(t_bandlimited *x, t_float freq) {
	unsigned int max_harmonics = (unsigned int)( x->cutoff / freq);
	unsigned int pos;
	unsigned int nearest;
	unsigned int i;
	t_float (*generator)(unsigned int, unsigned int, t_float)=0;
	

	
	if(max_harmonics > x->max_harmonics)
		max_harmonics = x->max_harmonics;
	
	pos = bandlimited_harmpos(max_harmonics);
	nearest = pos-- * BANDLIMITED_INCREMENT;
	post("bandlimited~: nearest harmonics is %d of %d", nearest,max_harmonics);
	if(x->generator == &bandlimited_saw || x->generator == &bandlimited_rsaw) {
		generator = &bandlimited_sawwavepart;
	} else if(x->generator == &bandlimited_square) {
		generator = &bandlimited_squarepart;
	} else if(x->generator == &bandlimited_triangle) {
		generator = &bandlimited_trianglepart;
	} else if(x->generator == &bandlimited_sawtriangle) {
		generator = &bandlimited_sawtrianglepart;
	} else if(x->generator == &bandlimited_pulse) {
		generator = &bandlimited_sawwavepart;
	}
	
	if(generator) {
		for(i = 1; i <= max_harmonics; i++) {
			post("bandlimited~: %d\t%f", i, generator(i, i, 0.25f));
		}
	}

	
}
#endif


/*
 * This function is the set method.
 *
 * param t_bandlimited* pointer to the bandlimited~ object
 * param t_symbol * symbol of the waveform: square, triangle, saw, rsaw, sawtriangle
 *
 */
static void bandlimited_type(t_bandlimited *x, t_symbol *type)
{
	if(bandlimited_typeset(x, type) == 1) {
		error("bandlimited~: Uknown type %s, leaving as is", GETSTRING(type));
	}
}



/*
 * This function implements the signal loop;
 *
 * param t_bant_int* array with parameters added on dsp call
 *
 * return t_int* pointer to next position
 */
static t_int *bandlimited_perform(t_int *w) {
    t_bandlimited *x = (t_bandlimited *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *dutycycle = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    int n = (int)(w[5]);
	t_signal *sp = (t_signal *)(w[6]);
	t_float p;
    double dphase = x->x_phase + UNITBIT32;
    union tabfudge tf;
    int normhipart;
	unsigned int max_harmonics;
	t_float cutoff;
	float conv;
	
	conv = 1.0f/sp->s_sr;
	x->s_nq = sp->s_sr / 2.0f - 1;
	cutoff = x->cutoff == 0? x->s_nq : x->cutoff;



	

	tf.tf_d = UNITBIT32;
    normhipart = tf.tf_i[HIOFFSET];
    tf.tf_d = dphase;
	
    while (n--)
    {
		
		if( *in > 0.0) {
			tf.tf_i[HIOFFSET] = normhipart;
			dphase += *in * conv;
			p = tf.tf_d - UNITBIT32;
			tf.tf_d = dphase;
			
			max_harmonics = (int)fmin((int)( cutoff / *in++), x->max_harmonics);
			//max_harmonics = (int)( x->cutoff / *in++);
			//if(max_harmonics > x->max_harmonics)
			//	max_harmonics = x->max_harmonics;
		

			*out++ =  x->generator(max_harmonics, p, *dutycycle++);
		} else {
			*out++ = 0.0f;
			in++;
			dutycycle++;
			tf.tf_d=0.0;
		}
    }
    tf.tf_i[HIOFFSET] = normhipart;
    x->x_phase = tf.tf_d - UNITBIT32;	
	
    return (w+7);	
}




/*
 * This function turns DSP on
 *
 */
static void bandlimited_dsp(t_bandlimited *x, t_signal **sp)
{

    
	dsp_add(bandlimited_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n, sp[0]);
}



/*
 * Setup function
 *
 */
extern void bandlimited_tilde_setup(void)
{
    bandlimited_class = class_new(gensym("bandlimited~"), (t_newmethod)bandlimited_new, (t_method) bandlimited_delete,
								  sizeof(t_bandlimited), 0, A_GIMME, 0); //A_DEFSYMBOL, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT
    CLASS_MAINSIGNALIN(bandlimited_class, t_bandlimited, x_f);
    class_addmethod(bandlimited_class, (t_method)bandlimited_dsp, gensym("dsp"), 0);
    class_addmethod(bandlimited_class, (t_method)bandlimited_ft1,
					gensym("ft1"), A_FLOAT, 0);	
    class_addmethod(bandlimited_class, (t_method)bandlimited_type,
					gensym("type"), A_SYMBOL, 0);	
    class_addmethod(bandlimited_class, (t_method)bandlimited_cutoff,
					gensym("cutoff"), A_FLOAT, 0);		
    class_addmethod(bandlimited_class, (t_method)bandlimited_max,
					gensym("max"), A_FLOAT, 0);		
    class_addmethod(bandlimited_class, (t_method)bandlimited_approximate,
					gensym("approximate"), A_FLOAT, 0);		
	
    debug(class_addmethod(bandlimited_class, (t_method)bandlimited_testsine,
					gensym("testsine"), A_FLOAT, 0);)		
    debug(class_addmethod(bandlimited_class, (t_method)bandlimited_print,
					gensym("print"), A_FLOAT, 0);)		
	
	
	post("bandlimited~: band limited signal generator. Using %d as the default maximum harmonics (to redefine compile with -DBANDLIMITED_MAXHARMONICS=x flag).", BANDLIMITED_MAXHARMONICS);
	
	
}
