#ifndef MINI_TMUX_LIST_H
#define MINI_TMUX_LIST_H

#include <stddef.h>

/*
 * container_of - get the struct for this entry
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/*
 * doubly linked list
 */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/*
 * init a list head
 */
static inline void list_init(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * internal add
 */
static inline void __list_add(struct list_head *entry,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = entry;
    entry->next  = next;
    entry->prev  = prev;
    prev->next = entry;
}

/*
 * add entry after head
 */
static inline void list_add(struct list_head *entry,
                            struct list_head *head)
{
    __list_add(entry, head, head->next);
}

/*
 * add entry before head (tail)
 */
static inline void list_add_tail(struct list_head *entry,
                                 struct list_head *head)
{
    __list_add(entry, head->prev, head);
}

/*
 * internal delete
 */
static inline void __list_del(struct list_head *prev,
                              struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/*
 * delete entry
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

/*
 * check if list empty
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/*
 * iterate
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * iterate safe (allow delete)
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         pos != (head); \
         pos = n, n = pos->next)

/*
 * get entry
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/*
 * get first entry (NULL if empty)
 */
#define list_first_entry(head, type, member) \
    (list_empty(head) ? NULL : list_entry((head)->next, type, member))

/*
 * get last entry (NULL if empty)
 */
#define list_last_entry(head, type, member) \
    (list_empty(head) ? NULL : list_entry((head)->prev, type, member))

/*
 * iterate over each entry
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/*
 * iterate over each entry safe (allow delete)
 */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member), \
         n = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.next, typeof(*pos), member))

/*
 * count entries in list
 */
static inline size_t list_count(const struct list_head *head)
{
    size_t count = 0;
    struct list_head *pos;
    list_for_each(pos, head) {
        count++;
    }
    return count;
}

#endif /* MINI_TMUX_LIST_H */
