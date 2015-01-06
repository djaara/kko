/*
 * Autor:		Jaroslav Bartoň, xbarto42
 * Datum:		2008-4-16
 * Soubor:		gif2bmp.h
 * Komentar:
 */ 

#ifndef __KKO_GIF2BMP_H__
#define __KKO_GIF2BMP_H__

#include <sys/types.h>
#include <stdio.h>

#define GIF2BMPOK 0
#define GIF2BMPFail -1

/* Datovy typ zaznamu o konverzi */
typedef struct{
	/* velikost souboru BMP */
	int64_t bmpSize;
	/* velikost souboru GIF */
	int64_t gifSize;
} tGIF2BMP;

/* Nazev:
 *   gif2bmp
 * Cinnost:
 *   Funkce prevadi soubor formatu GIF na format BMP.
 * Parametry:
 *   gif2bmp - zaznam o prevodu
 *   inputFile - vstupni soubor (GIF)
 *   outputFile - vystupni soubor (BMP)
 * Navratova hodnota:
 *   0 - prevod probehl v poradku
 *   -1 pri prevodu nastala chyba, prip. nedporouje dany format GIF
 */
int gif2bmp(tGIF2BMP *gif2bmp, FILE *inputFile, FILE *outputFile);


#endif

