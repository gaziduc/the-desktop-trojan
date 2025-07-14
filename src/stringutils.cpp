//
// Created by Gazi on 7/10/2025.
//

#include "stringutils.h"

#include <string>


unsigned int StringUtils::getUtf8StringSize(const char* str) {
    unsigned int count = 0;
    while (*str != 0)
    {
        if ((*str & 0xc0) != 0x80)
            ++count;
        ++str;
    }
    return count;
}
