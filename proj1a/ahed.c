/*
 * Autor:	Jaroslav Bartoň, xbarto42
 * Datum:	6.4.2008
 * Soubor:	ahead.c
 * Komentar:
 */

#include "ahed.h"

/**
 * Struktura jednoho uzlu v poli
 */
struct node {
	int64_t count;	/** kolikrát se znak objevil ve vstupním řetězci */
	int64_t order;	/** pořadí ve stromu pro případný přesun */
	int64_t	left;	/** ukazatel na levého syna */
	int64_t right;	/** ukazatel na pravého syna */
	int64_t parent;	/** ukazatel na otce */
};

/**
 * Struktura popisujici pruchod stromem od korene k uzlu
 */
struct path {
	int32_t path;	/** cesta */
	int8_t bits;	/** kolik bitu cesty je vyznamovych */
};

/** 
 * Nastavení všech uzlů na výchozí hodnoty
 * @param nodes	pole uzlů které mají být nastaveny
 */
void initNodes(struct node nodes[]) {
	for (int i = 0; i < AHEDlength; i++) {
		nodes[i].count = 0;
		nodes[i].left = AHEDnullNode;
		nodes[i].right = AHEDnullNode;
		nodes[i].parent = AHEDnullNode;
		nodes[i].order = 0;
	}
}

/**
 * Ziskani cesty od korene k uzlu ve stromu
 * @param nodes	pole s uzly stromu
 * @param node	uzel pro ktery chceme urcit cestu od korene
 * @param root	korenovy uzel
 * @param path	informace o nalezene ceste a poctu bitu
 * @return	AHEDOK
 */
char getNodePath(struct node nodes[], int64_t node, int64_t root, struct path* path) {
	int actual = node;
	int parent;
	path->path = 0;
	path->bits = 0;
	
	/** dokud nedorazis na vrchol stromu */
	while (root != actual) {
		/** uloz si ukazatele na rodice */
		parent = nodes[actual].parent;
		/** pokud jsi jeho pravy syn, uloz si do cesty jednicku */
		if (nodes[parent].right == actual) {
			path->path |= (1 << path->bits);
		}
		/** popojdi ve stromu o jedno patro vyse */
		actual = parent;
		/** a inkrementuj pocet bitu popisujicich cestu */
		path->bits++;
	}
	return(AHEDOK);
}

/**
 * zápis do výstupního proudu
 * uchovává si zapisovanou hodnotu a počet bitů, který je ještě volný
 * v okamžiku, kdy klesne počet volných bitů, provede zápis do souboru
 * @param file	výstupní proudu
 * @param path	informace o počtu bitů k zapsání
 */
int wos(FILE* file, struct path* path, int64_t* codedSize) {
	/** data uchovavane mezi jednotlivymi volanimi funkce */
	static unsigned char output;	/** vystup k zapsani */
	static char bit;				/** kolik bitu vystupu je jiz obsazeno */
	
	/** projdi vsechny bity vstupu */
	for (int i = path->bits-1; i >= 0; i--) {
		/** pokud vstup obsahuje na dane pozici 1 */
		if (path->path & (1 << i)) {
			/** uloz ji do vystupu */
			output |= 1 << (8-bit-1);
		}
		
		/** inkrementuj pocet pouzitych bitu vystupu */
		bit++;
		
		/** pokud jsi vyuzil cely vystup */
		if (bit == 8) {
			/** zapis vystup do souboru */
			if (fprintf(file, "%c", output) == 0) {
				return(AHEDFail);
			}
			/** a vynuluj vystup a pocet pouzitich bitu */
			output = 0;
			bit = 0;
			(*codedSize)++;
		}
	}
	
	return(AHEDOK);
}

/**
 * Zapsání zbývajících znaků do souboru + znak EOF (EOF je složen ze
 * značky nového znaku a nedostatečného počtu bitů pro nový znak)
 * @param file	ukazatel na výstupní soubor
 * @return informace o tom, zda se povedl zápis
 */
int flushWos(FILE* file, int64_t* codeSize, struct node nodes[], int64_t root) {
	//TODO zajistit vypsani zbytku uloženému ve výstupu
	struct path p;
	
	/** vynuceni zapsani konce souboru */
	getNodePath(nodes, AHEDzeroNode, root, &p);
	wos(file, &p, codeSize);
	
	p.path = 0;
	p.bits = 7;
	
	return(wos(file, &p, codeSize));
}

/**
 * zápis znaku do výstupního proudu
 * @param file	výstupní stream
 * @param c		znak k zapsání
 */
int wch(FILE* file, char c, int64_t* codeSize) {
	struct path p = {c, 8};
	/** zavoláme zápis do výstupního proudu pro hodnotu s platnými 8mi bity */
	return(wos(file, &p, codeSize));
}

/** 
 * přidání uzlu do stromu 
 * @param nodes	prvky stromu
 * @param ncnt	počet uzlů
 * @param left	index levého syna
 * @param right	index pravého syna
 * @return index pridaného uzlu
 */
int addNewNode(struct node nodes[], int64_t left, int64_t right) {
	static int64_t ncnt = 0;
	
	/** výpočet aktuálního volného uzlu */
	int u = (1 << AHEDbitness) + ncnt;
	/** pokud původní uzel neukazuje na kořen stromu */
	if (nodes[left].parent != AHEDnullNode) {
		/** nastav si ukazetele u rodiče a u sebe*/
		nodes[u].parent = nodes[left].parent;
		nodes[nodes[u].parent].left = u;
	}
	
	/** nastavi si spravne pořadí */
	nodes[u].order = nodes[left].order;
	/** nastav správně rodiče */
	nodes[left].parent = u;
	nodes[right].parent = u;
	/** nastav správně pořadí */
	nodes[left].order = nodes[u].order - 2;
	nodes[right].order = nodes[u].order - 1;
	/** pravý uzel je nově přidaný, nastav mu výchozí hodnotu */
	nodes[right].count = 1;
	/** nastav rodiči správné ukazatele */
	nodes[u].left = left;
	nodes[u].right = right;
	/** zvyš počet obsazených uzlů */
	(ncnt)++;
	
	return(u);
}

/**
 * Nalezení uzlu s odpovídajícími vlastnostmi
 * @param nodes	pole uzlů v kterých je uložen strom
 * @param count	hledáme uzel se stejným obsahem count
 * @param order	hledáme uzel jehož order je vyšší než naše
 * @return index nalezeného pole, jinak AHEDFail
 */
int findNode(struct node nodes[], int64_t count, int64_t order) {
	/** projdi celé pole v kterém jsou uloženy uzly */
	for (int64_t i = 0; i < AHEDlength; i++) {
		/** a najdi uzel se stejnou hodnotou a vyšším pořadím */
		if ((nodes[i].count == count) && (nodes[i].order > order)) {
			/** rekurzivně najdi nejvyšší pořadí */
			int i2 = findNode(nodes, count, order+1);
			if (i2 != AHEDFail) {
				return(i2);
			}
			return(i);
		}
	}
	return(AHEDFail);
}

/**
 * aktualizace stromu
 * @param nodes	prvky stromu
 * @param u vuci kteremu uzlu
 * @param root koren uzlu
 */
void updateTree(struct node nodes[], int64_t u, int64_t root) {
	int64_t actual = u;
	
	/** dokud se nedopracuješ ke kořeni stromu */
	while (actual != root) {
		int64_t id;
		/** nalezneme uzel vůči kterému se budeme vyměňovat */
		if ((id = findNode(nodes, nodes[actual].count, nodes[actual].order)) 
			!= AHEDFail && id != root &&
				nodes[actual].parent != id) {
			
			/** ulož si původní hodnoty */
			int64_t idx = nodes[id].parent;
			int64_t order = nodes[id].order;

			/** nastav hodnoty vyměňovaného uzlu */
			nodes[id].parent = nodes[actual].parent;
			nodes[id].order = nodes[actual].order;
			
			if (nodes[nodes[actual].parent].left == actual) {
				nodes[nodes[actual].parent].left = id;
			} else {
				nodes[nodes[actual].parent].right = id;
			}

			/** nastav vyměňovanému uzlu svoje hodnoty */
			nodes[actual].parent = idx;
			nodes[actual].order = order;
			if (nodes[idx].right == id) {
				nodes[idx].right = actual;
			} else {
				nodes[idx].left = actual;
			}
		}
		/** zvyš hodnocení uzlu */
		nodes[actual].count++;
		/** povpojdi o úroveň výš */
		actual = nodes[actual].parent;
	}
	/** vyš hodnocení kořenovému uzlu */
	nodes[actual].count++;
}

/* Nazev:
 *   AHEDEncoding
 * Cinnost:
 *   Funkce koduje vstupni soubor do vystupniho souboru a porizuje zaznam o
 *	 kodovani.
 * Parametry:
 *   ahed - zaznam o kodovani
 *   inputFile - vstupni soubor (nekodovany)
 *   outputFile - vystupni soubor (kodovany)
 * Navratova hodnota:
 *    0 - kodovani probehlo v poradku
 *    -1 - pri kodovani nastala chyba
 */
int AHEDEncoding(tAHED *ahed, FILE *inputFile, FILE *outputFile) {
	int64_t ch = 0;
	int64_t root = AHEDzeroNode;
	struct path path;
	
	/** pole pro uzly stromu */
	struct node nodes[AHEDlength];
	/** inicializace uzlů */
	initNodes(nodes);
	/** nastav nejvyšší skóre zatím jedinému uzlu zero */
	nodes[AHEDzeroNode].order = AHEDzeroNode;
	
	/** dokud se daří načítat vstup */
	while ((fread(&ch, sizeof(char), 1, inputFile)) != 0) {
		/** zvětši velikost nekódovaného vstupu */
		ahed->uncodedSize++;
		/** pokud jsi znak načetl poprvé */
		if (nodes[ch].count == 0) {
			int64_t i;
			/** získej cestu od uzlu zero ke kořeni */
			getNodePath(nodes, AHEDzeroNode, root, &path);
			
			/** zapiš cestu k uzlu zero a zapiš znak */ 
			if (wos(outputFile, &path, &ahed->codedSize) == AHEDFail ||
				wch(outputFile, ch, &ahed->codedSize) == AHEDFail) {
				return(AHEDFail);
			}
			
			/** proveď přidání nového uzlu */
			i = addNewNode(nodes, AHEDzeroNode, ch);
			/** a pokud byl kořen shodný s uzlem zero, změň kořen */
			if (root == AHEDzeroNode) {
				root = i;
			}
			/** aktualizuj strom */
			updateTree(nodes, i, root);
		} else {
			/** jinak jsi znak již viděl, získej cestu od znaku ke kořeni  */
			getNodePath(nodes, ch, root, &path);
			/** a zapiš cestu */
			if (wos(outputFile, &path, &ahed->codedSize) == AHEDFail) {
				return(AHEDFail);
			}
			/** aktualizuj strom */
			updateTree(nodes, ch, root);
		}
	}
	/** vyprázdni případné zbývající znaky, zapiš konec souboru */
	return(flushWos(outputFile, &ahed->codedSize, nodes, root));
}

/**
 * Načtení jednoho bitu ze vstupního souboru
 * @param inputFile	soubor z kterého probíhá čtení
 * @param ch hodnota načítaného bitu
 * @param codedSize velikost zakódovaného vstupu
 * @return AHEDOK pokud se podařilo načíst byte ze vstupního souboru, jinak
 * 		AHEDfail
 */
int readBit(FILE* inputFile, unsigned char* ch, int64_t* codedSize) {
	/** proměnné sdílené mezi jednotlivými voláními funkce */
	static char readed = 0;
	static char bits   = 0;
	
	/** výchozí návratová hodnota */
	if (bits == 0) {
		/** načti byte */
		if (fread(&readed, sizeof(char), 1, inputFile) == 0) {
			/** při neúspěšném čtení vrať chybu */
			return(AHEDFail);
		}
		/** zvětši velikost kódovaného vstupu */
		(*codedSize)++;
		bits = 8;
	}

	/** vypočítej hodnotu daného bitu */
	*ch = readed & 128;
	
	/** aktualizuj bity */	
	readed = readed << 1;
	bits--;

	return(AHEDOK);
}

/**
 * Načtení znaku
 * @param inputFile vstupní soubor
 * @param cc hodnota načteného znaku
 * @param codedSize velikost zakódovaného vstupu
 * @return AHEDOK pokud se načtení znaku podařilo, jinak AHEDFail
 */
char readChar(FILE* inputFile, unsigned char* cc, int64_t* codedSize) {
	/** načítaný znak */
	char readed = 0;
	/** aktuálně načtený bit */
	unsigned char ch;
	/** proveď osmkrát načtení bitu */
	for (int i = 0; i < AHEDbitness; i++) {
		/** aktualizuj načítaný znak */
		readed = readed << 1;
		if (readBit(inputFile, &ch, codedSize) == AHEDFail) {
			return(AHEDFail);
		}
		/** aktualizuj načítaný znak */
		if (ch) {
			readed |= 1;
		}
	}
	/** ulož výsledek */
	*cc = readed;
	return(AHEDOK);
} 

/* Nazev:
 *   AHEDDecoding
 * Cinnost:
 *   Funkce dekoduje vstupni soubor do vystupniho souboru a porizuje zaznam o
 * 	 dekodovani.
 * Parametry:
 *   ahed - zaznam o dekodovani
 *   inputFile - vstupni soubor (kodovany)
 *   outputFile - vystupni soubor (nekodovany)
 * Navratova hodnota:
 *    0 - dekodovani probehlo v poradku
 *    -1 - pri dekodovani nastala chyba
 */
int AHEDDecoding(tAHED *ahed, FILE *inputFile, FILE *outputFile) {
	int64_t ch;
	unsigned char bit;
	int64_t root;
	int64_t actual;
	int64_t anode;
	struct node nodes[AHEDlength];
	
	initNodes(nodes);
	nodes[AHEDzeroNode].order = AHEDzeroNode;
	
	/** načtení a zpracování prvního znaku */
	if (readChar(inputFile, &bit, &ahed->codedSize) == AHEDFail) {
		return(AHEDFail);
	}
	/** nastavení kořene, aktuálního prvku, aktualizace stromu, zápis výsledku */
	actual = root = addNewNode(nodes, AHEDzeroNode, bit);
	updateTree(nodes, actual, root);
	fwrite(&bit, sizeof(char), 1, outputFile);
	ahed->uncodedSize++;
	
	/** nekonečněkrát opakuj (ukončení returnem v cyklu) */
	while (1) {
		/** nastav aktuální prvek na kořen */
		actual = root;
		/** dokud klesáš ve stromu */
		while (actual != AHEDzeroNode && nodes[actual].left != AHEDnullNode) {
			/** načti bit */
			if (readBit(inputFile, &bit, &ahed->codedSize) == AHEDFail) {
				/** 
				 * chyba při čtení jednoho bitu, to se nemělo stát -> chybný
				 * konec
				 */
				return(AHEDFail);
			}
			/** a podle něj rozhodni kam dál pokračovat */
			if (bit) {
				actual = nodes[actual].right;
			} else {
				actual = nodes[actual].left;
			}
			/** kterému znaku odpovídá aktuální pozice */
			ch = actual;
		}
		
		/** pokud jsme se dostali k uzlu zero */
		if (actual == AHEDzeroNode) {
			/** proveď načtení znaku */
			if (readChar(inputFile, &bit, &ahed->codedSize) == AHEDFail) {
				/** 
				 * při čtení znaku jsme se dostali na konec souboru -> 
				 * takto máme zaveden konec kódovaného souboru, předpokládáme
				 * správné zpracování
				 */
				return(AHEDOK);
			}
			/** přidej uzel */
			anode = addNewNode(nodes, AHEDzeroNode, bit);
			ch = bit;
		} else {
			anode = ch;
		}
		/** aktualizuj strom */
		updateTree(nodes, anode, root);
		
		/** zapiš výsledek */
		fwrite(&ch, sizeof(char), 1, outputFile);
		ahed->uncodedSize++;
	}
}
