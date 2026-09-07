#ifndef MENU_SGATESETTING_HPP
#define MENU_SGATESETTING_HPP

#include <stdint.h>
#include "menu/UI.hpp"

void MenuConfigInit(void);
bool changesettings(EasyUIItem_t* item);

// Exposed helpers implemented in SGateSetting.cpp
bool configsettingcmp(char *a, char *b, int length);
void save_settings(void);

#endif // MENU_SGATESETTING_HPP
