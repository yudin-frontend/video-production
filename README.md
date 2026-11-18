# Свой видео-продакшн (C++ + OpenCV)

Программа показывает картинку с веб-камеры (или видеофайла) и накладывает на неё
графику по нажатию клавиш.

## Что сделано

| Клавиша | Действие |
|---|---|
| `c` | **Своя подпись** — плавно появляется снизу, висит ~2 секунды и исчезает |
| `l` | **Свой логотип** — вкл/выкл. **Усложнённый вариант:** логотип с анимацией (въезжает справа, вращается и растёт, затем «дышит»; при выключении уменьшается и исчезает) |
| `a` | вкл/выкл анимацию логотипа (статичный логотип) |
| `b` | чёрно-белый / цветной режим |
| `+` / `-` | прозрачность логотипа |
| `Esc` | выход |

В левом верхнем углу на секунду показывается нажатая клавиша — удобно для видео с экрана.
Раскладка клавиатуры при записи должна быть **английской**.

## Сборка и запуск

Нужны компилятор C++17, CMake и OpenCV.

- **Linux:** `sudo apt install build-essential cmake libopencv-dev`
- **macOS:** `brew install cmake opencv`
- **Windows:** установите OpenCV через [vcpkg](https://vcpkg.io) (`vcpkg install opencv`)
  и добавьте к команде cmake `-DCMAKE_TOOLCHAIN_FILE=<путь к vcpkg>/scripts/buildsystems/vcpkg.cmake`

```bash
cmake -S . -B build
cmake --build build --config Release
```

Запуск (из папки проекта):

```bash
./build/video_production                      # сама ищет камеру 0..3; нет камеры -> демо-режим
./build/video_production --camera 1           # конкретная камера
./build/video_production --video my.mp4       # видеофайл вместо камеры
./build/video_production --demo               # без камеры: анимированная демо-картинка
```
На Windows exe лежит в `build\Release\video_production.exe`.

В VS Code: расширения **C/C++** и **CMake Tools**, затем «CMake: Configure» → «CMake: Build» → «CMake: Run Without Debugging».

## Свои данные

Подпись и логотип — обычные PNG в папке `assets/`:
- `caption.png` — подпись (640×80): «Юдин Максим, студент ИСП9-Kh32»;
- `logo.png` — логотип IThub (на светлой подложке, фон прозрачный).

Чтобы поменять текст или картинку, измените переменные в начале `tools/make_assets.py`
(`AUTHOR`, `SUBTITLE`, `LOGO_SOURCE`) и запустите `python tools/make_assets.py`
(нужен Python + Pillow: `pip install Pillow`). Либо просто подложите свои PNG в `assets/`.
После этого пересоберите проект.

## Структура

```
src/main.cpp              # вся программа (клавиши, анимации, наложение)
CMakeLists.txt            # сборка
assets/caption.png        # подпись (показывается по клавише c)
assets/logo.png           # логотип (по клавише l)
assets/ithub_original.png # исходная картинка логотипа
tools/make_assets.py      # необязательный генератор caption.png и logo.png
```
