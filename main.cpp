#include <iostream>
#include "coroutine.h"

void test3(schedule *s, void *ud)
{
	int *data = (int*)ud;
	int** arr = (int**)malloc(sizeof(int*) * 3);
	for (int i = 0; i < 3; i++)
	{
		arr[i] = (int*)malloc(sizeof(int));
		printf("test3 i=%d\n", i);
		coroutine_yield(s);
		printf("yield co id = %d.\n", *data);
		free(arr[i]);
	}
	printf("finish-->");
}

void coroutine_test()
{
	printf("coroutine_test3 begin\n");
	schedule *s = coroutine_open();

	int a = 11;
	int id1 = coroutine_new(s, test3, &a);
	int id2 = coroutine_new(s, test3, &a);

	while (coroutine_status(s, id1) && coroutine_status(s, id2))
	{
		printf("\nresume co id = %d.\n", id1);
		coroutine_resume(s, id1);
		//printf("resume co id = %d.\n", id2);
		//coroutine_resume(s, id2);
	}

	int id3 = coroutine_new(s, test3, &a);
	while (coroutine_status(s, id3))
	{
		printf("\nresume co id = %d.\n", id3);
		coroutine_resume(s, id3);
	}

	printf("coroutine_test3 end\n");
	coroutine_close(s);
	co_print_memory();
}

void main()
{
	coroutine_test();
	system("pause");
}
