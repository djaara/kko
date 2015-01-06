/*
 * Autor:		Jaroslav Bartoň, xbarto42
 * Datum:		2008-4-16
 * Soubor:		gif2bmp.c
 * Komentar:
 */ 

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "gif2bmp.h"

#define MAX_BITS 12
#define MAX_DICT_SIZE 1 << MAX_BITS

/** hlavička GIF souboru */
struct gifHeader {
	u_int8_t id[6];	/** verze GIF */
	int16_t width;			/** šířka */
	int16_t height;			/** výška */
	u_int8_t bits;		/** bitovost */
	u_int8_t bgColor;	/** výchozí barva pozadí */
	u_int8_t reserved;
}__attribute__((__packed__));

/** hlavička bloku obrázku */
struct imgHeader {
	int16_t col0;			/** levý horní roh bloku */
	int16_t row0;			/** levý horní řádek bloku */
	int16_t width;			/** šířka */
	int16_t height;			/** výška */
	u_int8_t flags;	/** vlastnosti palety bloku */
}__attribute__((__packed__));

/** globální paleta obrázku */
struct globalPaletteInfo {
	u_int8_t global;	/** je obsažena? */
	u_int8_t bpp;		/** bitovost */
	u_int8_t sorted;	/** setříděná paleta? */
	u_int8_t length;	/** délka */
};

/** lokální paleta obrázku */
struct localPaletteInfo {
	u_int8_t local;	/** je obsažena? */
	u_int8_t interlaced;/** prokládaný? */
	u_int8_t sorted;	/** setříděná paleta? */
	u_int8_t reserved;	/** rezervováno */
	u_int8_t length;	/** délka */
};

/** struktura s popisem barvy v GIFu/BMP */
struct qrgb {
	u_int8_t r;		/** červená */
	u_int8_t g;		/** zelená */
	u_int8_t b;		/** modrá */
	u_int8_t res;		/** GIF nevyužívá, BMP musí mít nastaveno na 0 */
}__attribute__((__packed__));
#define COLOR_SIZE 3*1	/** je potřeba kvůli velikosti barvy GIF vs BMP */

/** informační hlavička BMP */
struct bmpInfoHeader {
	int32_t biSize;			/** velikost této hlavičky */
	int32_t biWidth;		/** šířka */
	int32_t biHeight;		/** výška */
	int16_t biPlanes;		/** úrovní */
	int16_t biBitCount;		/** bitů na pixel */
	int32_t biCompression;	/** komprese? */
	int32_t biSizeImage;	/** velikost obrázku */
	int32_t biXPelsPerMeter;/** pixelů na metr v osách */
	int32_t biYPelsPerMeter;
	int32_t biClrUsed;		/** použitých barev */
	int32_t biClrImportant;	/** důležitých barev */
}__attribute__((__packed__));

/** položka slovníku */
struct dictionaryItem {
	int16_t	length;			/** délka řetězce aktuální položky */
	u_int8_t* string;	/** řetězec */
};

/** struktura s informacemi dekodéru */
struct decoderInfo {
	u_int8_t initCWlen;	/** počáteční počet bitů */
	u_int8_t CWlen;		/** aktuální počet bitů */
	int16_t		  CC;			/** odpovídající clear code */
	int16_t		  EOI;			/** odpovídající End Of Input */
	int16_t		  max;			/** odpovídající maximální hodnota */
	int16_t		  last;			/** poslední načtený kódový znak */
	int16_t		  next;			/** index následujícího kódového znaku */
	tGIF2BMP	  *gif2png;		/** ukazatel na počítadlo */
	u_int8_t* image;		/** ukazatel na data obrázku */
	int32_t		  imIndex;		/** index v datech obrázku */
	struct dictionaryItem dict[1<<MAX_DICT_SIZE];	/** ukazatel na slovník */
};

#ifdef DEBUG
#define PRINT_DEBUG(s)	fprintf(stderr, s);
#else
#define PRINT_DEBUG(s)
#endif

#define EXTENSION_BLOCK_BEGIN_MARKER 0x21
#define IMAGE_BLOCK_BEGIN_MARKER 0x2c
#define GRAPICS_EXTENSION_BLOCK 0xf9
#define PLAIN_TEXT_EXTENSION_BLOCK 0x01
#define APPLICATION_EXTENSION_BLOCK 0xff
#define BLOCK_TERMINATOR 0x00

/**
 * vrací informace o globální paletě
 * @param bits načtení informace o paletě
 * @param p struktura pro uložení informací o globální paletě
 */
void getGlobalPaletteInfo(u_int8_t bits, struct globalPaletteInfo* p) {
	p->length = (bits & 0x07);
	p->sorted = (bits & 0x08) >> 3;
	p->bpp 	  = ((bits & 0x70) >> 4);
	p->global = bits >> 7;
}

/**
 * vrací informace o lokální paletě
 * @param bits načtené informace o paletě
 * @param p struktura pro uložení informací o lokální paletě
 */
void getLocalPaletteInfo(u_int8_t bits, struct localPaletteInfo *p) {
	p->length     = (bits & 0x07);
	p->reserved   = (bits & 0x18) >> 3;
	p->sorted     = (bits & 0x20) >> 5;
	p->interlaced = (bits & 0x40) >> 6;
	p->local      = (bits & 0x80) >> 7;
	
	#ifdef DEBUG
		fprintf(stderr, "interlaced: %d\n", p->interlaced);
	#endif
}

/**
 * Zápis hlavičky BMP souboru
 * @param outputFile soubor do kterého se provádí zápis
 * @param gifh hlavička s informacemi o původním GIF souboru
 * @param gpi hlavička s informacemi z globální palety
 * @param palette barevná paleta
 */
int8_t writeBmpHeader(FILE* outputFile, struct gifHeader* gifh,
				struct globalPaletteInfo* gpi, struct qrgb* palette, 
				tGIF2BMP* gif2bmp) {
					
	u_int8_t bm[] = "BM";
	int16_t res0_1 = 0;
	
	//POUZE pro 256 barevný GIF!!!!
	int32_t offset = 1078;

	/** naplnění struktury s hlavičkou BMP souboru */
	struct bmpInfoHeader bmpi = {sizeof(struct bmpInfoHeader),	/** velikost hlavičky */ 
			gifh->width,		/** šířka obrázku */
			gifh->height,		/** výška obrázku */
			1,					/** vždy 1, počet bitových rovin */
			gpi->bpp+1,			/** barevná hloubka */
			0, 					/** komprese, není použita */
			0, 					/** velikost obrázku, lze nula pokud není komprese */
			72, 				/** dpi */
			72, 				/** dpi */
			1 << (gpi->bpp+1),	/** barev v paletě */
			0};					/** použitých barev */
			
	/** počet pixelů */
	int32_t pixels = gifh->width * gifh->height +
				(((((gifh->width-1)/4)+1)*4) - gifh->width) * gifh->height;
	
	/** velikost souboru */
	int32_t size = pixels + offset;
	
	/** zápis hlavičky souboru BMP */
	fwrite(bm, 2, 1, outputFile);
	gif2bmp->bmpSize += 2;
	fwrite(&size, sizeof(size), 1, outputFile);
	gif2bmp->bmpSize += sizeof(size);
	fwrite(&res0_1, sizeof(res0_1), 1, outputFile);
	gif2bmp->bmpSize += sizeof(res0_1);
	fwrite(&res0_1, sizeof(res0_1), 1, outputFile);
	gif2bmp->bmpSize += sizeof(res0_1);
	fwrite(&offset, sizeof(offset), 1, outputFile);
	gif2bmp->bmpSize += sizeof(offset);
	
	/** zápis informační hlavičky */
	fwrite(&bmpi, sizeof(bmpi), 1, outputFile);
	gif2bmp->bmpSize += sizeof(bmpi);
	
	/** prohození červené a modré barevné složky */
	u_int8_t tmp;
	for (int16_t i = 0; i < (1 << (gpi->bpp+1)); i++) {
		tmp = palette[i].r;
		palette[i].r = palette[i].b;
		palette[i].b = tmp;
		
	}
	fwrite(&palette[0], sizeof(palette[0]) * (1<<(gpi->bpp+1)), 1, outputFile);
	gif2bmp->bmpSize += sizeof(palette[0]) * (1<<(gpi->bpp+1));
	return(0);
}

/**
 * zapsání BMP dat
 * @param outputfile výstupní soubor
 * @param gifh hlavička GIF souboru
 * @param gpi informace o globální paletě
 * @param palette barevná paleta
 * @param data data k zapsání do BMP souboru
 * @param interlaced informace o tom zda je obrázek prokládaný nebo ne
 * @param gif2bmp počítadlo přečtených/zapsaných byte
 */
int8_t writeBmpData(FILE* outputFile, struct gifHeader* gifh, struct
	globalPaletteInfo* gpi, struct qrgb* palette, u_int8_t* data, 
	u_int8_t interlaced, tGIF2BMP* gif2bmp) {
		
	int32_t padlen = ((((gifh->width-1)/4)+1)*4)- gifh->width;
	u_int8_t padding[] = {0, 0, 0};
	
	/** zápis hlavičky BMP souboru */
	if (writeBmpHeader(outputFile, gifh, gpi, palette, gif2bmp) == GIF2BMPFail) {
		return(GIF2BMPFail);
	}
	
	/** pokud je obrázek prokládaný */
	if (interlaced) { /** vytvoříme nový obrázek se správným uspořádáním řádků */
		u_int8_t* new_data = malloc(sizeof(u_int8_t) * 
												gifh->width * gifh->height);
		int32_t j = 0;
		for (int32_t i = 0; i < gifh->height; i += 8, j++) {
			memcpy(&(new_data[i*gifh->width]), &(data[j*gifh->width]), gifh->width);
		}
		for (int32_t i = 4; i < gifh->height; i += 8, j++) {
			memcpy(&(new_data[i*gifh->width]), &(data[j*gifh->width]), gifh->width);
		}
		for (int32_t i = 2; i < gifh->height; i += 4, j++) {
			memcpy(&(new_data[i*gifh->width]), &(data[j*gifh->width]), gifh->width);
		}
		for (int32_t i = 1; i < gifh->height; i += 2, j++) {
			memcpy(&(new_data[i*gifh->width]), &(data[j*gifh->width]), gifh->width);
		}
		free(data);			/** uvolníme prokládaný obrázek */
		data = new_data;	/** a nastavíme ukazatel */
	}
	
	for (int32_t row = gifh->height; row > 0; row--) { /** zápis */
		/** zapisuj řádky od konce bufferru, tím převrátíš obrázek podle osy y */
		if (fwrite(&data[(row-1) * gifh->width], sizeof(u_int8_t), 
				gifh->width, outputFile) != gifh->width) {
			return(GIF2BMPFail);
		}
		gif2bmp->bmpSize += gifh->width;
		
		/** zapiš případnou výplň (šířka obrázku není násobek 4)*/
		if (padlen) {
			if (fwrite(padding, sizeof(u_int8_t), padlen, outputFile) == 0) {
				return(GIF2BMPFail);
			}
			gif2bmp->bmpSize += padlen;
		}
	}
	/** uvolnění paměti s obrázkem */
	free(data);
	return(GIF2BMPOK);
}

/**
 * Načtení jednoho bitu ze vstupního souboru
 * @param inputFile	soubor z kterého probíhá čtení
 * @param ch hodnota načítaného bitu
 * @param codedSize velikost zakódovaného vstupu
 * @return AHEDOK pokud se podařilo načíst byte ze vstupního souboru, jinak
 * 		AHEDfail
 */
int8_t readBit(FILE* inputFile, u_int8_t* ch, tGIF2BMP* gif2bmp) {
	/** proměnné sdílené mezi jednotlivými voláními funkce */
	static u_int8_t readed = 0;
	static u_int8_t bits = 0;
	static int16_t bs = 0; 
	
	if (bits == 0) {
		/** načti byte */
		if (bs == 0) {
			if (fread(&bs, sizeof(int8_t), 1, inputFile) == 0) {
				/** při neúspěšném čtení vrať chybu */
				return(GIF2BMPFail);
			}
			gif2bmp->gifSize++;
		}
		if (fread(&readed, sizeof(int8_t), 1, inputFile) == 0) {
			/** při neúspěšném čtení vrať chybu */
			return(GIF2BMPFail);
		}
		bs--;
		gif2bmp->gifSize++;
		/** zvětši velikost kódovaného vstupu */
		bits = 8;
	}

	/** vypočítej hodnotu daného bitu */
	*ch = readed & 1;
	
	/** aktualizuj bity */	
	readed >>=1;
	bits--;

	return(GIF2BMPOK);
}

/**
 * načtení symbolu ze vstupního souboru
 * @param input načtený symbol
 * @param bits počet bitů na symbol
 * @param inputFile vstupní soubor
 * @param gif2bmp struktura s velikostí vstupního a výstupního souboru
 */
int8_t getCode(int16_t* input, u_int8_t bits, FILE* inputFile, 
	tGIF2BMP* gif2bmp) {
	u_int8_t ch;
	int16_t bit = 1;
	*input = 0;
	
	/** načteme požadovaný počet bitů */
	for (u_int8_t i = 0; i < bits; i++) {
		if (readBit(inputFile, &ch, gif2bmp) == GIF2BMPFail) {
			return(GIF2BMPFail);
		}
		
		if (ch) { /** vytvoříme z nich výstupní kódové slovo */
			*input |= bit;
		}
		
		bit <<= 1;
	}
	return(GIF2BMPOK);
}

/**
 * zapíše do bufferu obrázku data
 * @param di struktura s informacemi dekodéru
 * @param ktrerý kód má být zapsán do výstupu
 */
void output(struct decoderInfo* di, int16_t code) {
	memcpy(&di->image[di->imIndex], di->dict[code].string, di->dict[code].length);
	di->imIndex += di->dict[code].length;
}

/**
 * Inicializace dekodéru
 * @param di struktura s informacemi o dekodéru
 * @param inputFile vstupní soubor
 */
int8_t decoderInit(struct decoderInfo* di, FILE* inputFile) {
	
	di->CWlen = di->initCWlen + 1;	/** nastavení počtu bitů */
	di->CC = 1 << (di->CWlen - 1);	/** clear code */
	di->EOI = di->CC + 1;			/** end of input */
	di->max = (1 << di->CWlen);		/** maximální index pro aktuální počet bitů */
	di->next = di->CC + 2;			/** následující volný inde */
	
	/** uvolnění paměti */
	for (int16_t i = 0; i < MAX_DICT_SIZE; i++) {
		if (di->dict[i].length) {
			free(di->dict[i].string);
		}
		di->dict[i].length = 0;
	}
	
	/** inicializace paměti pro prvních n položek */
	int16_t cnt = 1 << di->initCWlen;
	for (int16_t i = 0; i < cnt; i ++) {
		di->dict[i].string = malloc(sizeof(u_int8_t));
		di->dict[i].string[0] = i;
		di->dict[i].length = 1;
	}
	
	/** načtení 'předchozího' znaku */
	if (getCode(&di->last, di->CWlen, inputFile, di->gif2png) == GIF2BMPFail) {
		return(GIF2BMPFail);
	}
	/** zapiš data pro daný vstupní kód do obrázku */
	output(di, di->last);
	
	return(GIF2BMPOK);
}

/**
 * přidání nového kódu
 * @param di struktura s informacemi o dekodéru
 * @param code výchozí znak
 * @param concat připojovaný znak
 */
int8_t addNewCode(struct decoderInfo* di, int16_t code, int16_t concat) {
	// vypočítání velikosti paměti, a její alokace, případně realokace
	if (di->dict[code].length == 0) { //nový kód
		di->dict[code].length = di->dict[di->last].length + 1;
		di->dict[code].string = malloc(sizeof(u_int8_t) *
										 di->dict[code].length);
		if (di->dict[code].string == NULL) { //alokace paměti selhala
			return(GIF2BMPFail);
		}
	} else { //známý kód
		di->dict[code].length = di->dict[di->last].length + 1;
		di->dict[code].string = realloc(di->dict[code].string,
							sizeof(u_int8_t) * di->dict[code].length + 1);
		 if (di->dict[code].string == NULL) { //alokace paměti selhala
			return(GIF2BMPFail);
		}
	}
	
	// zkopírování řetězce
	memcpy(di->dict[code].string, di->dict[di->last].string, di->dict[di->last].length);
	di->dict[code].string[di->dict[di->last].length] = concat;
	
	di->next++;
	if (di->next >= di->max && di->CWlen < MAX_BITS) { //zvětšení počtu bitů
		di->CWlen++;
		di->max = (1 << di->CWlen);
	}
		
	return(GIF2BMPOK);
}

/**
 * Uvolnění naalokované paměti
 * @param di struktura s informacemi o dekodéru
 */
void freeMemory(struct decoderInfo* di) {
	/** uvolnění paměti */
	for (int16_t i = 0; i < MAX_DICT_SIZE; i++) {
		if (di->dict[i].length) {
			free(di->dict[i].string);
		}
		di->dict[i].length = 0;
	}
}

/**
 * Zpracování zkomprimovaného vstupu
 * @param inputFile vstupní komprimovaný soubor
 */
int8_t decode(FILE* inputFile, tGIF2BMP* gif2bmp, struct gifHeader* gifh, 
	u_int8_t* image) {
	struct decoderInfo di; 
	
	int16_t	code;
	
	/** načtení výchozího počtu bitů do slovníku */
	if (fread(&di.initCWlen, 1, 1, inputFile) == 0) {
		return(GIF2BMPFail);
	}
	gif2bmp->gifSize += 1;
	
	/** počáteční nastavení dekodéru */
	di.CWlen = di.initCWlen + 1;
	di.CC = 1 << di.initCWlen;
	di.gif2png = gif2bmp;
	di.image = image;
	di.imIndex = 0;
	di.next = di.CC + 2;
	
	for (int16_t i = 0; i < MAX_DICT_SIZE; i++) {
		di.dict[i].length = 0;
	}
	
	 /** načtení úvodního clear code */
	if (getCode(&code, di.CWlen, inputFile, gif2bmp) == GIF2BMPFail || code != di.CC) {
		return(GIF2BMPFail);
	}
	
	/** inicializace struktury dekodéru (hlavně načtení kódu) */
	decoderInit(&di, inputFile);

	/** nekonečná smyčka načítající vstupní data */
	for (;;) {	
		/** načtaní dalšího kódového slova */
		if (getCode(&code, di.CWlen, inputFile, gif2bmp) == GIF2BMPFail) {
			return(GIF2BMPFail);
		}
		
		if (code == di.CC) { /** načetli jsme clear code, začínáme od začátku */
			PRINT_DEBUG("Clear code\n");
			decoderInit(&di, inputFile);
		} else
		if (code == di.EOI) { /** načetli jsme znak konce LZW dat, končíme */
			PRINT_DEBUG("End Of Input\n");
			freeMemory(&di);
			return(GIF2BMPOK);
		} else { /** načetli jsme jiné kódové slovo */
			if (code < di.next) { /** načetli jsme známé kódové slovo */
				output(&di,code);
				if (addNewCode(&di, di.next, di.dict[code].string[0]) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
			} else { /** načetli jsme neznámé kódové slovo */
				if (addNewCode(&di, code, di.dict[di.last].string[0]) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
				output(&di,code);
			}
			di.last = code;
		}
	}
	freeMemory(&di);
	return(GIF2BMPOK);
}

/**
 * Kontrola zda jsme na konci bloku -- konec bloku je označen jako 0x00
 * @param inputFile vstupní soubor
 * @param gif2bmp počitadlo vekolosti vstupního/výstupního souboru
 * @return GIF2BMPOK pokud nedošlo k chybě, jinak GIF2BMPFail 
 */
int8_t testEndOfBlock(FILE* inputFile, tGIF2BMP* gif2bmp) {
	int16_t ch = fgetc(inputFile);
	if (ch == EOF) return(GIF2BMPFail);
	gif2bmp->gifSize++;
	if (BLOCK_TERMINATOR == ch) {
		PRINT_DEBUG("Block Terminator found... GOOD\n");
		return(GIF2BMPOK);
	} else {
		PRINT_DEBUG("Block Terminator NOT found... BAD\n");
		return(GIF2BMPFail);
	}
}

/**
 * Přeskočení bloků rozšíření -- blok začíná značkou délky bloku (1B) a je
 * ukončen značkou 0x00
 * @param inputFile vstupní soubor
 * @param gif2bmp počítadlo velikosti vstupního/výstupního souboru
 * @return GIF2BMPOK pokud nedošlo k chybě, jinak GIF2BMPFail
 */
int8_t skipBlocks(FILE* inputFile, tGIF2BMP* gif2bmp) {
	u_int8_t blockSize;
	do {
		if (fread(&blockSize, sizeof(blockSize), 1, inputFile) == 0) {
			return(GIF2BMPFail);
		}
		gif2bmp->gifSize += blockSize;
		gif2bmp->gifSize += sizeof(blockSize);
		#ifdef DEBUG
		fprintf(stderr, "block size: %d\n", blockSize);
		#endif
		if (blockSize > 0) {
			if (fseek(inputFile, blockSize, SEEK_CUR) != 0) {
				return(GIF2BMPFail);
			}
		}
	} while (blockSize != BLOCK_TERMINATOR);
	return(GIF2BMPOK);
}

/**
 * Přeskočení rozšiřujících bloků
 * @param inputFile vstupní soubor
 * @param gif2bmp počítadlo načtených/zapsaných byte
 * @return GIF2BMPOK pokud nedošlo k chybě, jinak GIF2BMPFail
 */
int8_t skipExtensions(FILE* inputFile, tGIF2BMP* gif2bmp) {
	u_int8_t magic; /** uložení magického symbolu oddělujícího sekce */

	do { 	/** potřebujeme se dostat na hranici hlavičky Image Block */
		if (fread(&magic, 1, 1, inputFile) == 0) {
			return(GIF2BMPFail);
		}
		gif2bmp->gifSize += 1;
		
		/** načetli jsme hlavičku rozšiřujícího bloku */
		if (magic == EXTENSION_BLOCK_BEGIN_MARKER) {
			PRINT_DEBUG("Skiping EXTENSION");
			/** načteme typ rozšiřujícího bloku */
			if (fread(&magic, sizeof(magic), 1, inputFile) == 0) {
				return(GIF2BMPFail);
			}
			gif2bmp->gifSize += 1;
		
			if (magic == GRAPICS_EXTENSION_BLOCK) {
				/** přeskočení graphics extension block */
				PRINT_DEBUG(" -- GRAPHICS\n");
				if (skipBlocks(inputFile, gif2bmp) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
			}
			if (magic == PLAIN_TEXT_EXTENSION_BLOCK) {
				/** přeskočení plain text extension block */
				PRINT_DEBUG(" -- PLAIN TEXT\n");
				if (skipBlocks(inputFile, gif2bmp) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
			}
			if (magic == APPLICATION_EXTENSION_BLOCK) {
				/** přeskočení application extension block */
				PRINT_DEBUG(" -- APPLICATION\n");
				if (skipBlocks(inputFile, gif2bmp) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
			}
			if (magic == 0xfe) {
				/** přeskočení comment extension block */
				PRINT_DEBUG(" -- COMMENT\n");
				if (skipBlocks(inputFile, gif2bmp) == GIF2BMPFail) {
					return(GIF2BMPFail);
				}
			}
		}
	} while (magic != IMAGE_BLOCK_BEGIN_MARKER);
	/** zpracování případných rozšiřujících hlaviček proběhlo v pořádku */
	return(GIF2BMPOK);
}

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
int gif2bmp(tGIF2BMP *gif2bmp, FILE *inputFile, FILE *outputFile) {
	struct gifHeader gifh;			///< hlavička GIF souboru
	struct globalPaletteInfo gpi;	///< globální informace o paletě
	struct localPaletteInfo lpi;	///< lokální informace o paletě
	struct imgHeader im;			///< informace o subdokumentu
	
	/** paleta barev */
	struct qrgb palette[256];
	
	/** čtení složeného datového typu, raději použijeme zadanou velikost */
	if (fread(&gifh, sizeof(gifh), 1, inputFile) == 0) {
		return(GIF2BMPFail);
	}
	gif2bmp->gifSize = sizeof(gifh);
	getGlobalPaletteInfo(gifh.bits, &gpi);

	#ifdef DEBUG
		fprintf(stderr, "palette colors: %d\n", 1 << (gpi.bpp+1));
	#endif

	/** nepodporujeme méně než osmibitové soubory (256 barev) */
	if (gpi.bpp < 7) {
		return(GIF2BMPFail);
	}

	/** je použita globální barevná paleta, provedeme její načtení */	
	if (gpi.global) {
		///< kolik barev budeme načítat
		int16_t colors = 1 << (gpi.length + 1);
		
		/** načtení barev a jejich uložení do pole */
		for (int index = 0; index < colors; index++) {
			if (fread(&palette[index], COLOR_SIZE, 1, inputFile) == 0) {
				return(GIF2BMPFail);
			}
			gif2bmp->gifSize += COLOR_SIZE;
			palette[index].res = 0;
		}
	}
	
	/** přeskočení rozšiřujících hlaviček */
	if (skipExtensions(inputFile, gif2bmp) == GIF2BMPFail) {
		return(GIF2BMPFail);
	}
	
	/** načtení hlavičky Image Block */
	if (fread(&im, sizeof(im), 1, inputFile) == 0) {
		return(GIF2BMPFail);
	}
	gif2bmp->gifSize += sizeof(im);
	
	/** zpracování informací o lokální paletě */
	getLocalPaletteInfo(im.flags, &lpi);

	/** lokální barevné paleta použita? nerozumět! */
	if (lpi.local) {
		return(GIF2BMPFail);
	}
	
	/** dekódování vstupního souboru */
	#ifdef DEBUG
		fprintf(stderr, "width: %d, height: %d, malloc: %lu\n", gifh.width,
				 gifh.height, sizeof(u_int8_t) * gifh.width * gifh.height);
	#endif
	u_int8_t* image = malloc(sizeof(u_int8_t) * gifh.width * gifh.height);
	if (decode(inputFile, gif2bmp, &gifh, image) == GIF2BMPFail) {
		return(GIF2BMPFail);
	}
	
	/** zkontrolujeme jestli jsme na konci souboru */
	testEndOfBlock(inputFile, gif2bmp);
	
	/** dočteme soubor do konce */
	while (fgetc(inputFile) != EOF) {
		gif2bmp->gifSize++;
	}

	/** vrátíme výsledek zápisu dat */
	return(writeBmpData(outputFile, &gifh, &gpi, palette, image, lpi.interlaced,
		gif2bmp));
}
