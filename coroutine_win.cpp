
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <assert.h>
#include "coroutine.h"
#if COROUTINE_DEBUG
#include <map>
#endif

/* windows fiber版本的协程yield的时候直接切换到主协程(main)，
而不是swapcontext的切换到上次运行协程,但最后达到的结果却一样
*/

// 默认容量
#define DEFAULT_CAP   16
// 堆栈大小
#define INIT_STACK    (1024*1024)

typedef struct schedule schedule;
typedef struct coroutine coroutine;
typedef struct coroutine_para coroutine_para;

struct schedule
{
	int  cap;     // 容量
	int  conums;
	int  curID;   // 当前协程ID
	LPVOID    main;
	coroutine **co;
};

struct coroutine
{
	schedule  *s;
	void      *ud;
	int       status;
	LPVOID    ctx;
	coroutine_func func;
};

static int co_putin(schedule *s, coroutine *co)
{
	if (s->conums >= s->cap)
	{
		int id = s->cap;
		s->co = (coroutine**)realloc(s->co, sizeof(coroutine *) * s->cap * 2);
		memset(&s->co[s->cap], 0, sizeof(coroutine *) * s->cap);
		s->co[s->cap] = co;
		s->cap *= 2;
		++s->conums;
		return id;
	}
	else
	{
		for (int i = 0; i < s->cap; i++)
		{
			int id = (i + s->conums) % s->cap;
			if (s->co[id] == NULL)
			{
				s->co[id] = co;
				++s->conums;
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

static void co_delete(coroutine *co)
{
	//If the currently running fiber calls DeleteFiber, its thread calls ExitThread and terminates.
	//However, if a currently running fiber is deleted by another fiber, the thread running the 
	//deleted fiber is likely to terminate abnormally because the fiber stack has been freed.
	DeleteFiber(co->ctx);
	co_free(co);
}

schedule *coroutine_open()
{
	schedule *s = (schedule*)co_malloc(sizeof(schedule));
	s->cap = DEFAULT_CAP;
	s->conums = 0;
	s->curID = -1;
	s->co = (coroutine **)co_malloc(sizeof(coroutine *) * s->cap);
	memset(s->co, 0, sizeof(coroutine *) * s->cap);
	s->main = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	return s;
}

void coroutine_close(schedule *s)
{
	for (int i = 0; i < s->cap; i++)
	{
		coroutine *co = s->co[i];
		if (co) co_delete(co);
	}
	co_free(s->co);
	s->co = NULL;
	co_free(s);
}

void __stdcall coroutine_main(LPVOID lpParameter)
{
	schedule* s = (schedule*)lpParameter;
	int id = s->curID;
	coroutine *co = s->co[id];

	(co->func)(s, co->ud);

	co_free(co);
	s->curID = -1;
	--s->conums;
	s->co[id] = NULL;

	SwitchToFiber(s->main);
}

int coroutine_new(schedule *s, coroutine_func func, void *ud)
{
	coroutine *co = (coroutine*)co_malloc(sizeof(coroutine));
	co->s = s;
	co->status = COROUTINE_READY;
	co->func = func;
	co->ud = ud;
	int id = co_putin(s, co);
	co->ctx = CreateFiberEx(INIT_STACK, 0, FIBER_FLAG_FLOAT_SWITCH, coroutine_main, s);
	co->status = COROUTINE_READY;

	return id;
}

void coroutine_resume(schedule *s, int id)
{
	assert(id >= 0 && id < s->cap);
	if (id < 0 || id >= s->cap) return;
	coroutine *co = s->co[id];
	if (co == NULL) return;
	switch (co->status)
	{
	case COROUTINE_READY:case COROUTINE_SUSPEND:
		co->status = COROUTINE_RUNNING;
		s->curID = id;
		SwitchToFiber(co->ctx);
		//if (!s->co[id]) co_delete(co);
		break;
	default:
		assert(0);
		break;
	}
}

void coroutine_yield(schedule *s)
{
	int id = s->curID;
	assert(id >= 0 && id < s->cap);
	if (id < 0) return;

	coroutine *co = s->co[id];
	co->status = COROUTINE_SUSPEND;
	s->curID = -1;

	SwitchToFiber(s->main);
}

int coroutine_status(schedule *s, int id)
{
	assert(id >= 0 && id < s->cap);
	if (id < 0) return COROUTINE_DEAD;// fc change
	if (s->co[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return s->co[id]->status;
}

int coroutine_running(schedule *s)
{
	return s->curID;
}


#if COROUTINE_DEBUG
struct co_mem_info
{
	int line;
	int size;
	char func[256];
};
std::map<void*, co_mem_info*> co_mem_map;
void* co_malloc_ex(int size, int line, const char* func)
{
	void* p = malloc(size);
	int infosize = sizeof(co_mem_info);
	co_mem_info* info = (co_mem_info*)malloc(sizeof(co_mem_info));
	info->size = size;
	info->line = line;
	strcpy(info->func, func);
	printf("%p line:%d\tfunction:%s\n", p, line, func);

	co_mem_map.insert(std::make_pair(p, info));

	return p;
}

void co_free_ex(void* p, int line)
{
	std::map<void*, co_mem_info*>::iterator it = co_mem_map.find(p);
	if (it != co_mem_map.end())
	{
		free(it->second);
		printf("free\t%p\t%d\n", it->first, line);
		co_mem_map.erase(it);
	}
	free(p);
}

void co_print_memory()
{
	std::map<void*, co_mem_info*>::iterator it = co_mem_map.begin();
	for (; it != co_mem_map.end(); ++it)
	{
		printf("%p size:%d\tline:%d\tfunction:%s\n", it->first, it->second->size, it->second->line, it->second->func);
	}
}

#endif