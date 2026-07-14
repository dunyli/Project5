#ifndef SCHEDULER_MODEL_H
#define SCHEDULER_MODEL_H

/* Подключаем стандартные заголовочные файлы */
#include <stddef.h>      /* Для size_t */
#include <stdbool.h>     /* Для bool (true/false) */
#include <stdio.h>       /* Для printf */
#include <stdlib.h>      /* Для malloc/free */
#include <string.h>      /* Для strcpy/strcmp */
#include <time.h>        /* Для time() */

/*
 * Условная компиляция для разных ОС
 * _WIN32 - определяется в Windows (включая 64-битную)
 */
#ifdef _WIN32
#include <windows.h>     /* Windows API: CreateThread, CRITICAL_SECTION */
#else
#include <pthread.h>     /* POSIX threads для Linux/Mac */
#include <unistd.h>      /* usleep() для Linux */
#endif

 /**
  * ============================================
  * ОПРЕДЕЛЕНИЯ И СТРУКТУРЫ
  * ============================================
  */

  /*
   * Перечисление SchedulerType
   * Определяет тип алгоритма планирования
   */
typedef enum {
    SCHEDULER_FIFO,          /* 0: Первый пришёл - первый выполнен */
    SCHEDULER_ROUND_ROBIN,   /* 1: Круговая очередь с квантом времени */
    SCHEDULER_PRIORITY       /* 2: Приоритетное планирование */
} SchedulerType;

/*
 * Структура Process
 * Представляет процесс в системе планирования
 *
 * Поля:
 *   pid             - уникальный идентификатор процесса
 *   name            - имя процесса (для отображения в консоли)
 *   arrival_time    - время появления процесса в системе
 *   burst_time      - полное время выполнения процесса
 *   remaining_time  - оставшееся время выполнения
 *   priority        - приоритет (0 - наивысший, 3 - наинизший)
 *   start_time      - время начала выполнения (-1 если не начинался)
 *   completion_time - время завершения выполнения
 *   waiting_time    - общее время ожидания в очереди
 *   turnaround_time - время от появления до завершения
 *   is_completed    - флаг завершения (true - завершён)
 */
typedef struct Process {
    int pid;                 /* Идентификатор процесса */
    char name[32];           /* Имя процесса */
    int arrival_time;        /* Время прибытия */
    int burst_time;          /* Время выполнения */
    int remaining_time;      /* Оставшееся время */
    int priority;            /* Приоритет (0 - наивысший) */
    int start_time;          /* Время начала (-1 = не начинался) */
    int completion_time;     /* Время завершения */
    int waiting_time;        /* Время ожидания */
    int turnaround_time;     /* Время оборота */
    bool is_completed;       /* Завершён ли процесс */
} Process;

/*
 * Структура Scheduler
 * Главная структура планировщика
 *
 * Поля:
 *   processes     - массив указателей на процессы
 *   num_processes - количество процессов
 *   current_time  - текущее время моделирования
 *   type          - тип алгоритма планирования
 *   time_quantum  - квант времени (только для Round Robin)
 *   ready_queue   - указатель на очередь (для Round Robin)
 *   lock          - мьютекс для синхронизации потоков
 *   threads       - дескриптор потока (HANDLE или pthread_t)
 *   is_running    - флаг работы планировщика
 */
typedef struct Scheduler {
    Process** processes;     /* Массив указателей на процессы */
    int num_processes;       /* Количество процессов */
    int current_time;        /* Текущее время моделирования */
    SchedulerType type;      /* Тип алгоритма планирования */
    int time_quantum;        /* Квант времени для Round Robin */
    void* ready_queue;       /* Очередь готовых процессов (для RR) */

    /* Примитивы синхронизации - защищают общие данные от одновременного доступа */
#ifdef _WIN32
    CRITICAL_SECTION lock;   /* Критическая секция Windows */
    HANDLE threads;          /* Дескриптор потока Windows */
#else
    pthread_mutex_t lock;    /* Мьютекс POSIX */
    pthread_t threads;       /* Идентификатор потока POSIX */
#endif
    bool is_running;         /* Флаг работы планировщика */
} Scheduler;

/*
 * Прототипы функций
 */
Scheduler* create_scheduler(Process** processes, int num_processes, SchedulerType type);
void destroy_scheduler(Scheduler* s);
void run_scheduler(Scheduler* s);
void* scheduler_worker(void* arg);
void print_scheduler_stats(Scheduler* s);
Process** generate_test_processes(int* count);

#ifdef _WIN32
DWORD WINAPI scheduler_worker_wrapper(LPVOID arg);
#endif

#endif /* SCHEDULER_MODEL_H */