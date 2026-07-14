#define _CRT_SECURE_NO_WARNINGS
#include "parallel_merge_sort.h"
#include "scheduler_model.h"
#include "locale.h"

/*
 * Объявление функции из test.c
 * run_all_tests запускает все 8 тестов
 */
void run_all_tests(void);

/*
 * Функция: main
 *
 * Назначение: точка входа в программу
 *
 * Алгоритм:
 *   1. Отключаем буферизацию вывода для отображения в реальном времени
 *   2. Запускаем все тесты
 *   3. Ожидаем нажатия Enter перед выходом
 *
 * Применение: запуск полного тестирования программы
 */
int main(void) {
    setlocale(0, "Rus");
    /*
     * setbuf(stdout, NULL) отключает буферизацию вывода
     * Это позволяет видеть сообщения сразу, без задержки
     */
    setbuf(stdout, NULL);

    /* Запуск всех тестов */
    run_all_tests();

    /*
     * Ожидание ввода перед завершением
     * Позволяет пользователю увидеть результаты
     */
    printf("\nНажмите Enter для выхода...");
    getchar();

    return 0;
}