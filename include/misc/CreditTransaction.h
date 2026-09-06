/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef CREDITTRANSACTION_H
#define CREDITTRANSACTION_H

#include <fixmath/FixPoint.h>

namespace CreditTransaction {

inline bool tryTakeExact(FixPoint& storedCredits, FixPoint& startingCredits, FixPoint amount) {
    if(amount < 0 || (storedCredits + startingCredits) < amount) {
        return false;
    }

    if(storedCredits >= amount) {
        storedCredits -= amount;
    } else {
        const FixPoint remainder = amount - storedCredits;
        storedCredits = 0;
        startingCredits -= remainder;
    }
    return true;
}

} // namespace CreditTransaction

#endif // CREDITTRANSACTION_H
