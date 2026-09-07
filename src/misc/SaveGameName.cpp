/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <misc/SaveGameName.h>

#include <misc/string_util.h>

#include <cctype>

namespace {

bool isForbiddenFilenameCharacter(char c) {
    return c == '?' || c == '*' || c == ':' || c == '|' || c == '<'
        || c == '>' || c == '/' || c == '\\' || c == '"';
}

std::string trimFilenameEdges(std::string value) {
    const auto first = value.find_first_not_of(" .");
    if(first == std::string::npos) return "";

    const auto last = value.find_last_not_of(" .");
    return value.substr(first, last - first + 1);
}

std::string sanitizeContext(const std::string& context) {
    std::string result;
    bool pendingSpace = false;

    for(char c : context) {
        const auto byte = static_cast<unsigned char>(c);
        if(byte < 0x20 || (byte < 0x80 && std::isspace(byte)) || isForbiddenFilenameCharacter(c)) {
            pendingSpace = !result.empty();
            continue;
        }

        if(pendingSpace) {
            result += ' ';
            pendingSpace = false;
        }
        result += c;
    }

    return trimFilenameEdges(result);
}

std::string makeCandidate(const std::string& context, const std::string& timestamp,
                          const std::string& suffix) {
    const std::string separator = " - ";
    const size_t fixedLength = utf8Length(separator) + utf8Length(timestamp) + utf8Length(suffix);
    const size_t contextLength = fixedLength < SaveGameName::MAX_NAME_LENGTH
        ? SaveGameName::MAX_NAME_LENGTH - fixedLength
        : 0;

    std::string shortenedContext = trimFilenameEdges(utf8Substr(context, 0, contextLength));
    if(shortenedContext.empty()) shortenedContext = "Save";

    return shortenedContext + separator + timestamp + suffix;
}

} // namespace

std::string SaveGameName::create(const std::string& context, const std::tm& localTime,
                                 const std::function<bool(const std::string&)>& exists) {
    char timestampBuffer[20] = {};
    if(std::strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%d %H-%M-%S", &localTime) == 0) {
        return "";
    }

    std::string safeContext = sanitizeContext(context);
    if(safeContext.empty()) safeContext = "Save";

    std::string candidate = makeCandidate(safeContext, timestampBuffer, "");
    if(!exists(candidate)) return candidate;

    for(unsigned int index = 2; ; ++index) {
        const std::string suffix = " (" + std::to_string(index) + ")";
        candidate = makeCandidate(safeContext, timestampBuffer, suffix);
        if(!exists(candidate)) return candidate;
    }
}
