// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/skip_set.h>

#include <logging.h>

bool CSkipSet::add(uint64_t value)
{
    assert(!contains(value));

    if (auto it = skipped.find(value); it != skipped.end()) {
        skipped.erase(it);
        return true;
    }

    assert(current_max <= value);
    if (capacity() + value - current_max > capacity_limit) {
        LogPrintf("CSkipSet::add failed due to capacity exceeded: requested %lld to %lld while limit is %lld\n",
                value - current_max, capacity(), capacity_limit);
        return false;
    }
    for (uint64_t index = current_max; index < value; ++index) {
        bool insert_ret = skipped.insert(index).second;
        assert(insert_ret);
    }
    current_max = value + 1;
    return true;
}

bool CSkipSet::canBeAdded(uint64_t value) const
{
    if (contains(value)) return false;

    if (skipped.find(value) != skipped.end()) return true;

    if (capacity() + value - current_max > capacity_limit) {
        return false;
    }

    return true;
}

bool CSkipSet::contains(uint64_t value) const
{
    if (current_max <= value) return false;
    return skipped.find(value) == skipped.end();
}

