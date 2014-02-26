#ifndef _EVENT_H_
#define _EVENT_H_

#define TASK_COUNT 10

#define MAX(m, n) ((m)<(n) ? (n) : (m))
#define MIN(m, n) (MAX((m), (n)) == (m) ? (n) : (m))
#define FD_READ 0x1
#define FD_WRITE 0x2
#define FD_EXCEPTION 0x3

typedef enum {
    SIG_LOOP= 0,
    SIG_ERROR = 1,
    SIG_COUNT = 2,
} sig_t;

typedef void (* event_func_t)(void *o);
typedef void (* sig_func_t)(sig_t s, void *o);
typedef void (* fd_func_t)(int fd, void *o);

typedef struct sig_table_t {
    struct sig_table_t *next;
    sig_t signal;
    sig_func_t func;
    void *data;
} sig_table_t;

typedef struct fd_t {
    struct fd_t *next;
    int fd;
    int type;
    int stamp;
    fd_func_t func;
    void *data;
} fd_t;

typedef struct event_t {
    struct event_t *next;
    int stamp;
    event_func_t func;
    void *data;
} event_t;

typedef void (* e_funct_t  )(void *);

int signal_register(sig_t signal, sig_func_t func, void *data);
void signal_set(sig_t signal);
int event_add(event_func_t func, void *data);
int event_add_once(event_func_t func, void *data);
void event_del_once(void *data);
void event_del_all(void *data);
int fd_add(int fd, int type, fd_func_t func, void *data);
void fd_del(int fd, int type, fd_func_t func, void *data);
int event_loop(void);
void event_init(void);
void event_uninit(void);

#endif
