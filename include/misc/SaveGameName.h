/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SAVEGAMENAME_H
#define SAVEGAMENAME_H

#include <cstddef>
#include <ctime>
#include <functional>
#include <string>

namespace SaveGameName {

constexpr std::size_t MAX_NAME_LENGTH = 64;

/**
    Creates a human-readable, filesystem-safe save name without an extension.
    The callback receives each candidate name and returns true when it already
    exists. The returned name is at most MAX_NAME_LENGTH UTF-8 code points.
*/
std::string create(const std::string& context, const std::tm& localTime,
                   const std::function<bool(const std::string&)>& exists);

} // namespace SaveGameName

#endif // SAVEGAMENAME_H
