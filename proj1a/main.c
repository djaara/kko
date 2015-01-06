/*
 * Autor:    Jaroslav Barton (xbarto42)
 * Datum:
 * Soubor:   main.c
 * Komentar:
 */ 
#include <stdlib.h>
#include <getopt.h> /** C99 getopt */

#include "ahed.h"

/** Informace o tom zda se poradilo zpracovat parametry prikazove radky, pouzito
 * jako navratova hodnota */
#define COMMAND_LINE_OK 1
#define COMMAND_LINE_ERR 0

/** jmeno pouzite ve vystupu do logovaciho souboru */
const char* username = "xbarto42";

/**
 * Struktura s konfigurací aplikace
 */
struct configuration {
	char* input; 		/** jméno vstupního souboru */
	FILE* ifile;
	char* output;		/** jméno výstupního souboru */
	FILE* ofile;
	char* log;			/** jméno pro uložení informací o de/kompresi*/
	FILE* lfile;
	char direction;		/** de/komprese */
};

/**
 * Otevření jednoho souboru a test zda se otevření podařilo
 * @param filename	název souboru
 * @param file		ukazatel na otevřený soubor
 * @param mode		v jakém módu má být soubor otevřen
 */
void openOneFile(char* filename, FILE** file, char* mode) {
	*file = fopen(filename, mode);
	if (*file == NULL) {
		perror("fopen");
		exit(-1);
	}
}

/**
 * Otevření souborů se kterými budu pracovat
 * @param config struktura s konfigurací aplikace
 */
void openFiles(struct configuration* config) {
	/** otevřeme vstupní soubor */
	if (config->input) {	
		openOneFile(config->input, &config->ifile, "rb");
	} else {
		config->ifile = stdin;
	}
	
	/** otevřeme výstupní soubor */
	if (config->output) {
		openOneFile(config->output, &config->ofile, "wb");
	} else {
		config->ofile = stdout;
	}
	
	/** otevřeme soubor se záznamem */
	if (config->log) {
		openOneFile(config->log, &config->lfile, "w");
	} else {
		config->lfile = NULL;
	}
}

/**
 * Zavření souborů skterými jsme pracovali
 * @param config struktura s konfigurací aplikace
 */
void closeFiles(struct configuration* config) {
	/** zavřeme vstupní soubor */
	if (config->ifile != stdin) {
		fclose(config->ifile);
	}
	config->ifile = NULL;
	
	/** zavřeme výstupní soubor */
	if (config->ofile != stdout) {
		fclose(config->ofile);
	}
	config->ofile = NULL;
	
	/** zavřeme soubor se záznamem */
	if (config->lfile != NULL) {
		fclose(config->lfile);
	}
	config->lfile = NULL;
}

/**
 * Zapsani vysledky zpracovani do logovaciho souboru
 * @param config	konfigurace programu
 * @param result	vysledky prevodu
 */
void writeResults(struct configuration* config, tAHED* result) {
	/** pokud je zadán soubor pro uložení záznamu */
	if (config->lfile != NULL) {
		/** zapíšeme autorovo jméno */
		fprintf(config->lfile, "login = %s\n", username);
		/** velikost nezakódovaného souboru */
		fprintf(config->lfile, "uncodedSize = %lld\n", (long long int)result->uncodedSize);
		/** velikost zakódovaného souboru */
		fprintf(config->lfile, "codedSize = %lld\n", (long long int)result->codedSize);
	}
}

/**
 * zpracování parametrů příkazové řádky
 * @param argc	počet parametrů příkazové řádky
 * @param argv	pole ukazatelů na parametry příkazové řádky
 * @param config	struktura do které se uloží konfigurační hodnoty
 */
int commandline(int argc, char **argv, struct configuration* config) {
	int c;
	extern char *optarg;
	
	/** výchozí směr komprese je nedefinováno */
	config->direction = AHEDUndefined;
	config->log = NULL;
	config->input = NULL;
	config->output = NULL;
	
	/** zpracování parametrů příkazové rádky */
	while ((c = getopt(argc, argv, "i:o:l:cxh")) != -1) {
		switch (c) {
			case 'i':	/** parametr specifikující vstupní soubor */
				config->input = optarg; 
				break;
			case 'o':	/** parametr specifikující výstupní soubor */
				config->output = optarg;
				break;
			case 'l':	/** parametr specifikující log soubor */
				config->log = optarg;
				break;
			case 'c':	/** komprimuj vstupní soubor */
				config->direction = AHEDCompress;
				break;
			case 'x':	/** dekomprimuj výstupní soubor */
				config->direction = AHEDDecompress;
				break;
			case 'h':	/** zobraz nápovědu */
				return(COMMAND_LINE_ERR);
			case '?':
				exit(-1);
		}		
	}
	return(COMMAND_LINE_OK);
}

/**
 * Vypsání nápovědy k aplikaci - její vypsání je zajištěno v případě
 * že zadán parametr -h
 */
void help(void) {
	printf("ahead [-i ifile] [-o ofile] [-l logfile] [-c] [-x] [-h]\n\n"
			"\t-i ifile jméno vstupního souboru, pokud není zadán bude se\n"
			"\t\t za vstup považovat stdin\n"
			"\t-o ofile jméno výstupního souboru, pokud není zadán bude se\n"
			"\t\t za výstup považovat stdout\n"
			"\t-l lfile jméno souboru pro výstupní zprávy, pokud není zadán\n"
			"\t\t bude výstup ignorován\n"
			"\t-c\t komprimuj vstupní soubor\n"
			"\t-x\t dekomprimuj vstupní soubor\n"
			"\t-h\t zobrazí tuto nápovědu\n");
}

/**
 * spuštění aplikace, předání počtu parametrů a jejich výčet
 */
int main(int argc, char **argv) {
	struct	configuration configuration; /** konfigurace zpracování */
	int		retval=0;						 /** návratová hodnota */
	
	/** pokud se podari zpracovani prikazove radky */
	if (commandline(argc, argv, &configuration)) {
		tAHED	result = {0, 0};			/** výsledky de/komprese */
		
		/** kontrola kterym smerem se ma provadet prevod */
		if (configuration.direction == AHEDCompress) {
			/** komprese, otevreme soubory */
			openFiles(&configuration);
			/** zpracujeme */
			retval = AHEDEncoding(&result, configuration.ifile,
									configuration.ofile);
			/** zapiseme vysledek prevodu */
			writeResults(&configuration, &result);
			/** zavreme soubory */
			closeFiles(&configuration);
		} else if (configuration.direction == AHEDDecompress) {
			/** dekomprese, otevreme soubory */
			openFiles(&configuration);
			/** zpracujeme */
			retval = AHEDDecoding(&result, configuration.ifile,
									configuration.ofile);
			/** zapiseme vysledky prevodu */
			writeResults(&configuration, &result);
			/** zavreme soubory */
			closeFiles(&configuration);
		}
	} else {
		/** zpracovani prikazove radky se nezdarilo, vypiseme ovladani */
		help();
	}
	
	/** ukoncime s navratovou hodnotou, ktera nam byla vracena po zpracovani */
	exit(retval);
}
