//
// Created by Gazi on 7/10/2025.
//

#ifndef STRINGUTILS_H
#define STRINGUTILS_H
#include <string>


class StringUtils {
public:
    static unsigned int get_utf8_str_length(const char* str);

    static std::string get_utf8_substr(const std::string& str, unsigned int start, unsigned int length);

    static void pop_back_utf8(std::string& utf8_str);
};



#endif //STRINGUTILS_H
