#pragma once

#include <string>
#include <glad/glad.h>

std::string preprocessString(std::string dest, const char* identifier, std::string source) {
    std::string result;
    size_t id_start, id_end;
    if ((id_start = dest.find(identifier, 0)) == std::string::npos)
        return dest;
    id_end = id_start + strlen(identifier);

    result.append(dest, 0, id_start);
    result.append(source);
    result.append(dest, id_end, dest.length());

    return result;
}