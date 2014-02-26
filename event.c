#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "event.h"

typedef enum event_status_t {
    EVENT_CONTINUE = 0,
    EVENT_TERMINATE = 1,
    EVENT_ERROR = 2,
} event_status_t;

typedef unsigned long long sig_mask_t;

static int stamp_now;
static sig_mask_t signals;
static event_t *event_list, **event_list_tail;
static fd_t *fd_list;
static int fd_max;
static fd_set fd_read, fd_write, fd_excep;
static sig_table_t *signal_table[SIG_COUNT];

static sig_table_t *alloc_sig_table_t(sig_t signal, sig_func_t func, void *data)
{
    sig_table_t *s;

    if (!(s = malloc(sizeof(sig_table_t))))
	return NULL;

    s->next = NULL;
    s->signal = signal;
    s->func = func;
    s->data = data;
    return s;
}

static void free_sig_table_t(sig_table_t *s)
{
    free(s);
}

static fd_t *alloc_fd_t(int fd, int type, fd_func_t func, void *data)
{
    fd_t *efd;

    if (!(efd = malloc(sizeof(fd_t))))
	return NULL;

    efd->next = NULL;
    efd->fd = fd;
    efd->type = type;
    efd->stamp = stamp_now;
    efd->func = func;
    efd->data = data;
    return efd;
}

static void free_fd_t(fd_t *efd)
{
    free(efd);
}

static event_t *alloc_event_t(event_func_t func, void *data)
{
    event_t *e;

    if (!(e = malloc(sizeof(event_t))))
	return NULL;

    e->next = NULL;
    e->func = func;
    e->data = data;
    e->stamp = stamp_now;
    return e;
}

static void free_event_t(event_t *e)
{
    free(e);
}

/* registers a task on a signal */
int signal_register(sig_t signal, sig_func_t func, void *data)
{
    sig_table_t *s, **sptr = &(signal_table[signal]);

    if (!(s = alloc_sig_table_t(signal, func, data)))
	return -1;

    s->next = *sptr;
    *sptr = s;
    return 0;
}

void signal_set(sig_t s)
{
    signals |= (sig_mask_t)1 << s;
}

static void signal_calls(void)
{
    int s;

    for (s = 0; s < SIG_COUNT; s++)
    {
	sig_mask_t mask = 1 << s;
	sig_table_t *sptr;

	if (!(signals & mask))
	    continue;

	for (sptr = signal_table[s]; sptr; sptr = sptr->next)
	    sptr->func(s, sptr->data);
    }
    signals = 0;
}

/* cleanup the signal table */
static void signal_table_clean(void)
{
    int i;

    for (i = 0; i < SIG_COUNT; i++)
    {
	sig_table_t *sptr;

	while ((sptr = signal_table[i]))
	{
	    signal_table[i] = signal_table[i]->next;
	    free_sig_table_t(sptr);
	}
    }
}

/* adds an event to the end of the event list */
int event_add(event_func_t func, void *data)
{
    if (!(*event_list_tail = alloc_event_t(func, data)))
	return -1;

    event_list_tail = &(*event_list_tail)->next;
    return 0;
}

int event_add_once(event_func_t func, void *data)
{
    event_t *tmp;

    for (tmp = event_list; tmp && (tmp->func != func || tmp->data != data); 
	    tmp = tmp->next);
    return tmp ? 0 : event_add(func, data);
}

/* deletes an event from the event list */
static void event_del(void *data, int is_once)
{
    event_t **eptr = &event_list;

    while (*eptr)
    {
	event_t *etmp;

	if ((*eptr)->data != data)
	{
	    eptr = &(*eptr)->next;
	    continue;
	}

	etmp = *eptr;
	*eptr = (*eptr)->next;
	if (event_list_tail == &etmp->next)
	    event_list_tail = eptr;
	free_event_t(etmp);
	if (is_once)
	    return;
    }
}

void event_del_once(void *data)
{
    event_del(data, 1);
}

void event_del_all(void *data)
{
    event_del(data, 0);
}

static fd_set *fd_set_choose(int type, fd_set *rfd, fd_set *wfd, fd_set *efd)
{
    switch (type)
    {
    case FD_READ:
	return rfd;
    case FD_WRITE:
	return wfd;
    case FD_EXCEPTION:
	return efd;
    default:
	return NULL;
    }
}

static void fd_list_clean(void)
{
    fd_t *tmp;

    while ((tmp = fd_list))
    {
	fd_list = fd_list->next;
	free_fd_t(tmp);
    }
}

static int fd_equal(fd_t *efd, int fd, int type, fd_func_t func, void *data)
{
    return efd->fd==fd && efd->type==type && efd->func==func && efd->data==data;
}

int fd_add(int fd, int type, fd_func_t func, void *data)
{
    fd_t *efd, **eptr;
    fd_set *fds;

    if (type != FD_READ && type != FD_WRITE && type != FD_EXCEPTION)
    {
	if (((type & FD_READ) && fd_add(fd, FD_READ, func, data)) ||
	    ((type & FD_WRITE) && fd_add(fd, FD_WRITE, func, data)) ||
	    ((type & FD_EXCEPTION) && fd_add(fd, FD_EXCEPTION, func, data)))
	{
	    fd_max = 0;
	    for (eptr = &fd_list; *eptr; eptr = &(*eptr)->next)
	    {
		if ((*eptr)->fd == fd)
		{
		    efd = *eptr;
		    *eptr = (*eptr)->next;
		    free_fd_t(efd);
		}
		fd_max = MAX((*eptr)->fd, fd);
	    }
	    return -1;
	}
	return 0;
    }

    /* TODO if (!(efd = fd_find(fd, type, func, data)) && ...)*/
    if (!(efd = alloc_fd_t(fd, type, func, data)))
	return -1;

    if (!(fds = fd_set_choose(type, &fd_read, &fd_write, &fd_excep)))
    {
	free_fd_t(efd);
	return -1;
    }

    fd_max = MAX(fd, fd_max);
    if (!FD_ISSET(fd, fds))
	FD_SET(fd, fds);

    for (eptr = &fd_list; *eptr; eptr = &((*eptr)->next))
    {
	if (fd <= (*eptr)->fd)
	    break;
    }

    efd->next = *eptr;
    *eptr = efd;
    return 0;
}

void fd_del(int fd, int type, fd_func_t func, void *data)
{
    fd_t **efdp, *efd;
    fd_set *fds;

    for (efdp = &fd_list; *efdp && !fd_equal(*efdp, fd, type, func, data); 
	efdp = &((*efdp)->next));

    if (!*efdp)
	return;

    efd = *efdp;
    *efdp =(*efdp)->next;
    fds = fd_set_choose(efd->type, &fd_read, &fd_write, &fd_excep);
    FD_CLR(efd->fd, fds);
    free_fd_t(efd);
}

static int fd_call(void)
{
#define TV_SEC 0
#define TV_USEC 10

    fd_t **efdp;
    fd_set fdr, fdw, fde;
    struct timeval tv = {TV_SEC, TV_USEC};
    int n, fds_copy_sz;

    fds_copy_sz = MIN(fd_max / sizeof(char) + 1, sizeof(fd_set));
    FD_ZERO(&fdr);
    FD_ZERO(&fdw);
    FD_ZERO(&fde);
    memcpy(&fdr, &fd_read, fds_copy_sz);
    memcpy(&fdw, &fd_write, fds_copy_sz);
    memcpy(&fde, &fd_excep, fds_copy_sz);

    if ((n = select(fd_max + 1, &fdr, &fdw, &fde, &tv)) <= 0)
	return n;

    for (efdp = &fd_list; *efdp; efdp = &((*efdp)->next))
    {
	fd_t *efd;
	fd_set *fds;

	if (!(fds = fd_set_choose((*efdp)->type, &fd_read, &fd_write, 
	    &fd_excep)))
	{
	    return -1;
	}

	if (!FD_ISSET((*efdp)->fd, fds) || ((*efdp)->stamp == stamp_now))
	    continue;

	efd = *efdp;
	*efdp = (*efdp)->next;
	efd->func(efd->fd, efd->data);
	free_fd_t(efd);
    }
    return 0;

#undef TV_SEC
#undef TV_USEC
}

static event_status_t event_loop_once(void)
{
    event_t **eptr = &event_list;

    signal_calls();
    stamp_now++;
    while (*eptr)
    {
	event_t *etmp, event;

	if ((*eptr)->stamp == stamp_now)
	{
	    eptr = &((*eptr)->next);
	    continue;
	}

	etmp = *eptr;
	*eptr = (*eptr)->next;
	if (event_list_tail == &etmp->next)
	    event_list_tail = eptr;
	event = *etmp;
	free_event_t(etmp);
	event.func(event.data);
    }

    if (!event_list && !fd_list && !signals)
	return EVENT_TERMINATE;
    if (fd_call())
	return EVENT_ERROR;

    return EVENT_CONTINUE;
}

int event_loop(void)
{
    event_status_t res = EVENT_CONTINUE;
    event_t *e;

    while (res == EVENT_CONTINUE)
    {
	if ((res = event_loop_once()) == EVENT_ERROR)
	    goto Error;
    }
    return 0;

Error:
    while ((e = event_list))
    {
	event_list = event_list->next;
	free_event_t(e);
    }
    return -1;
}

void event_init(void)
{
    event_list_tail = &event_list;
}

void event_uninit(void)
{
    signal_table_clean();
    fd_list_clean();
}
