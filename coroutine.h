#ifndef __COROUTINE__H__
#define __COROUTINE__H__

#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_RUNNING 2
#define COROUTINE_SUSPEND 3

struct schedule;

typedef void(*coroutine_func)(struct schedule *, void *ud);

struct schedule * coroutine_open(void);
void coroutine_close(struct schedule *);

int coroutine_new(struct schedule *, coroutine_func, void *ud);
void coroutine_resume(struct schedule *, int id);
int coroutine_status(struct schedule *, int id);
int coroutine_running(struct schedule *);
void coroutine_yield(struct schedule *);

#define COROUTINE_DEBUG 0

#if COROUTINE_DEBUG
void* co_malloc_ex(int size, int line, const char* func);
void co_free_ex(void* p, int line);
void co_print_memory();

#define co_malloc(size) co_malloc_ex(size, __LINE__, __FUNCTION__)
#define co_free(p) co_free_ex(p, __LINE__)
#else
#define co_malloc malloc
#define co_free free
#define co_print_memory()
#endif

#endif