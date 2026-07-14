#define _CRT_SECURE_NO_WARNINGS
#include "scheduler_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Условная компиляция для разных ОС
 */
#ifdef _WIN32
#include <windows.h>     /* Для CreateThread, WaitForSingleObject */
#else
#include <pthread.h>     /* Для pthread_create, pthread_join */
#include <unistd.h>      /* Для usleep */
#endif

 /* ============================================
  * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
  * ============================================ */

  /*
   * static Process* current_process
   *
   * Указатель на текущий выполняемый процесс.
   * static означает, что переменная видна только в этом файле.
   * Используется для отслеживания состояния между вызовами.
   */
static Process* current_process = NULL;

/* ============================================
 * СТРУКТУРА И ФУНКЦИИ ОЧЕРЕДИ (ДЛЯ ROUND ROBIN)
 * ============================================ */

 /*
  * Структура узла очереди
  * Содержит указатель на процесс и ссылку на следующий узел
  */
typedef struct QueueNode {
    Process* process;          /* Указатель на процесс */
    struct QueueNode* next;    /* Указатель на следующий узел */
} QueueNode;

/*
 * Структура очереди
 * Содержит указатели на начало и конец очереди, а также размер
 */
typedef struct {
    QueueNode* front;          /* Начало очереди (первый элемент) */
    QueueNode* rear;           /* Конец очереди (последний элемент) */
    int size;                  /* Количество элементов в очереди */
} Queue;

/*
 * Функция: create_queue
 * Назначение: создаёт пустую очередь
 * Возвращает: указатель на созданную очередь или NULL при ошибке
 */
static Queue* create_queue(void) {
    /* Выделяем память под структуру очереди */
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) return NULL;       /* Если память не выделилась - возвращаем NULL */

    q->front = NULL;           /* Начало очереди - NULL (пусто) */
    q->rear = NULL;            /* Конец очереди - NULL (пусто) */
    q->size = 0;               /* Размер очереди - 0 */
    return q;
}

/*
 * Функция: enqueue
 * Назначение: добавляет процесс в конец очереди
 * Параметры:
 *   q - указатель на очередь
 *   p - указатель на процесс
 */
static void enqueue(Queue* q, Process* p) {
    /* Создаём новый узел очереди */
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return;         /* Если память не выделилась - выходим */

    node->process = p;         /* Сохраняем указатель на процесс */
    node->next = NULL;         /* Следующий узел - NULL (последний) */

    /* Если очередь пуста - новый узел становится и началом, и концом */
    if (q->rear == NULL) {
        q->front = q->rear = node;
    }
    else {
        /* Иначе добавляем в конец */
        q->rear->next = node;
        q->rear = node;
    }
    q->size++;                 /* Увеличиваем размер очереди */
}

/*
 * Функция: dequeue
 * Назначение: извлекает процесс из начала очереди
 * Параметры:
 *   q - указатель на очередь
 * Возвращает: указатель на извлечённый процесс или NULL если очередь пуста
 */
static Process* dequeue(Queue* q) {
    if (q->front == NULL) return NULL;   /* Если очередь пуста - возвращаем NULL */

    /* Запоминаем первый узел */
    QueueNode* node = q->front;
    Process* p = node->process;          /* Сохраняем указатель на процесс */

    q->front = q->front->next;           /* Перемещаем начало на следующий узел */

    /* Если очередь стала пустой - обнуляем указатель на конец */
    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(node);                          /* Освобождаем память узла */
    q->size--;                           /* Уменьшаем размер очереди */
    return p;                            /* Возвращаем указатель на процесс */
}

/*
 * Функция: is_queue_empty
 * Назначение: проверяет, пуста ли очередь
 * Параметры:
 *   q - указатель на очередь
 * Возвращает: true если пуста, false если есть элементы
 */
static int is_queue_empty(Queue* q) {
    return q->front == NULL;    /* Если начало = NULL - очередь пуста */
}

/*
 * Функция: free_queue
 * Назначение: освобождает всю память, занятую очередью
 * Параметры:
 *   q - указатель на очередь
 */
static void free_queue(Queue* q) {
    /* Пока очередь не пуста - извлекаем элементы (освобождаем узлы) */
    while (!is_queue_empty(q)) {
        dequeue(q);
    }
    free(q);    /* Освобождаем саму структуру очереди */
}

/* ============================================
 * ОБЁРТКА ДЛЯ WINDOWS
 * ============================================ */

#ifdef _WIN32
 /*
  * Функция: scheduler_worker_wrapper
  * Назначение: обёртка для Windows
  *
  * Windows CreateThread требует функцию с сигнатурой DWORD WINAPI (LPVOID)
  * Эта обёртка вызывает основную функцию scheduler_worker
  *
  * Параметры:
  *   arg - указатель на планировщик
  * Возвращает: 0 (всегда)
  */
DWORD WINAPI scheduler_worker_wrapper(LPVOID arg) {
    scheduler_worker(arg);    /* Вызываем основную функцию */
    return 0;                 /* Возвращаем 0 (успешное завершение) */
}
#endif

/* ============================================
 * ФУНКЦИИ ПЛАНИРОВЩИКА
 * ============================================ */

 /*
  * Функция: create_scheduler
  * Назначение: создание и инициализация планировщика
  *
  * Алгоритм:
  *   1. Выделяем память под структуру планировщика
  *   2. Сохраняем указатели на процессы и их количество
  *   3. Устанавливаем начальное время (0)
  *   4. Сохраняем тип алгоритма
  *   5. Для Round Robin устанавливаем квант времени (2 такта)
  *   6. Инициализируем мьютекс для синхронизации
  *   7. Для Round Robin создаём очередь
  *
  * Параметры:
  *   processes     - массив процессов
  *   num_processes - количество процессов
  *   type          - тип алгоритма планирования
  *
  * Возвращает: указатель на созданный планировщик
  */
Scheduler* create_scheduler(Process** processes, int num_processes, SchedulerType type) {
    /* Выделяем память под структуру планировщика */
    Scheduler* s = (Scheduler*)malloc(sizeof(Scheduler));
    if (!s) {
        fprintf(stderr, "ОШИБКА: не удалось выделить память для планировщика\n");
        return NULL;
    }

    /* Сохраняем указатели на процессы и их количество */
    s->processes = processes;
    s->num_processes = num_processes;

    /* Устанавливаем начальное время моделирования */
    s->current_time = 0;

    /* Сохраняем тип алгоритма планирования */
    s->type = type;

    /* Для Round Robin устанавливаем квант времени (2 такта) */
    s->time_quantum = (type == SCHEDULER_ROUND_ROBIN) ? 2 : 0;

    /* Планировщик ещё не запущен */
    s->is_running = false;

    /* Очередь пока не создана */
    s->ready_queue = NULL;

    /*
     * Инициализация примитива синхронизации
     * В Windows: критическая секция
     * В Linux: мьютекс
     * Это нужно для защиты общих данных от одновременного доступа
     */
#ifdef _WIN32
    InitializeCriticalSection(&s->lock);
#else
    pthread_mutex_init(&s->lock, NULL);
#endif

    /* Для Round Robin создаём очередь */
    if (type == SCHEDULER_ROUND_ROBIN) {
        s->ready_queue = create_queue();
    }

    return s;
}

/*
 * Функция: destroy_scheduler
 * Назначение: освобождение ресурсов планировщика
 *
 * Параметры:
 *   s - указатель на планировщик
 */
void destroy_scheduler(Scheduler* s) {
    if (!s) return;    /* Проверка на NULL */

    /* Уничтожение примитива синхронизации */
#ifdef _WIN32
    DeleteCriticalSection(&s->lock);
#else
    pthread_mutex_destroy(&s->lock);
#endif

    /* Если есть очередь - освобождаем её */
    if (s->ready_queue) {
        free_queue((Queue*)s->ready_queue);
    }

    free(s);    /* Освобождаем память планировщика */
}

/*
 * Функция: find_next_process
 * Назначение: поиск следующего процесса для выполнения
 *
 * Алгоритм выбора зависит от типа планировщика:
 *
 * FIFO:
 *   - Выбирает процесс с наименьшим временем прибытия
 *   - Процессы выполняются в порядке поступления
 *
 * Priority:
 *   - Выбирает процесс с наивысшим приоритетом (меньший номер)
 *   - Приоритет 0 - наивысший
 *
 * Round Robin:
 *   - Использует очередь (процессы добавляются в конец, берутся из начала)
 *
 * Параметры:
 *   s - указатель на планировщик
 *
 * Возвращает: указатель на выбранный процесс или NULL
 */
static Process* find_next_process(Scheduler* s) {
    Process* best = NULL;    /* Лучший найденный процесс (пока NULL) */

    /* ============================================
     * ROUND ROBIN - используем очередь
     * ============================================ */
    if (s->type == SCHEDULER_ROUND_ROBIN) {
        Queue* q = (Queue*)s->ready_queue;

        /*
         * Добавляем все прибывшие процессы в очередь
         * Проходим по всем процессам
         */
        for (int i = 0; i < s->num_processes; i++) {
            Process* p = s->processes[i];

            /* Пропускаем завершённые процессы */
            if (p->is_completed) continue;

            /* Проверяем, прибыл ли процесс в текущее время */
            if (p->arrival_time <= s->current_time) {
                /* Проверяем, есть ли уже процесс в очереди */
                int found = 0;
                QueueNode* current = q->front;
                while (current) {
                    if (current->process == p) {
                        found = 1;    /* Уже есть в очереди */
                        break;
                    }
                    current = current->next;
                }
                /* Если процесса нет в очереди - добавляем */
                if (!found) {
                    enqueue(q, p);
                }
            }
        }

        /* Берём процесс из начала очереди */
        if (!is_queue_empty(q)) {
            return dequeue(q);
        }
        return NULL;    /* Очередь пуста */
    }

    /* ============================================
     * FIFO И PRIORITY - линейный поиск
     * ============================================ */
    for (int i = 0; i < s->num_processes; i++) {
        Process* p = s->processes[i];

        /* Пропускаем завершённые процессы */
        if (p->is_completed) continue;

        /* Пропускаем процессы, которые ещё не прибыли */
        if (p->arrival_time > s->current_time) continue;

        /* Выбор по алгоритму */
        if (s->type == SCHEDULER_FIFO) {
            /* FIFO: выбираем процесс с наименьшим временем прибытия */
            if (!best || p->arrival_time < best->arrival_time) {
                best = p;
            }
        }
        else if (s->type == SCHEDULER_PRIORITY) {
            /* Priority: выбираем процесс с наивысшим приоритетом (меньший номер) */
            if (!best || p->priority < best->priority) {
                best = p;
            }
        }
    }

    return best;    /* Возвращаем найденный процесс или NULL */
}

/*
 * Функция: scheduler_worker
 * Назначение: рабочая функция потока планировщика
 *
 * Это основная функция, которая выполняется в отдельном потоке.
 * Она реализует алгоритм планирования:
 *
 * 1. Проверяет, все ли процессы завершены
 * 2. Захватывает мьютекс для доступа к общим данным
 * 3. Находит следующий процесс для выполнения
 * 4. Проверяет необходимость вытеснения
 * 5. Выполняет процесс
 * 6. Проверяет завершение процесса
 * 7. Освобождает мьютекс
 * 8. Повторяет цикл
 *
 * Синхронизация:
 *   - Мьютекс защищает доступ к общим данным
 *   - Только один поток может выполнять критическую секцию
 *   - Предотвращает гонки данных
 *
 * Параметры:
 *   arg - указатель на планировщик
 *
 * Возвращает: NULL (всегда)
 */
void* scheduler_worker(void* arg) {
    /* Получаем указатель на планировщик из аргумента */
    Scheduler* s = (Scheduler*)arg;

    /* Устанавливаем флаг работы */
    s->is_running = true;

    /* Счётчик времени для квантования (Round Robin) */
    int time_slice = 0;

    /*
     * Основной цикл планировщика
     * Работает пока планировщик запущен
     */
    while (s->is_running) {
        /* ============================================
         * ШАГ 1: Проверка завершения всех процессов
         * ============================================ */
        int completed_count = 0;
        for (int i = 0; i < s->num_processes; i++) {
            if (s->processes[i]->is_completed) {
                completed_count++;    /* Считаем завершённые процессы */
            }
        }
        /* Если все процессы завершены - выходим из цикла */
        if (completed_count == s->num_processes) break;

        /* ============================================
         * ШАГ 2: Захват мьютекса (критическая секция)
         * ============================================ */
#ifdef _WIN32
        EnterCriticalSection(&s->lock);    /* Windows: входим в критическую секцию */
#else
        pthread_mutex_lock(&s->lock);      /* Linux: захватываем мьютекс */
#endif

        /* ============================================
         * ШАГ 3: Поиск следующего процесса
         * ============================================ */
        Process* next = find_next_process(s);

        /* ============================================
         * ШАГ 4: Проверка вытеснения (для Priority)
         * ============================================ */
        if (s->type == SCHEDULER_PRIORITY && next && next != current_process) {
            if (current_process && !current_process->is_completed) {
                if (next->priority < current_process->priority) {
                    int executed_time = s->current_time - current_process->start_time;

                    // Защита от отрицательного времени
                    if (executed_time < 0) executed_time = 0;

                    if (executed_time >= current_process->burst_time) {
                        // Процесс должен быть завершён
                        current_process->completion_time = s->current_time;
                        current_process->turnaround_time = current_process->completion_time -
                            current_process->arrival_time;
                        current_process->waiting_time = current_process->turnaround_time -
                            current_process->burst_time;
                        current_process->is_completed = true;

                        printf("[%d] Процесс %s завершен (оборот=%d, ожидание=%d)\n",
                            s->current_time, current_process->name,
                            current_process->turnaround_time, current_process->waiting_time);

                        current_process = NULL;
                    }
                    else {
                        current_process->remaining_time = current_process->burst_time - executed_time;

                        printf("[%d] Вытеснен процесс %s (осталось %d тактов)\n",
                            s->current_time, current_process->name,
                            current_process->remaining_time);

                        current_process = NULL;
                    }
                    continue;
                }
            }
        }

        /* ============================================
         * ШАГ 5: Запуск нового процесса
         * ============================================ */
        if (current_process == NULL && next != NULL) {
            /* Начинаем выполнение нового процесса */
            current_process = next;

            /* Если процесс ещё не начинался - запоминаем время начала */
            if (current_process->start_time == -1) {
                current_process->start_time = s->current_time;
            }

            printf("[%d] Запущен процесс %s (приоритет %d)\n",
                s->current_time, current_process->name, current_process->priority);

            time_slice = 0;    /* Сбрасываем счётчик кванта */
        }

        /* ============================================
         * ШАГ 6: Выполнение процесса
         * ============================================ */
        if (current_process && !current_process->is_completed) {
            /* Определяем время выполнения (1 такт по умолчанию) */
            int elapsed = 1;

            /* ============================================
             * Для Round Robin - используем квант времени
             * ============================================ */
            if (s->type == SCHEDULER_ROUND_ROBIN) {
                /* Вычисляем, сколько можно выполнять: квант или оставшееся время */
                elapsed = (s->time_quantum < current_process->remaining_time) ?
                    s->time_quantum : current_process->remaining_time;
                time_slice += elapsed;    /* Увеличиваем счётчик кванта */
            }

            /* ============================================
             * Для Priority - проверяем появление более приоритетного процесса
             * ============================================ */
            if (s->type == SCHEDULER_PRIORITY) {
                /* Проверяем, есть ли процесс с более высоким приоритетом */
                int higher_priority = 0;
                for (int i = 0; i < s->num_processes; i++) {
                    Process* p = s->processes[i];
                    /*
                     * Условия для вытеснения:
                     * - процесс не завершён
                     * - не текущий процесс
                     * - уже прибыл
                     * - имеет более высокий приоритет (меньший номер)
                     */
                    if (!p->is_completed && p != current_process &&
                        p->arrival_time <= s->current_time &&
                        p->priority < current_process->priority) {
                        higher_priority = 1;
                        break;
                    }
                }
                if (higher_priority) {
                    /* Вытесняем текущий процесс */
                    int executed_time = s->current_time - current_process->start_time;

                    // ВАЖНО: Проверяем, не завершился ли процесс
                    if (executed_time >= current_process->burst_time) {
                        // Процесс должен был завершиться - завершаем его
                        current_process->completion_time = s->current_time;
                        current_process->turnaround_time = current_process->completion_time -
                            current_process->arrival_time;
                        current_process->waiting_time = current_process->turnaround_time -
                            current_process->burst_time;
                        current_process->is_completed = true;

                        printf("[%d] Процесс %s завершен (оборот=%d, ожидание=%d)\n",
                            s->current_time, current_process->name,
                            current_process->turnaround_time, current_process->waiting_time);

                        current_process = NULL;
                    }
                    else {
                        // Процесс вытесняется
                        current_process->remaining_time = current_process->burst_time - executed_time;

                        printf("[%d] Вытеснен процесс %s (осталось %d тактов)\n",
                            s->current_time, current_process->name,
                            current_process->remaining_time);

                        current_process = NULL;
                    }
                    continue; // Переходим к следующей итерации
                }
            }

            /* ============================================
             * ВЫПОЛНЯЕМ ПРОЦЕСС
             * ============================================ */
            current_process->remaining_time -= elapsed;    /* Уменьшаем оставшееся время */
            s->current_time += elapsed;                    /* Увеличиваем глобальное время */

            /* ============================================
             * ШАГ 7: Проверка завершения процесса
             * ============================================ */
            if (current_process->remaining_time <= 0) {
                /* Процесс завершился - рассчитываем статистику */
                current_process->completion_time = s->current_time;
                current_process->turnaround_time = current_process->completion_time -
                    current_process->arrival_time;
                current_process->waiting_time = current_process->turnaround_time -
                    current_process->burst_time;
                current_process->is_completed = true;

                printf("[%d] Процесс %s завершен (оборот=%d, ожидание=%d)\n",
                    s->current_time, current_process->name,
                    current_process->turnaround_time, current_process->waiting_time);

                current_process = NULL;    /* Сбрасываем текущий процесс */
                time_slice = 0;            /* Сбрасываем счётчик кванта */
            }
            /* ============================================
             * Для Round Robin - проверка истечения кванта
             * ============================================ */
            else if (s->type == SCHEDULER_ROUND_ROBIN &&
                time_slice >= s->time_quantum) {
                /* Квант истёк - возвращаем процесс в очередь */
                printf("[%d] Процесс %s вытеснен (квант истёк, осталось %d)\n",
                    s->current_time, current_process->name, current_process->remaining_time);
                Queue* q = (Queue*)s->ready_queue;
                /*
                 * ПРАВИЛЬНО: сначала добавляем текущий процесс в конец,
                 * потом берём следующий из начала
                 */
                enqueue(q, current_process);
                current_process = NULL;
                time_slice = 0;
            }
        }
        else {
            /* ============================================
             * Если нет процессов для выполнения - время идёт
             * ============================================ */
            if (s->type != SCHEDULER_ROUND_ROBIN) {
                s->current_time++;    /* Увеличиваем время (простой процессора) */
            }
        }

        /* ============================================
         * ШАГ 8: Освобождение мьютекса
         * ============================================ */
#ifdef _WIN32
        LeaveCriticalSection(&s->lock);    /* Windows: выходим из критической секции */
        Sleep(10);    /* Небольшая задержка для имитации работы */
#else
        pthread_mutex_unlock(&s->lock);    /* Linux: освобождаем мьютекс */
        usleep(10000); /* 10 мс задержка для имитации работы */
#endif
    }

    /* Планировщик завершил работу */
    s->is_running = false;
    return NULL;    /* Возвращаем NULL (как требуется для void* функции) */
}

/*
 * Функция: run_scheduler
 * Назначение: запуск планировщика в отдельном потоке
 *
 * Параметры:
 *   s - указатель на планировщик
 */
void run_scheduler(Scheduler* s) {
    /* Выводим информацию о запуске */
    printf("\n=== ЗАПУСК ПЛАНИРОВЩИКА ===\n");
    switch (s->type) {
    case SCHEDULER_FIFO:
        printf("Тип: FIFO (невытесняющий)\n");
        break;
    case SCHEDULER_ROUND_ROBIN:
        printf("Тип: Round Robin (вытесняющий)\n");
        printf("Квант времени: %d\n", s->time_quantum);
        break;
    case SCHEDULER_PRIORITY:
        printf("Тип: Приоритетное (вытесняющий)\n");
        break;
    }
    printf("====================================\n\n");

    /*
     * Создаём поток для планировщика
     * В Windows: CreateThread с обёрткой
     * В Linux: pthread_create
     */
#ifdef _WIN32
     /* Windows: создаём поток */
    s->threads = CreateThread(NULL, 0, scheduler_worker_wrapper, s, 0, NULL);
    /* Ожидаем завершения потока */
    WaitForSingleObject(s->threads, INFINITE);
    /* Закрываем дескриптор потока */
    CloseHandle(s->threads);
#else
     /* Linux: создаём поток */
    pthread_t thread;
    pthread_create(&thread, NULL, scheduler_worker, s);
    /* Ожидаем завершения потока */
    pthread_join(thread, NULL);
#endif
}

/*
 * Функция: print_scheduler_stats
 * Назначение: вывод статистики работы планировщика
 *
 * Параметры:
 *   s - указатель на планировщик
 */
void print_scheduler_stats(Scheduler* s) {
    printf("\n=== СТАТИСТИКА ПЛАНИРОВЩИКА ===\n");
    printf("Всего процессов: %d\n", s->num_processes);
    printf("Общее время: %d тактов\n\n", s->current_time);

    /* Заголовки таблицы */
    printf("Процесс    Прибытие   Выполнение Ожидание   Оборот     Приоритет\n");
    printf("----------------------------------------------------------------\n");

    int total_wait = 0, total_turnaround = 0;
    for (int i = 0; i < s->num_processes; i++) {
        Process* p = s->processes[i];
        printf("%-10s %-10d %-10d %-10d %-10d %-10d\n",
            p->name, p->arrival_time, p->burst_time,
            p->waiting_time, p->turnaround_time, p->priority);
        total_wait += p->waiting_time;
        total_turnaround += p->turnaround_time;
    }

    printf("----------------------------------------------------------------\n");
    printf("Среднее время ожидания: %.2f\n", (float)total_wait / s->num_processes);
    printf("Среднее время оборота: %.2f\n", (float)total_turnaround / s->num_processes);
}

/*
 * Функция: generate_test_processes
 * Назначение: генерация тестовых процессов
 *
 * Параметры:
 *   count - указатель для сохранения количества процессов
 *
 * Возвращает: массив указателей на процессы
 */
Process** generate_test_processes(int* count) {
    *count = 6;    /* Создаём 6 процессов */
    Process** processes = (Process**)malloc(*count * sizeof(Process*));

    /* Тестовые данные */
    const char* names[] = { "Firefox", "Chrome", "Python", "Java", "Docker", "MySQL" };
    int arrivals[] = { 0, 1, 2, 3, 5, 7 };    /* Время прибытия */
    int bursts[] = { 5, 3, 4, 6, 2, 4 };      /* Время выполнения */
    int priorities[] = { 1, 0, 2, 1, 3, 0 };  /* Приоритет (0 - наивысший) */

    /* Создаём каждый процесс */
    for (int i = 0; i < *count; i++) {
        processes[i] = (Process*)malloc(sizeof(Process));
        processes[i]->pid = i + 1;                     /* ID от 1 до 6 */
        strcpy(processes[i]->name, names[i]);          /* Имя процесса */
        processes[i]->arrival_time = arrivals[i];      /* Время прибытия */
        processes[i]->burst_time = bursts[i];          /* Время выполнения */
        processes[i]->remaining_time = bursts[i];      /* Оставшееся время */
        processes[i]->priority = priorities[i];        /* Приоритет */
        processes[i]->start_time = -1;                 /* Ещё не начинался */
        processes[i]->completion_time = 0;             /* Ещё не завершён */
        processes[i]->waiting_time = 0;                /* Начальное ожидание 0 */
        processes[i]->turnaround_time = 0;             /* Начальный оборот 0 */
        processes[i]->is_completed = false;            /* Не завершён */
    }

    return processes;
}