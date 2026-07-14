#ifndef PARALLEL_MERGE_SORT_H
#define PARALLEL_MERGE_SORT_H

#ifdef __cplusplus
extern "C" {
#endif

	/* Главная функция сортировки */
	void parallel_merge_sort(int* arr, int size, int max_threads);

	/* Последовательная сортировка (для сравнения) */
	void merge_sort_sequential(int* arr, int left, int right);

	/* Освобождение ресурсов (заглушка) */
	void destroy_sort_pool(void);

#ifdef __cplusplus
}
#endif

#endif /* PARALLEL_MERGE_SORT_H */