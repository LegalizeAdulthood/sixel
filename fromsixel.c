#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gd.h>

typedef unsigned char BYTE;

#define PALVAL(n,a,m)	(((n) * (a) + ((m) / 2)) / (m))

#define	XRGB(r,g,b) gdTrueColor(PALVAL(r,gdRedMax, 100), \
			PALVAL(g,gdGreenMax, 100), PALVAL(b,gdBlueMax, 100))

#define	RGBMAX	gdRedMax
#define	HLSMAX	100
#define	PALMAX	1024

char *sixel_param = NULL;
char *sixel_gra   = NULL;
int sixel_palfix = 0;
char sixel_palinit[PALMAX];
int sixel_palet[PALMAX];

static int ColTab[] = {
	XRGB( 0,  0,  0),	//  0 Black
	XRGB(20, 20, 80),	//  1 Blue
	XRGB(80, 13, 13),	//  2 Red
	XRGB(20, 80, 20),	//  3 Green
	XRGB(80, 20, 80),	//  4 Magenta
	XRGB(20, 80, 80),	//  5 Cyan
	XRGB(80, 80, 20),	//  6 Yellow
	XRGB(53, 53, 53), 	//  7 Gray 50%
	XRGB(26, 26, 26), 	//  8 Gray 25%
	XRGB(33, 33, 60), 	//  9 Blue*
	XRGB(60, 26, 26), 	// 10 Red*
	XRGB(33, 60, 33), 	// 11 Green*
	XRGB(60, 33, 60), 	// 12 Magenta*
	XRGB(33, 60, 60),	// 13 Cyan*
	XRGB(60, 60, 33),	// 14 Yellow*
	XRGB(80, 80, 80), 	// 15 Gray 75%
    };

static int HueToRGB(int n1, int n2, int hue)
{
    if ( hue < 0 )
	hue += HLSMAX;

    if (hue > HLSMAX)
	hue -= HLSMAX;

    if ( hue < (HLSMAX / 6) )
         return (n1 + (((n2 - n1) * hue + (HLSMAX / 12)) / (HLSMAX / 6)));
    if ( hue < (HLSMAX / 2))
        return ( n2 );
    if ( hue < ((HLSMAX * 2) / 3))
	return ( n1 + (((n2 - n1) * (((HLSMAX * 2) / 3) - hue) + (HLSMAX / 12))/(HLSMAX / 6)));
    else
	return ( n1 );
}
static int HLStoRGB(int hue, int lum, int sat)
{
    int R, G, B;
    int Magic1, Magic2;

    if ( sat == 0 ) {
        R = G = B = (lum * RGBMAX) / HLSMAX;
    } else {
        if ( lum <= (HLSMAX / 2) )
	    Magic2 = (lum * (HLSMAX + sat) + (HLSMAX / 2)) / HLSMAX;
        else
	    Magic2 = lum + sat - ((lum * sat) + (HLSMAX / 2)) / HLSMAX;
        Magic1 = 2 * lum - Magic2;

	R = (HueToRGB(Magic1, Magic2, hue + (HLSMAX / 3)) * RGBMAX + (HLSMAX / 2)) / HLSMAX;
        G = (HueToRGB(Magic1, Magic2, hue) * RGBMAX + (HLSMAX / 2)) / HLSMAX;
        B = (HueToRGB(Magic1, Magic2, hue - (HLSMAX / 3)) * RGBMAX + (HLSMAX/2)) / HLSMAX;
    }
    return gdTrueColor(R, G, B);
}
static BYTE *GetParam(BYTE *p, int *param, int *len)
{
    int n;

    *len = 0;
    while ( *p != '\0' ) {
        while ( *p == ' ' || *p == '\t' )
            p++;
        if ( isdigit(*p) ) {
            for ( n = 0 ; isdigit(*p) ; p++ )
                n = n * 10 + (*p - '0');
            if ( *len < 10 )
		param[(*len)++] = n;
            while ( *p == ' ' || *p == '\t' )
                p++;
            if ( *p == ';' )
                p++;
        } else if ( *p == ';' ) {
            if ( *len < 10 )
		param[(*len)++] = 0;
            p++;
        } else
            break;
    }
    return p;
}
gdImagePtr gdImageCreateFromSixelPtr(int len, BYTE *p, int bReSize)
{
    int n, i, a, b, c;
    int px, py;
    int mx, my;
    int ax, ay;
    int tx, ty;
    int rep, col, bc, id;
    int param[10];
    gdImagePtr im, dm;
    BYTE *s;
    static char pam[256];
    static char gra[256];

    px = py = 0;
    mx = my = 0;
    ax = 2; ay = 1;
    tx = ty = 0;
    rep = 1;
    col = 0;
    bc = 0;

    if ( (im = gdImageCreateTrueColor(1024, 1024)) == NULL )
	return NULL;
    im->alphaBlendingFlag = 0;

    for ( n = 0 ; n < 16 ; n++ )
	sixel_palet[n] = ColTab[n];

    // colors 16-231 are a 6x6x6 color cube
    for ( a = 0 ; a < 6 ; a++ ) {
	for ( b = 0 ; b < 6 ; b++ ) {
	    for ( c = 0 ; c < 6 ; c++ )
		sixel_palet[n++] = gdTrueColor(a * 51, b * 51, c * 51);
	}
    }
    // colors 232-255 are a grayscale ramp, intentionally leaving out
    for ( a = 0 ; a < 24 ; a++ )
	sixel_palet[n++] = gdTrueColor(a * 11, a * 11, a * 11);

    bc = gdTrueColorAlpha(gdRedMax, gdGreenMax, gdBlueMax, gdAlphaMax);

    for ( ; n < PALMAX ; n++ )
	sixel_palet[n] = gdTrueColor(gdRedMax, gdGreenMax, gdBlueMax);

    gdImageFill(im, 0, 0, bc);

    pam[0] = gra[0] = '\0';
    sixel_param = pam;
    sixel_gra   = gra;

    for ( n = 0 ; n < PALMAX ; n++ )
	sixel_palinit[n] = 0;

    sixel_palfix = 0;

    while ( *p != '\0' ) {
	if ( (p[0] == '\033' && p[1] == 'P') || *p == 0x90 ) {
	    if ( *p == '\033' )
		p++;

	    s = ++p;
            p = GetParam(p, param, &n);
	    if ( s < p ) {
		for ( i = 0 ; i < 255 && s < p ; )
		    pam[i++] = *(s++);
		pam[i] = '\0';
	    }

	    if ( *p == 'q' ) {
		p++;

		if ( n > 0 ) {	// Pn1
		    switch(param[0]) {
		    case 0: case 1: ay = 2; break;
		    case 2: ay = 5; break;
		    case 3: ay = 4; break;
		    case 4: ay = 4; break;
		    case 5: ay = 3; break;
		    case 6: ay = 3; break;
		    case 7: ay = 2; break;
		    case 8: ay = 2; break;
		    case 9: ay = 1; break;
		    }
		}

		if ( n > 2 ) {	// Pn3
		    if ( param[2] == 0 )
			param[2] = 10;
		    ax = ax * param[2] / 10;
		    ay = ay * param[2] / 10;
            	    if ( ax <= 0 ) ax = 1;
            	    if ( ay <= 0 ) ay = 1;
		}
	    }

	} else if ( (p[0] == '\033' && p[1] == '\\') || *p == 0x9C ) {
	    break;

        } else if ( *p == '"' ) { // DECGRA Set Raster Attributes " Pan ; Pad ; Ph ; Pv 
	    s = p++;
            p = GetParam(p, param, &n);
	    if ( s < p ) {
		for ( i = 0 ; i < 255 && s < p ; )
		    gra[i++] = *(s++);
		gra[i] = '\0';
	    }

            if ( n > 0 ) ay = param[0];
            if ( n > 1 ) ax = param[1];
            if ( n > 2 && param[2] > 0 ) tx = param[2];
            if ( n > 3 && param[3] > 0 ) ty = param[3];

            if ( ax <= 0 ) ax = 1;
            if ( ay <= 0 ) ay = 1;

	    if ( gdImageSX(im) < tx || gdImageSY(im) < ty ) {
		if ( (dm = gdImageCreateTrueColor(
			gdImageSX(im) > tx ? gdImageSX(im) : tx, 
			gdImageSY(im) > ty ? gdImageSY(im) : ty)) == NULL )
		    return NULL;
    		dm->alphaBlendingFlag = 0;
		gdImageFill(dm, 0, 0, bc);
		gdImageCopy(dm, im, 0, 0, 0, 0, gdImageSX(im), gdImageSY(im));
		gdImageDestroy(im);
		im = dm;
	    }

        } else if ( *p == '!' ) { // DECGRI Graphics Repeat Introducer ! Pn Ch
            p = GetParam(++p, param, &n);

            if ( n > 0 )
                rep = param[0];

        } else if ( *p == '#' ) {
			// DECGCI Graphics Color Introducer # Pc ; Pu; Px; Py; Pz 
            p = GetParam(++p, param, &n);

            if ( n > 0 ) {
                if ( (col = param[0]) < 0 )
                    col = 0;
                else if ( col >= PALMAX )
                    col = PALMAX - 1;
            }

            if ( n > 4 ) {
                if ( param[1] == 1 ) {            // HLS
                    if ( param[2] > 360 ) param[2] = 360;
                    if ( param[3] > 100 ) param[3] = 100;
                    if ( param[4] > 100 ) param[4] = 100;
                    sixel_palet[col] = HLStoRGB(param[2] * 100 / 360, param[3], param[4]);
		    sixel_palinit[col] |= 2;
                } else if ( param[1] == 2 ) {    // RGB
                    if ( param[2] > 100 ) param[2] = 100;
                    if ( param[3] > 100 ) param[3] = 100;
                    if ( param[4] > 100 ) param[4] = 100;
		    sixel_palet[col] = XRGB(param[2], param[3], param[4]);
		    sixel_palinit[col] |= 2;
		}
            }

        } else if ( *p == '$' ) {        // DECGCR Graphics Carriage Return
            p++;
            px = 0;
            rep = 1;

        } else if ( *p == '-' ) {        // DECGNL Graphics Next Line
            p++;
            px  = 0;
            py += 6;
            rep = 1;

        } else if ( *p >= '?' && *p <= '\x7E' ) {
            if ( gdImageSX(im) < (px + rep) || gdImageSY(im) < (py + 6) ) {
                int nx = gdImageSX(im) * 2;
                int ny = gdImageSY(im) * 2;

                while ( nx < (px + rep) || ny < (py + 6) ) {
                    nx *= 2;
                    ny *= 2;
                }

		if ( (dm = gdImageCreateTrueColor(nx, ny)) == NULL )
		    return NULL;
    		dm->alphaBlendingFlag = 0;
		gdImageFill(dm, 0, 0, bc);
		gdImageCopy(dm, im, 0, 0, 0, 0, gdImageSX(im), gdImageSY(im));
		gdImageDestroy(im);
		im = dm;
            }

            if ( (b = *(p++) - '?') == 0 ) {
                px += rep;

            } else {
		if ( sixel_palinit[col] == 0 )
		    sixel_palfix = 1;
		sixel_palinit[col] |= 1;

                a = 0x01;

		if ( rep <= 1 ) {
                    for ( i = 0 ; i < 6 ; i++ ) {
                        if ( (b & a) != 0 ) {
                            gdImageSetPixel(im, px, py + i, sixel_palet[col]);
                            if ( mx < px )
                                mx = px;
                            if ( my < (py + i) )
                                my = py + i;
                        }
                        a <<= 1;
                    }
                    px += 1;

                } else {
                    for ( i = 0 ; i < 6 ; i++ ) {
                        if ( (b & a) != 0 ) {
                            c = a << 1;
                            for ( n = 1 ; (i + n) < 6 ; n++ ) {
                                if ( (b & c) == 0 )
                                    break;
                                c <<= 1;
                            }
			    gdImageFilledRectangle(im, px, py + i,
			    		px + rep - 1, py + i + n - 1, sixel_palet[col]);
    
                            if ( mx < (px + rep - 1)  )
                                mx = px + rep - 1;
                            if ( my < (py + i + n - 1) )
                                my = py + i + n - 1;
    
                            i += (n - 1);
                            a <<= (n - 1);
                        }
                        a <<= 1;
                    }
                    px += rep;
		}
	    }
            rep = 1;

        } else {
            p++;
        }
    }

    if ( ++mx < tx )
        mx = tx;
    if ( ++my < ty )
        my = ty;

    if ( gdImageSX(im) > mx || gdImageSY(im) > my ) {
	if ( (dm = gdImageCreateTrueColor(mx, my)) == NULL )
	    return NULL;
    	dm->alphaBlendingFlag = 0;
	gdImageCopy(dm, im, 0, 0, 0, 0, gdImageSX(dm), gdImageSY(dm));
	gdImageDestroy(im);
	im = dm;
    }

    gdImageTrueColorToPalette(im, 0, 256);

    if ( bReSize ) {
        sixel_param = NULL;
	sprintf(gra, "\"%d;%d", ay, ax);
    }

    return im;
}
