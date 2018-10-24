#ifndef UCI_H
#define UCI_H

#include "types.h"

void parsePosition(Board& b, std::string input);
void parseGo(Board& b, SearchInfo& si, std::string input);

#endif