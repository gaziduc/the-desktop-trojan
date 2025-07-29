//
// Created by Gazi on 7/10/2025.
//

#include "stringutils.h"

#include <string>


unsigned int StringUtils::get_utf8_str_length(const char* str) {
    unsigned int count = 0;
    while (*str != 0)
    {
        if ((*str & 0xc0) != 0x80)
            ++count;
        ++str;
    }
    return count;
}

std::string StringUtils::get_utf8_substr(const std::string& str, unsigned int start, unsigned int length)
{
    if (length == 0)
        return "";

    unsigned int c, i, ix, q, min = std::string::npos, max = std::string::npos;
    for (q = 0, i = 0, ix = str.length(); i < ix; i++, q++)
    {
        if (q == start)
            min = i;
        if (q <= start + length || length == std::string::npos)
            max = i;

        c = (unsigned char) str[i];
        if (c <= 127)
            i += 0;
        else if ((c & 0xE0) == 0xC0)
            i += 1;
        else if ((c & 0xF0) == 0xE0)
            i += 2;
        else if ((c & 0xF8) == 0xF0)
            i += 3;
        else
            return "";
    }

    if (q <= start + length || length == std::string::npos)
        max = i;

    if (min == std::string::npos || max == std::string::npos)
        return "";

    return str.substr(min, max);
}

void StringUtils::pop_back_utf8(std::string& utf8_str)
{
    if (utf8_str.empty())
        return;

    auto cp = utf8_str.data() + utf8_str.size();
    while (--cp >= utf8_str.data() && ((*cp & 0b10000000) && !(*cp & 0b01000000))) {}
    if (cp >= utf8_str.data())
        utf8_str.resize(cp - utf8_str.data());
}
