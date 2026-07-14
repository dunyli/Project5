#define _CRT_SECURE_NO_WARNINGS
#include "parallel_merge_sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ============================================
 * ПРОТОТИПЫ ВНУТРЕННИХ ФУНКЦИЙ
 * ============================================ */

 /*
  * Функция для параллельного слияния двух массивов
  * Использует бинарный поиск для нахождения позиции элемента
  */
static void parallel_merge_impl(int* source, int left1, int right1, int left2, int right2,
    int* dest, int dest_left, int depth, int max_threads,
    int* active_threads, CRITICAL_SECTION* mutex);

/* Обёртка для потока слияния (Windows) */
#ifdef _WIN32
static DWORD WINAPI merge_thread_wrapper(LPVOID arg);
#endif

/* Обёртка для потока сортировки (Windows) */
#ifdef _WIN32
static DWORD WINAPI sort_thread_wrapper(LPVOID arg);
#endif

/* ============================================
 * ВСПОМОГАТЕЛЬНЫЕ СТРУКТУРЫ
 * ============================================ */

 /*
  * Структура для передачи параметров в поток слияния
  * Содержит все данные, необходимые для слияния двух частей массива
  */
typedef struct {
    int* source;        /* Исходный массив */
    int left1, right1;  /* Левая часть */
    int left2, right2;  /* Правая часть */
    int* dest;          /* Результирующий массив */
    int dest_left;      /* Начальная позиция в результирующем массиве */
    int depth;          /* Глубина рекурсии */
    int max_threads;    /* Максимальное число потоков */
    int* active_threads; /* Счётчик активных потоков */
    CRITICAL_SECTION* mutex; /* Мьютекс для синхронизации */
} MergeParams;

/*
 * Структура для передачи параметров в поток сортировки
 * Содержит данные для сортировки части массива
 */
typedef struct {
    int* arr;           /* Массив для сортировки */
    int left;           /* Левая граница */
    int right;          /* Правая граница */
    int depth;          /* Глубина рекурсии */
    int max_threads;    /* Максимальное число потоков */
    int* active_threads; /* Счётчик активных потоков */
    CRITICAL_SECTION* mutex; /* Мьютекс для синхронизации */
} SortParams;

/* ============================================
 * БИНАРНЫЙ ПОИСК
 * ============================================ */

 /*
  * Функция: find_insert_position
  * Назначение: находит количество элементов в массиве, которые меньше или равны value
  *
  * Алгоритм: классический бинарный поиск
  * Сложность: O(log n)
  *
  * Параметры:
  *   value - искомое значение
  *   arr   - массив для поиска
  *   left  - левая граница поиска
  *   right - правая граница поиска
  *
  * Возвращает: количество элементов <= value
  */
static int find_insert_position(int value, int* arr, int left, int right) {
    int low = left;     /* Нижняя граница поиска */
    int high = right;   /* Верхняя граница поиска */

    /* Пока границы не сошлись */
    while (low <= high) {
        /* Находим середину */
        int mid = low + (high - low) / 2;

        /* Если средний элемент <= value, ищем правее */
        if (arr[mid] <= value) {
            low = mid + 1;
        }
        else {
            /* Иначе ищем левее */
            high = mid - 1;
        }
    }

    /* Возвращаем количество элементов <= value */
    return low - left;
}

/* ============================================
 * ОБЫЧНОЕ (ПОСЛЕДОВАТЕЛЬНОЕ) СЛИЯНИЕ
 * ============================================ */

 /*
  * Функция: simple_merge
  * Назначение: последовательное слияние двух отсортированных частей массива
  *
  * Алгоритм: классическое слияние двух массивов
  * Сложность: O(n)
  *
  * Параметры:
  *   arr   - массив для слияния
  *   left  - левая граница первой части
  *   mid   - середина (конец первой части)
  *   right - правая граница второй части
  */
static void simple_merge(int* arr, int left, int mid, int right) {
    /* Вычисляем размеры левой и правой частей */
    int left_size = mid - left + 1;
    int right_size = right - mid;

    /* Выделяем память под временные массивы */
    int* left_part = (int*)malloc(left_size * sizeof(int));
    int* right_part = (int*)malloc(right_size * sizeof(int));

    /* Копируем данные во временные массивы */
    for (int i = 0; i < left_size; i++) {
        left_part[i] = arr[left + i];
    }
    for (int j = 0; j < right_size; j++) {
        right_part[j] = arr[mid + 1 + j];
    }

    /* Сливаем два временных массива обратно в исходный */
    int i = 0, j = 0, k = left;
    while (i < left_size && j < right_size) {
        if (left_part[i] <= right_part[j]) {
            arr[k++] = left_part[i++];
        }
        else {
            arr[k++] = right_part[j++];
        }
    }

    /* Копируем оставшиеся элементы из левой части */
    while (i < left_size) {
        arr[k++] = left_part[i++];
    }

    /* Копируем оставшиеся элементы из правой части */
    while (j < right_size) {
        arr[k++] = right_part[j++];
    }

    /* Освобождаем временные массивы */
    free(left_part);
    free(right_part);
}

/* ============================================
 * ПОСЛЕДОВАТЕЛЬНАЯ СОРТИРОВКА
 * ============================================ */

 /*
  * Функция: merge_sort_sequential
  * Назначение: рекурсивная последовательная сортировка слиянием
  *
  * Алгоритм: классическая сортировка слиянием
  * Сложность: O(n log n)
  *
  * Параметры:
  *   arr   - массив для сортировки
  *   left  - левая граница
  *   right - правая граница
  */
void merge_sort_sequential(int* arr, int left, int right) {
    /* Базовый случай: 0 или 1 элемент — уже отсортирован */
    if (left >= right) return;

    /* Находим середину */
    int mid = left + (right - left) / 2;

    /* Рекурсивно сортируем левую и правую половины */
    merge_sort_sequential(arr, left, mid);
    merge_sort_sequential(arr, mid + 1, right);

    /* Сливаем отсортированные половины */
    simple_merge(arr, left, mid, right);
}

/* ============================================
 * ЗАГЛУШКА (для совместимости)
 * ============================================ */

 /*
  * Функция: destroy_sort_pool
  * Назначение: заглушка для совместимости с другими модулями
  * В этой версии пул потоков не используется
  */
void destroy_sort_pool(void) {
    /* Ничего не делаем */
}

/* ============================================
 * МНОГОПОТОЧНОЕ СЛИЯНИЕ С БИНАРНЫМ ПОИСКОМ
 * ============================================ */

 /*
  * Функция: merge_thread_wrapper
  * Назначение: обёртка для потока слияния (Windows)
  *
  * Windows CreateThread требует функцию с сигнатурой DWORD WINAPI (LPVOID)
  * Эта обёртка вызывает основную функцию parallel_merge_impl
  *
  * Параметры:
  *   arg - указатель на структуру MergeParams
  * Возвращает: 0 (всегда)
  */
#ifdef _WIN32
static DWORD WINAPI merge_thread_wrapper(LPVOID arg) {
    MergeParams* params = (MergeParams*)arg;
    parallel_merge_impl(params->source, params->left1, params->right1,
        params->left2, params->right2, params->dest, params->dest_left,
        params->depth + 1, params->max_threads,
        params->active_threads, params->mutex);
    return 0;
}
#endif

/*
 * Функция: parallel_merge_impl
 * Назначение: многопоточное слияние с бинарным поиском
 *
 * Алгоритм (ключевые шаги):
 * 1. Если один из массивов пуст — копируем другой
 * 2. Если массивы маленькие или глубина достигла предела — последовательное слияние
 * 3. Находим середину первого массива (опорный элемент)
 * 4. Бинарным поиском находим позицию опорного элемента во втором массиве
 * 5. Вставляем опорный элемент в результирующий массив
 * 6. Рекурсивно сливаем левые и правые части (параллельно)
 *
 * Сложность: O(log² n) при параллельном выполнении
 *
 * Параметры:
 *   source        - исходный массив
 *   left1, right1 - границы первого массива
 *   left2, right2 - границы второго массива
 *   dest          - результирующий массив
 *   dest_left     - начальная позиция в результирующем массиве
 *   depth         - текущая глубина рекурсии
 *   max_threads   - максимальное число потоков
 *   active_threads - счётчик активных потоков
 *   mutex         - мьютекс для синхронизации
 */
static void parallel_merge_impl(int* source, int left1, int right1, int left2, int right2,
    int* dest, int dest_left, int depth, int max_threads,
    int* active_threads, CRITICAL_SECTION* mutex) {

    /* Вычисляем размеры массивов */
    int n1 = right1 - left1 + 1;
    int n2 = right2 - left2 + 1;

    /* Базовый случай 1: первый массив пуст — копируем второй */
    if (n1 == 0) {
        for (int i = 0; i < n2; i++) {
            dest[dest_left + i] = source[left2 + i];
        }
        return;
    }

    /* Базовый случай 2: второй массив пуст — копируем первый */
    if (n2 == 0) {
        for (int i = 0; i < n1; i++) {
            dest[dest_left + i] = source[left1 + i];
        }
        return;
    }

    /*
     * Условия переключения на последовательное слияние:
     * 1. Общий размер <= 2000 элементов (оптимальный порог)
     * 2. Глубина рекурсии > 3 (ограничение числа потоков)
     * 3. Достигнут лимит активных потоков
     */
    if (n1 + n2 <= 2000 || depth > 3 || *active_threads >= max_threads) {
        /* Обычное последовательное слияние */
        int i = left1, j = left2, k = dest_left;
        while (i <= right1 && j <= right2) {
            if (source[i] <= source[j]) {
                dest[k++] = source[i++];
            }
            else {
                dest[k++] = source[j++];
            }
        }
        /* Копируем оставшиеся элементы из первого массива */
        while (i <= right1) {
            dest[k++] = source[i++];
        }
        /* Копируем оставшиеся элементы из второго массива */
        while (j <= right2) {
            dest[k++] = source[j++];
        }
        return;
    }

    /*
     * Убеждаемся, что первый массив не меньше второго
     * Это нужно для эффективности алгоритма (балансировка)
     */
    if (n1 < n2) {
        /* Меняем массивы местами */
        int temp_l = left1, temp_r = right1;
        left1 = left2;
        right1 = right2;
        left2 = temp_l;
        right2 = temp_r;
        n1 = right1 - left1 + 1;
        n2 = right2 - left2 + 1;
    }

    /* Шаг 1: Находим середину первого массива (опорный элемент) */
    int mid1 = left1 + (right1 - left1) / 2;
    int pivot = source[mid1];

    /* Шаг 2: Бинарный поиск опорного элемента во втором массиве */
    int mid2 = left2 + find_insert_position(pivot, source, left2, right2);

    /* Шаг 3: Вычисляем позицию опорного элемента в результирующем массиве */
    int mid3 = dest_left + (mid1 - left1) + (mid2 - left2);
    dest[mid3] = pivot;  /* Вставляем опорный элемент */

    /*
     * Шаг 4: Рекурсивно сливаем левые и правые части
     *
     * Левая часть: source[left1..mid1-1] и source[left2..mid2-1]
     * Правая часть: source[mid1+1..right1] и source[mid2..right2]
     */

     /* Создаём локальные структуры для левой и правой частей */
    MergeParams left_params;
    left_params.source = source;
    left_params.left1 = left1;
    left_params.right1 = mid1 - 1;
    left_params.left2 = left2;
    left_params.right2 = mid2 - 1;
    left_params.dest = dest;
    left_params.dest_left = dest_left;
    left_params.depth = depth + 1;
    left_params.max_threads = max_threads;
    left_params.active_threads = active_threads;
    left_params.mutex = mutex;

    MergeParams right_params;
    right_params.source = source;
    right_params.left1 = mid1 + 1;
    right_params.right1 = right1;
    right_params.left2 = mid2;
    right_params.right2 = right2;
    right_params.dest = dest;
    right_params.dest_left = mid3 + 1;
    right_params.depth = depth + 1;
    right_params.max_threads = max_threads;
    right_params.active_threads = active_threads;
    right_params.mutex = mutex;

    HANDLE left_thread = NULL;
    int left_created = 0;

    /*
     * Пытаемся создать поток для левой части
     * Используем мьютекс для атомарного обновления счётчика потоков
     */
    EnterCriticalSection(mutex);
    if (*active_threads < max_threads) {
        (*active_threads)++;   /* Увеличиваем счётчик */
        left_created = 1;
    }
    LeaveCriticalSection(mutex);

    /* Если удалось создать поток — запускаем левую часть параллельно */
    if (left_created) {
        left_thread = CreateThread(NULL, 0, merge_thread_wrapper, &left_params, 0, NULL);
        if (!left_thread) {
            /* Если поток не создался — откатываем изменения */
            left_created = 0;
            EnterCriticalSection(mutex);
            (*active_threads)--;
            LeaveCriticalSection(mutex);
        }
    }

    /* Правую часть выполняем в текущем потоке (параллельно с левой) */
    if (left_created) {
        parallel_merge_impl(source, mid1 + 1, right1, mid2, right2, dest, mid3 + 1,
            depth + 1, max_threads, active_threads, mutex);
    }
    else {
        /* Если не удалось создать поток — выполняем обе части последовательно */
        parallel_merge_impl(source, left1, mid1 - 1, left2, mid2 - 1, dest, dest_left,
            depth + 1, max_threads, active_threads, mutex);
        parallel_merge_impl(source, mid1 + 1, right1, mid2, right2, dest, mid3 + 1,
            depth + 1, max_threads, active_threads, mutex);
    }

    /* Ожидаем завершения левого потока */
    if (left_created && left_thread) {
        WaitForSingleObject(left_thread, INFINITE);
        CloseHandle(left_thread);
        EnterCriticalSection(mutex);
        (*active_threads)--;   /* Уменьшаем счётчик */
        LeaveCriticalSection(mutex);
    }
}

/* ============================================
 * ОБЁРТКА ДЛЯ ПАРАЛЛЕЛЬНОГО СЛИЯНИЯ
 * ============================================ */

 /*
  * Функция: merge_parallel_wrapper
  * Назначение: обёртка для вызова параллельного слияния
  *
  * Адаптирует parallel_merge_impl (работает с двумя массивами)
  * к формату simple_merge (работает с одним массивом)
  *
  * Параметры:
  *   arr           - массив для слияния
  *   left, mid, right - границы частей
  *   depth         - глубина рекурсии
  *   max_threads   - максимальное число потоков
  *   active_threads - счётчик активных потоков
  *   mutex         - мьютекс для синхронизации
  */
static void merge_parallel_wrapper(int* arr, int left, int mid, int right, int depth,
    int max_threads, int* active_threads, CRITICAL_SECTION* mutex) {

    int n1 = mid - left + 1;
    int n2 = right - mid;

    /* Условия переключения на последовательное слияние */
    if (n1 + n2 <= 2000 || depth > 3 || *active_threads >= max_threads) {
        simple_merge(arr, left, mid, right);
        return;
    }

    /* Создаём временный массив для результата слияния */
    int* temp = (int*)malloc((right - left + 1) * sizeof(int));
    if (!temp) {
        simple_merge(arr, left, mid, right);
        return;
    }

    /* Вызываем многопоточное слияние */
    parallel_merge_impl(arr, left, mid, mid + 1, right, temp, 0,
        depth, max_threads, active_threads, mutex);

    /* Копируем результат обратно в исходный массив */
    for (int i = 0; i < right - left + 1; i++) {
        arr[left + i] = temp[i];
    }
    free(temp);
}

/* ============================================
 * ПАРАЛЛЕЛЬНАЯ СОРТИРОВКА
 * ============================================ */

 /*
  * Функция: sort_thread_wrapper
  * Назначение: обёртка для потока сортировки (Windows)
  *
  * Параметры:
  *   arg - указатель на структуру SortParams
  * Возвращает: 0 (всегда)
  */
#ifdef _WIN32
static DWORD WINAPI sort_thread_wrapper(LPVOID arg) {
    SortParams* params = (SortParams*)arg;
    int left = params->left;
    int right = params->right;
    int* arr = params->arr;
    int depth = params->depth;
    int max_threads = params->max_threads;
    int* active_threads = params->active_threads;
    CRITICAL_SECTION* mutex = params->mutex;

    /* Базовый случай: 0 или 1 элемент */
    if (left >= right) {
        free(params);
        return 0;
    }

    /*
     * Порог для переключения на последовательную сортировку:
     * - Массив <= 5000 элементов
     * - Или глубина рекурсии > 4
     * Это уменьшает накладные расходы на создание потоков для маленьких массивов
     */
    if (right - left + 1 <= 5000 || depth > 4) {
        merge_sort_sequential(arr, left, right);
        free(params);
        return 0;
    }

    /* Находим середину */
    int mid = left + (right - left) / 2;

    /* Создаём параметры для левой и правой половин */
    SortParams* left_params = (SortParams*)malloc(sizeof(SortParams));
    SortParams* right_params = (SortParams*)malloc(sizeof(SortParams));

    left_params->arr = arr;
    left_params->left = left;
    left_params->right = mid;
    left_params->depth = depth + 1;
    left_params->max_threads = max_threads;
    left_params->active_threads = active_threads;
    left_params->mutex = mutex;

    right_params->arr = arr;
    right_params->left = mid + 1;
    right_params->right = right;
    right_params->depth = depth + 1;
    right_params->max_threads = max_threads;
    right_params->active_threads = active_threads;
    right_params->mutex = mutex;

    HANDLE left_thread = NULL;
    int left_created = 0;

    /* Пытаемся создать поток для левой половины */
    EnterCriticalSection(mutex);
    if (*active_threads < max_threads) {
        (*active_threads)++;
        left_created = 1;
    }
    LeaveCriticalSection(mutex);

    if (left_created) {
        left_thread = CreateThread(NULL, 0, sort_thread_wrapper, left_params, 0, NULL);
        if (!left_thread) {
            left_created = 0;
            free(left_params);
            EnterCriticalSection(mutex);
            (*active_threads)--;
            LeaveCriticalSection(mutex);
        }
    }

    /* Правую половину сортируем в текущем потоке */
    if (left_created) {
        sort_thread_wrapper(right_params);
    }
    else {
        merge_sort_sequential(arr, left, mid);
        merge_sort_sequential(arr, mid + 1, right);
        free(left_params);
        free(right_params);
    }

    /* Ожидаем завершения левого потока */
    if (left_created && left_thread) {
        WaitForSingleObject(left_thread, INFINITE);
        CloseHandle(left_thread);
        EnterCriticalSection(mutex);
        (*active_threads)--;
        LeaveCriticalSection(mutex);
    }

    /* Многопоточное слияние отсортированных половин */
    merge_parallel_wrapper(arr, left, mid, right, depth + 1, max_threads, active_threads, mutex);

    free(params);
    return 0;
}
#endif

/* ============================================
 * ГЛАВНАЯ ФУНКЦИЯ
 * ============================================ */

 /*
  * Функция: parallel_merge_sort
  * Назначение: основная функция параллельной сортировки
  *
  * Алгоритм:
  * 1. Проверяем входные данные
  * 2. Инициализируем мьютекс для синхронизации
  * 3. Создаём корневую задачу с параметрами
  * 4. Запускаем сортировку в отдельном потоке
  * 5. Ожидаем завершения
  * 6. Очищаем ресурсы
  *
  * Параметры:
  *   arr   - массив для сортировки
  *   size  - размер массива
  *   max_threads - максимальное число потоков
  */
void parallel_merge_sort(int* arr, int size, int max_threads) {
    /* Проверка на пустой массив или один элемент */
    if (size <= 1) return;

    /* Создаём мьютекс для синхронизации потоков */
    CRITICAL_SECTION mutex;
    InitializeCriticalSection(&mutex);

    /* Счётчик активных потоков (главный поток уже активен) */
    int active_threads = 1;

    /* Выделяем память для корневых параметров */
    SortParams* params = (SortParams*)malloc(sizeof(SortParams));
    params->arr = arr;
    params->left = 0;
    params->right = size - 1;
    params->depth = 0;
    params->max_threads = max_threads;
    params->active_threads = &active_threads;
    params->mutex = &mutex;

    /* Запускаем сортировку в отдельном потоке */
#ifdef _WIN32
    HANDLE root_thread = CreateThread(NULL, 0, sort_thread_wrapper, params, 0, NULL);
    WaitForSingleObject(root_thread, INFINITE);
    CloseHandle(root_thread);
#else
    pthread_t root_thread;
    pthread_create(&root_thread, NULL, sort_thread_wrapper, params);
    pthread_join(root_thread, NULL);
#endif

    /* Уничтожаем мьютекс */
    DeleteCriticalSection(&mutex);
}