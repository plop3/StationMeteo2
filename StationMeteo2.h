#define DEBUG_OFF
// Communication série
#define SERIALBAUD 9600

// Pluviomètre
#define Plevel 0.517	// mm de pluie/1000 par impulsion (Réel: 0.517mm/imp)
// Pins
#define PINrain			D4 // Pluviomètre
#define IRQ_ORAGE 		D5 // PIN IRQ MOD1016	
#define DATAPIN			D7 // Anémomètre Lacrosse TX20

//#define CORAGE	// MOD-1016
#define RTEMP     // Lecture de la T° extérieure depuis station météo 1
