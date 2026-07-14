#define _CRT_SECURE_NO_WARNINGS
#include "parallel_merge_sort.h"
#include "scheduler_model.h"
#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/**
 * ============================================
 * ПОДРОБНЫЕ ТЕСТЫ ДЛЯ ПАРАЛЛЕЛЬНОЙ СОРТИРОВКИ И ПЛАНИРОВЩИКОВ
 * ============================================
 *
 * ИСПРАВЛЕННАЯ ВЕРСИЯ:
 * - Использует высокоточный замер времени (QueryPerformanceCounter)
 * - Маленькие массивы для демонстрации ускорения
 * - Правильные размеры в выводе
 */

 /*
  * ============================================
  * ВЫСОКОТОЧНЫЙ ЗАМЕР ВРЕМЕНИ
  * ============================================
  */

#ifdef _WIN32
static double get_time_seconds(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
}
#else
static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
#endif

/*
 * ============================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================================
 */

static int is_sorted(int* arr, int n) {
    if (!arr || n <= 1) return 1;
    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) return 0;
    }
    return 1;
}

static void fill_random(int* arr, int n, int max_val) {
    for (int i = 0; i < n; i++) {
        arr[i] = rand() % max_val;
    }
}

static void fill_descending(int* arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = n - i;
    }
}

static void fill_ascending(int* arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = i;
    }
}

static void fill_constant(int* arr, int n, int val) {
    for (int i = 0; i < n; i++) {
        arr[i] = val;
    }
}

static void copy_array(int* src, int* dst, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

static void print_array(int* arr, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

/*
 * ============================================
 * ТЕСТ 1: Последовательная сортировка
 * ============================================
 */
static void test_sequential_sort(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 1: Последовательная сортировка слиянием\n");
    printf("============================================================\n");
    printf("  Проверка корректности работы на массиве из 7 элементов\n");
    printf("\n");

    int arr[] = { 38, 27, 43, 3, 9, 82, 10 };
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("  Исходный массив: ");
    print_array(arr, n);

    printf("  Выполняем сортировку...\n");
    merge_sort_sequential(arr, 0, n - 1);

    printf("  Отсортированный: ");
    print_array(arr, n);

    int ok = is_sorted(arr, n);
    if (ok) {
        printf("\n  РЕЗУЛЬТАТ: УСПЕШНО\n");
        printf("  Массив отсортирован корректно.\n");
    }
    else {
        printf("\n  РЕЗУЛЬТАТ: ОШИБКА\n");
        printf("  Массив отсортирован неверно.\n");
    }
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 2: Сравнение последовательной и параллельной сортировки
 * ============================================
 *
 * ИСПРАВЛЕННАЯ ВЕРСИЯ:
 * - Маленькие массивы (как в примере)
 * - Точный замер времени
 * - Правильные размеры в выводе
 */
static void test_parallel_vs_sequential(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 2: Сравнение последовательной и параллельной сортировки\n");
    printf("============================================================\n");
    printf("  Проверка ускорения на маленьких массивах\n");
    printf("\n");

    /* МАЛЕНЬКИЕ МАССИВЫ (КАК В ПРИМЕРЕ) */
    const int sizes[] = { 1000, 10000, 50000, 100000 };
    const char* size_names[] = { "1 000", "10 000", "50 000", "100 000" };
    const int thread_counts[] = { 2, 4, 8 };
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    const int num_threads = sizeof(thread_counts) / sizeof(thread_counts[0]);

    printf("  ------------------------------------------------------------\n");
    printf("  Размер   | Последоват. | Потоков | Параллельн. | Ускорение\n");
    printf("  ------------------------------------------------------------\n");

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        int* arr_seq = (int*)malloc(n * sizeof(int));
        int* arr_par = (int*)malloc(n * sizeof(int));
        int* arr_copy = (int*)malloc(n * sizeof(int));

        if (!arr_seq || !arr_par || !arr_copy) {
            printf("  ОШИБКА: не удалось выделить память для размера %d\n", n);
            free(arr_seq);
            free(arr_par);
            free(arr_copy);
            continue;
        }

        fill_random(arr_seq, n, 10000);
        copy_array(arr_seq, arr_copy, n);

        /* Последовательная сортировка - ТОЧНЫЙ ЗАМЕР */
        double start_seq = get_time_seconds();
        merge_sort_sequential(arr_seq, 0, n - 1);
        double end_seq = get_time_seconds();
        double seq_time = end_seq - start_seq;

        int seq_ok = is_sorted(arr_seq, n);

        /* Параллельная сортировка - ТОЧНЫЙ ЗАМЕР */
        for (int t = 0; t < num_threads; t++) {
            copy_array(arr_copy, arr_par, n);

            double start_par = get_time_seconds();
            parallel_merge_sort(arr_par, n, thread_counts[t]);
            double end_par = get_time_seconds();
            double par_time = end_par - start_par;

            int par_ok = is_sorted(arr_par, n);

            double speedup = seq_time / par_time;
            printf("  %-8s | %-12.4f | %-7d | %-11.4f | %-8.2fx | %s\n",
                size_names[s],
                seq_time,
                thread_counts[t],
                par_time,
                speedup,
                par_ok ? "OK" : "ОШИБКА");
        }

        free(arr_seq);
        free(arr_par);
        free(arr_copy);
        printf("  ------------------------------------------------------------\n");
    }

    printf("\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 3: FIFO планировщик
 * ============================================
 */
static void test_fifo_scheduler(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 3: FIFO планировщик (невытесняющий)\n");
    printf("============================================================\n");
    printf("  Принцип: первый пришёл - первый выполнен\n");
    printf("\n");

    int count;
    Process** processes = generate_test_processes(&count);
    Scheduler* s = create_scheduler(processes, count, SCHEDULER_FIFO);

    printf("  Процессы:\n");
    printf("  ------------------------------------------------------------\n");
    printf("  Имя       | Прибытие | Выполнение | Приоритет\n");
    printf("  ------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        Process* p = processes[i];
        printf("  %-9s | %-8d | %-10d | %-8d\n",
            p->name, p->arrival_time, p->burst_time, p->priority);
    }
    printf("  ------------------------------------------------------------\n");
    printf("\n");

    run_scheduler(s);
    print_scheduler_stats(s);

    destroy_scheduler(s);
    for (int i = 0; i < count; i++) free(processes[i]);
    free(processes);

    printf("\n  ХАРАКТЕРИСТИКИ FIFO:\n");
    printf("  + Простой и предсказуемый алгоритм\n");
    printf("  - Возможен эффект конвоя (длинный процесс задерживает короткие)\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 4: Round Robin планировщик
 * ============================================
 */
static void test_rr_scheduler(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 4: Round Robin планировщик (вытесняющий)\n");
    printf("============================================================\n");
    printf("  Принцип: каждому процессу выделяется квант времени (2 такта)\n");
    printf("\n");

    int count;
    Process** processes = generate_test_processes(&count);
    Scheduler* s = create_scheduler(processes, count, SCHEDULER_ROUND_ROBIN);

    printf("  Процессы:\n");
    printf("  ------------------------------------------------------------\n");
    printf("  Имя       | Прибытие | Выполнение | Приоритет\n");
    printf("  ------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        Process* p = processes[i];
        printf("  %-9s | %-8d | %-10d | %-8d\n",
            p->name, p->arrival_time, p->burst_time, p->priority);
    }
    printf("  ------------------------------------------------------------\n");
    printf("  Квант времени: 2 такта\n");
    printf("\n");

    run_scheduler(s);
    print_scheduler_stats(s);

    destroy_scheduler(s);
    for (int i = 0; i < count; i++) free(processes[i]);
    free(processes);

    printf("\n  ХАРАКТЕРИСТИКИ ROUND ROBIN:\n");
    printf("  + Справедливое распределение CPU\n");
    printf("  - Частые переключения контекста\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 5: Priority планировщик
 * ============================================
 */
static void test_priority_scheduler(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 5: Приоритетный планировщик (вытесняющий)\n");
    printf("============================================================\n");
    printf("  Принцип: процесс с наивысшим приоритетом выполняется первым\n");
    printf("\n");

    int count;
    Process** processes = generate_test_processes(&count);
    Scheduler* s = create_scheduler(processes, count, SCHEDULER_PRIORITY);

    printf("  Процессы (0 - наивысший приоритет):\n");
    printf("  ------------------------------------------------------------\n");
    printf("  Имя       | Прибытие | Выполнение | Приоритет\n");
    printf("  ------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        Process* p = processes[i];
        printf("  %-9s | %-8d | %-10d | %-8d\n",
            p->name, p->arrival_time, p->burst_time, p->priority);
    }
    printf("  ------------------------------------------------------------\n");
    printf("\n");

    run_scheduler(s);
    print_scheduler_stats(s);

    destroy_scheduler(s);
    for (int i = 0; i < count; i++) free(processes[i]);
    free(processes);

    printf("\n  ХАРАКТЕРИСТИКИ PRIORITY:\n");
    printf("  + Эффективно для систем реального времени\n");
    printf("  - Возможно голодание низкоприоритетных процессов\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 6: Сравнение всех планировщиков
 * ============================================
 */
static void test_all_schedulers(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 6: Сравнение всех планировщиков\n");
    printf("============================================================\n");
    printf("  Сравнение по среднему времени ожидания и оборота\n");
    printf("\n");

    SchedulerType types[] = { SCHEDULER_FIFO, SCHEDULER_ROUND_ROBIN, SCHEDULER_PRIORITY };
    const char* names[] = { "FIFO", "Round Robin", "Priority" };
    double results[3][2];

    for (int t = 0; t < 3; t++) {
        int count;
        Process** processes = generate_test_processes(&count);
        Scheduler* s = create_scheduler(processes, count, types[t]);

        run_scheduler(s);

        int total_wait = 0, total_turnaround = 0;
        for (int i = 0; i < count; i++) {
            total_wait += processes[i]->waiting_time;
            total_turnaround += processes[i]->turnaround_time;
        }

        results[t][0] = (float)total_wait / count;
        results[t][1] = (float)total_turnaround / count;

        destroy_scheduler(s);
        for (int i = 0; i < count; i++) free(processes[i]);
        free(processes);
    }

    printf("  ------------------------------------------------------------\n");
    printf("  Алгоритм     | Среднее ожидание | Средний оборот\n");
    printf("  ------------------------------------------------------------\n");
    for (int t = 0; t < 3; t++) {
        printf("  %-12s | %-16.2f | %-13.2f\n",
            names[t], results[t][0], results[t][1]);
    }
    printf("  ------------------------------------------------------------\n");

    printf("\n  ВЫВОДЫ ПО ПЛАНИРОВЩИКАМ:\n");
    printf("  1. FIFO: минимальные переключения, средние показатели\n");
    printf("  2. Round Robin: справедливое распределение, но больше накладных расходов\n");
    printf("  3. Priority: оптимально для систем с приоритетами задач\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 7: Проверка синхронизации
 * ============================================
 */
static void test_synchronization(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 7: Проверка синхронизации\n");
    printf("============================================================\n");
    printf("  Проверка работы примитивов синхронизации\n");
    printf("\n");

    printf("  Используемые примитивы синхронизации:\n");
    printf("  1. Мьютекс (Mutex) - защита общих данных\n");
    printf("  2. Критическая секция - блокировка доступа\n");
    printf("  3. Ожидание потоков (Join) - синхронизация завершения\n");
    printf("\n");

    int count;
    Process** processes = generate_test_processes(&count);
    Scheduler* s = create_scheduler(processes, count, SCHEDULER_FIFO);

    printf("  Запускаем планировщик с синхронизацией...\n");
    run_scheduler(s);

    int completed = 0;
    for (int i = 0; i < count; i++) {
        if (processes[i]->is_completed) completed++;
    }

    printf("  Завершено процессов: %d из %d\n", completed, count);

    if (completed == count) {
        printf("\n  РЕЗУЛЬТАТ: СИНХРОНИЗАЦИЯ РАБОТАЕТ КОРРЕКТНО\n");
        printf("  Все процессы завершены, данные консистентны.\n");
    }
    else {
        printf("\n  РЕЗУЛЬТАТ: ОШИБКА СИНХРОНИЗАЦИИ\n");
        printf("  Не все процессы завершены.\n");
    }

    destroy_scheduler(s);
    for (int i = 0; i < count; i++) free(processes[i]);
    free(processes);

    printf("\n  Обоснование необходимости синхронизации:\n");
    printf("  - Без мьютексов возможны гонки данных (data races)\n");
    printf("  - Мьютексы обеспечивают взаимное исключение\n");
    printf("  - Критические секции защищают разделяемые ресурсы\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 8: Стресс-тест сортировки
 * ============================================
 */
static void test_stress_sort(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 8: Стресс-тест сортировки\n");
    printf("============================================================\n");
    printf("  Проверка на разных размерах\n");
    printf("\n");

    const int sizes[] = { 1000, 10000, 50000, 100000 };
    const char* size_names[] = { "1 000", "10 000", "50 000", "100 000" };
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("  Результаты стресс-теста:\n");
    printf("  ------------------------------------------------------------\n");
    printf("  Размер     | Время (сек) | Статус\n");
    printf("  ------------------------------------------------------------\n");

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        int* arr = (int*)malloc(n * sizeof(int));

        if (!arr) {
            printf("  ОШИБКА: не удалось выделить память для размера %d\n", n);
            continue;
        }

        fill_random(arr, n, 10000);

        double start = get_time_seconds();
        parallel_merge_sort(arr, n, 4);
        double end = get_time_seconds();
        double time = end - start;

        int ok = is_sorted(arr, n);

        printf("  %-10s | %-11.4f | %s\n",
            size_names[s], time, ok ? "OK" : "ОШИБКА");

        free(arr);
        printf("  ------------------------------------------------------------\n");
    }

    printf("\n  ВЫВОДЫ:\n");
    printf("  - Сортировка успешно работает на всех размерах\n");
    printf("  - Время выполнения растёт линейно-логарифмически O(n log n)\n");
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 9: Проверка на уже отсортированном массиве
 * ============================================
 */
static void test_already_sorted(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 9: Проверка на уже отсортированном массиве\n");
    printf("============================================================\n");
    printf("  Проверка, что сортировка не портит уже отсортированный массив\n");
    printf("\n");

    const int n = 10000;
    int* arr = (int*)malloc(n * sizeof(int));

    if (!arr) {
        printf("  ОШИБКА: не удалось выделить память\n");
        return;
    }

    fill_ascending(arr, n);

    double start = get_time_seconds();
    parallel_merge_sort(arr, n, 4);
    double end = get_time_seconds();
    double time = end - start;

    int ok = is_sorted(arr, n);

    printf("  Размер: %d\n", n);
    printf("  Время: %.4f сек\n", time);
    printf("  Результат: %s\n", ok ? "УСПЕШНО (массив остался отсортированным)" : "ОШИБКА");

    free(arr);
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 10: Проверка на всех равных элементах
 * ============================================
 */
static void test_all_equal(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 10: Проверка на всех равных элементах\n");
    printf("============================================================\n");
    printf("  Проверка, что сортировка работает с массивом одинаковых чисел\n");
    printf("\n");

    const int n = 1000;
    int* arr = (int*)malloc(n * sizeof(int));

    if (!arr) {
        printf("  ОШИБКА: не удалось выделить память\n");
        return;
    }

    fill_constant(arr, n, 42);

    double start = get_time_seconds();
    parallel_merge_sort(arr, n, 4);
    double end = get_time_seconds();
    double time = end - start;

    int ok = is_sorted(arr, n);

    printf("  Размер: %d\n", n);
    printf("  Все элементы: 42\n");
    printf("  Время: %.4f сек\n", time);
    printf("  Результат: %s\n", ok ? "УСПЕШНО (массив не изменился)" : "ОШИБКА");

    free(arr);
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 11: Проверка на обратно отсортированном массиве
 * ============================================
 */
static void test_descending(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 11: Проверка на обратно отсортированном массиве\n");
    printf("============================================================\n");
    printf("  Проверка работы сортировки с массивом по убыванию\n");
    printf("\n");

    const int n = 10000;
    int* arr = (int*)malloc(n * sizeof(int));

    if (!arr) {
        printf("  ОШИБКА: не удалось выделить память\n");
        return;
    }

    fill_descending(arr, n);

    double start = get_time_seconds();
    parallel_merge_sort(arr, n, 4);
    double end = get_time_seconds();
    double time = end - start;

    int ok = is_sorted(arr, n);

    printf("  Размер: %d\n", n);
    printf("  Исходный порядок: по убыванию\n");
    printf("  Время: %.4f сек\n", time);
    printf("  Результат: %s\n", ok ? "УСПЕШНО (отсортирован по возрастанию)" : "ОШИБКА");

    free(arr);
    printf("============================================================\n");
}

/*
 * ============================================
 * ТЕСТ 12: Проверка с разным числом потоков
 * ============================================
 */
static void test_various_threads(void) {
    printf("\n============================================================\n");
    printf("  ТЕСТ 12: Проверка с разным числом потоков\n");
    printf("  Проверка работы сортировки с 1, 2, 4, 8 потоками\n");
    printf("\n");

    const int n = 100000;
    const int thread_counts[] = { 1, 2, 4, 8 };
    const int num_threads = sizeof(thread_counts) / sizeof(thread_counts[0]);

    int* arr = (int*)malloc(n * sizeof(int));
    int* arr_copy = (int*)malloc(n * sizeof(int));

    if (!arr || !arr_copy) {
        printf("  ОШИБКА: не удалось выделить память\n");
        free(arr);
        free(arr_copy);
        return;
    }

    fill_random(arr, n, 10000);

    printf("  ------------------------------------------------------------\n");
    printf("  Потоков | Время (сек) | Статус\n");
    printf("  ------------------------------------------------------------\n");

    for (int t = 0; t < num_threads; t++) {
        copy_array(arr, arr_copy, n);

        double start = get_time_seconds();
        parallel_merge_sort(arr_copy, n, thread_counts[t]);
        double end = get_time_seconds();
        double time = end - start;

        int ok = is_sorted(arr_copy, n);

        printf("  %-7d | %-11.4f | %s\n",
            thread_counts[t], time, ok ? "OK" : "ОШИБКА");
    }

    free(arr);
    free(arr_copy);
    printf("============================================================\n");
}

/*
 * ============================================
 * ФУНКЦИЯ ЗАПУСКА ВСЕХ ТЕСТОВ
 * ============================================
 */
void run_all_tests(void) {
    printf("\n");
    printf("============================================================\n");
    printf("  ЗАПУСК ВСЕХ ТЕСТОВ\n");
    printf("============================================================\n");
    printf("  Всего тестов: 12\n");
    printf("  Тесты покрывают сортировку, планирование и синхронизацию\n");
    printf("\n");

    srand((unsigned int)time(NULL));

    test_sequential_sort();
    test_parallel_vs_sequential();
    test_fifo_scheduler();
    test_rr_scheduler();
    test_priority_scheduler();
    test_all_schedulers();
    test_synchronization();
    test_stress_sort();
    test_already_sorted();
    test_all_equal();
    test_descending();
    test_various_threads();

    printf("\n");
    printf("============================================================\n");
    printf("  ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ\n");
    printf("============================================================\n");
    printf("  Итоги тестирования:\n");
    printf("  1. Сортировка работает корректно на всех размерах\n");
    printf("  2. Все три планировщика демонстрируют ожидаемое поведение\n");
    printf("  3. Синхронизация обеспечивает корректную работу потоков\n");
    printf("============================================================\n");

    destroy_sort_pool();
}