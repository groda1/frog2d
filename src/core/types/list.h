#ifndef LIST_H
#define LIST_H

/* List macros */

#define SLLPopFirst(first, next)                                                                   \
    do                                                                                             \
    {                                                                                              \
        if ((first))                                                                               \
        {                                                                                          \
            (first) = first->next;                                                                 \
        }                                                                                          \
    } while (0)

#define SLLInsertFirst(first, next, val)                                                           \
    do                                                                                             \
    {                                                                                              \
        val->next = first;                                                                         \
        first = val;                                                                               \
    } while (0)

#endif
