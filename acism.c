/*
** Copyright (C) 2009-2014 Mischa Sandberg <mischasan@gmail.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License Version as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU Lesser General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "_acism.h"

static inline char const * 
textStart(MEMREF const text, int is_reverse) 
{
    if (!is_reverse) {
        return text.ptr;
    }
    return text.ptr + (text.len - 1);
}

static inline char const * 
textEnd(MEMREF const text, int is_reverse) 
{
    if (!is_reverse) {
        return text.ptr + text.len;
    }
    return text.ptr;
}

static inline int isEnd(char const * cp, char const * endp, int is_reverse, int cp_increment) 
{
    return ((cp - (endp + is_reverse)) * cp_increment) >= 0;
}

static inline int matchDistance(char const * cp, MEMREF const text, int is_reverse) 
{
    if (!is_reverse) {
        return (cp - text.ptr);
    }
    return (textStart(text, is_reverse) - cp);
}

static int 
run_match(ACISM const *psp, MEMREF const text, int is_reverse,
           ACISM_ACTION *cb, void *context, int *statep)
{
    ACISM const ps = *psp;
    char const *cp = textStart(text, is_reverse);
    char const *endp = textEnd(text, is_reverse);
    const int cp_increment = is_reverse ? -1 : 1;
    STATE state = *statep;

    while (!isEnd(cp, endp, is_reverse, cp_increment)) {
        _SYMBOL sym = ps.symv[(uint8_t)*cp];
	cp += cp_increment;
        if (!sym) {
            // Input byte is not in any pattern string.
            state = 0;
            continue;
        }

        // Search for a valid transition from this (state, sym),
        //  following the backref chain.

        TRAN next, back;
        while (!t_valid(&ps, next = p_tran(&ps, state, sym)) && state) {
            back = p_tran(&ps, state, 0);
            state = t_valid(&ps, back) ? t_next(&ps, back) : 0; // All states have an implicit backref to root.
        }

        if (!t_valid(&ps, next))
            continue;

        if (!(next & (IS_MATCH | IS_SUFFIX))) {
            // No complete match yet; keep going.
            state = t_next(&ps, next);
            continue;
        }

        // At this point, one or more patterns have matched.
        // Find all matches by following the backref chain.
        // A valid node for (sym) with no SUFFIX marks the
        //  end of the suffix chain.
        // In the same backref traversal, find a new (state),
        //  if the original transition is to a leaf.

        STATE s = state;
        state = t_isleaf(&ps, next) ? 0 : t_next(&ps, next);

        while (1) {

            if (t_valid(&ps, next)) {

                if (next & IS_MATCH) {
                    unsigned strno, ss = s + sym, i;
                    if (t_isleaf(&ps, ps.tranv[ss])) {
                        strno = t_strno(&ps, ps.tranv[ss]);
                    } else {
                        for (i = p_hash(&ps, ss); ps.hashv[i].state != ss; ++i);
                        strno = ps.hashv[i].strno;
                    }

                    int ret = cb(strno, matchDistance(cp, text, is_reverse), context);
                    if (ret)
                        return *statep = state, ret;
                }

                if (!state && !t_isleaf(&ps, next))
                    state = t_next(&ps, next);
                if (state && !(next & IS_SUFFIX))
                    break;
            }

            if (!s)
                break;
            TRAN b = p_tran(&ps, s, 0);
            s = t_valid(&ps, b) ? t_next(&ps, b) : 0;
            next = p_tran(&ps, s, sym);
        }
    }

    return *statep = state, 0;
}

int
acism_more(ACISM const *psp, MEMREF const text,
           ACISM_ACTION *cb, void *context, int *statep)
{
    return run_match(psp, text, 0, cb, context, statep);
}

int
acism_more_reverse(ACISM const *psp, MEMREF const text,
           ACISM_ACTION *cb, void *context, int *statep)
{
    return run_match(psp, text, 1, cb, context, statep);
}

size_t
acism_size(ACISM const *psp)
{
    return sizeof(ACISM) + p_size(psp);
}
//EOF
