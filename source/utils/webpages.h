#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// Dichiarazione delle pagine HTML memorizzate in PROGMEM
extern const char GAME_HTML_PAGE[] PROGMEM;
extern const char WIFI_HTML_PAGE[] PROGMEM;

// Include l'implementazione (workaround per le limitazioni dell'IDE Arduino)
#include "webpages_impl.h"

#endif // WEBPAGES_H