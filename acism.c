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
#define BACK ((SYMBOL)0)
#define ROOT ((STATE) 0)

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

static inline int isEnd(char const * cp, char const * endp, const int is_reverse, const int cp_increment) 
{
    return (((cp + is_reverse) - endp) * cp_increment) >= 0;
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
    char const *cp = textStart(text, is_reverse);
    char const *endp = textEnd(text, is_reverse);
    const int cp_increment = is_reverse ? -1 : 1;
    STATE state = *statep;
    int ret = 0;

    if (0 == (psp->flags & ACFLAG_ANCHORED)) {
        while (!isEnd(cp, endp, is_reverse, cp_increment)) {
            _SYMBOL sym = psp->symv[(uint8_t)*cp];
            cp += cp_increment;
            if (!sym) {
                // Input byte is not in any pattern string.
                state = ROOT;
                continue;
            }
    
            // Search for a valid transition from this (state, sym),
            //  following the backref chain.
    
            TRAN next;
            while (!t_valid(psp, next = p_tran(psp, state, sym)) && state != ROOT) {
                TRAN back = p_tran(psp, state, BACK);
                state = t_valid(psp, back) ? t_next(psp, back) : ROOT;
            }
    
            if (!t_valid(psp, next))
                continue;
    
            if (!(next & (IS_MATCH | IS_SUFFIX))) {
                // No complete match yet; keep going.
                state = t_next(psp, next);
                continue;
            }
    
            // At this point, one or more patterns have matched.
            // Find all matches by following the backref chain.
            // A valid node for (sym) with no SUFFIX flag marks the
            //  end of the suffix chain.
            // In the same backref traversal, find a new (state),
            //  if the original transition is to a leaf.
    
            STATE s = state;
    
            // Initially state is ROOT. The chain search saves the
            //  first state from which the next char has a transition.
            state = t_isleaf(psp, next) ? 0 : t_next(psp, next);
    
            while (1) {
    
                if (t_valid(psp, next)) {
    
                    if (next & IS_MATCH) {
                        unsigned strno, ss = s + sym, i;
                        if (t_isleaf(psp, psp->tranv[ss])) {
                            strno = t_strno(psp, psp->tranv[ss]);
                        } else {
                            for (i = p_hash(psp, ss); psp->hashv[i].state != ss; ++i);
                            strno = psp->hashv[i].strno;
                        }
    
                        if ((ret = cb(strno, matchDistance(cp, text, is_reverse), context)))
                            goto EXIT;
                    }
    
                    if (!state && !t_isleaf(psp, next))
                        state = t_next(psp, next);
                    if ( state && !(next & IS_SUFFIX))
                        break;
                }
    
                if (s == ROOT)
                    break;
    
                TRAN b = p_tran(psp, s, BACK);
                s = t_valid(psp, b) ? t_next(psp, b) : ROOT;
                next = p_tran(psp, s, sym);
            }
        }
    } else {
        while (!isEnd(cp, endp, is_reverse, cp_increment)) {
            _SYMBOL sym = psp->symv[(uint8_t)*cp];
            cp += cp_increment;
            TRAN next;
            if (!sym || !t_valid(psp, next = p_tran(psp, state, sym))) {
                // Input byte is not in any pattern string.
                break;
            }
            STATE s = state;
            state = t_isleaf(psp, next) ? 0 : t_next(psp, next);
            if (next & IS_MATCH) {
                unsigned strno, ss = s + sym, i;
                if (t_isleaf(psp, psp->tranv[ss])) {
                    strno = t_strno(psp, psp->tranv[ss]);
                } else {
                    for (i = p_hash(psp, ss); psp->hashv[i].state != ss; ++i);
                    strno = psp->hashv[i].strno;
                }

                if ((ret = cb(strno, matchDistance(cp, text, is_reverse), context)))
                    goto EXIT;
            }
        }
    }
EXIT:
    return *statep = state, ret;
}

int
default_on_match(int strnum, int textpos, void *c)
{
    (void)strnum, (void)textpos, (void)c;

    return strnum;
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
