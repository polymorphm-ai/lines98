# Lines-98 (C + SDL2 + Meson)

Полноценная игра в стиле Lines-98, написанная на чистом C (без C++), с рендерингом и звуком через SDL2.

## Особенности

- Поле 9x9, классические правила Lines-98
- Поиск пути между клетками (BFS)
- Удаление линий по 4 направлениям (горизонталь, вертикаль, 2 диагонали)
- Отображение следующих шаров
- Звук через SDL-аудио (генерация тона)
- Юнит-тесты ядра логики

## Зависимости

SDL2 подключается через Meson wrap (`subprojects/sdl2.wrap`), без привязки к пакетам конкретного Linux-дистрибутива.

## Сборка

```bash
meson setup build --wrap-mode=forcefallback
meson compile -C build
```

## Запуск игры

```bash
./build/lines98
```

Управление:

- ЛКМ: выбрать шар / переместить в пустую клетку
- `R`: начать новую игру

## Тесты

```bash
meson test -C build --print-errorlogs
```

Для прогона только тестов логики (без сборки SDL-приложения):

```bash
meson setup build-tests -Dbuild_game=false
meson test -C build-tests --print-errorlogs
```

## Проверки памяти

ASan/UBSan:

```bash
meson setup build-asan -Db_sanitize=address,undefined -Dbuild_game=false
meson test -C build-asan --print-errorlogs
```

LeakSanitizer (в средах без ограничений `ptrace`):

```bash
meson setup build-lsan -Db_sanitize=address,undefined -Dbuild_game=false -Denable_lsan=true
meson test -C build-lsan --print-errorlogs
```

Примечание: в некоторых sandbox/CI-окружениях LeakSanitizer недоступен из-за ограничений ядра.
