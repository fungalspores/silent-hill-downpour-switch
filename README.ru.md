# Silent Hill: Downpour для Nintendo Switch

[English](README.md)

Нативный порт **Xbox 360**-версии Silent Hill: Downpour (Vatra Games / Konami, 2012) на Nintendo Switch, сделанный
**статической рекомпиляцией** ([ReXGlue](https://github.com/rexglue/rexglue-sdk)): код игры переведён из PowerPC в C++
и собран под ARM64 для Horizon OS, а её команды Direct3D рисуются через Vulkan (NVK/Mesa).
Без эмулятора и без эмуляции видеочипа Xbox 360.

**Бесплатно. Не для продажи.** Новости: [t.me/fspores](https://t.me/fspores)

В репозитории **нет файлов игры**, нужна своя копия для Xbox 360.

![status](https://img.shields.io/badge/Silent%20Hill%20Downpour-playable-brightgreen)

## Скачать

Готовые сборки (NRO, настройки, NSP-форвардер для HOME) лежат в разделе **Releases**.

## Что получается

| | Silent Hill: Downpour |
|---|---|
| Картинка | 640x360 + FSR 1 по умолчанию (рекомендуется); 960x540 и 1280x720 (родное) на экране настроек |
| Состояние | работает всё, 30 кадров* |

\* Switch Lite (Mariko), Horizon OC: CPU 1963 / GPU 1152 / RAM 2333 МГц. На штатных частотах ниже.
При 640x360 консоль греется до 57 °C, при 1280x720 было 70 °C.

## Установка

Нужны Atmosphère 1.8.0+ с sigpatches, DBI (или другой установщик NSP), около 8 ГБ на карте. FAT32 подходит.

1. Скопируйте `switch/` из релиза в корень карты:
   ```
   sdmc:/switch/rex-start.nro
   sdmc:/switch/shd-nx/shd.nro, shd.toml
   ```
2. Положите файлы игры в `sdmc:/switch/shd-nx/game/` (внутри должен лежать `default.xex`).
   Из ISO: `extract-xiso -x "SilentHillDownpour.iso" -d game`. Папка `$SystemUpdate` не нужна.
3. Установите NSP из `nsp/` через DBI и запускайте игру с HOME. Она стартует через `rex-start.nro`: пять секунд
   показывается экран игры, **+** открывает настройки (внутреннее разрешение).
   Без NSP: зажмите **R** при запуске любой установленной игры и откройте `switch/shd-nx/shd.nro` из Homebrew-меню.
   **Не из Альбома:** в режиме апплета не хватает памяти.

По умолчанию `shd_renderer = "p2"` переводит шейдеры игры во время работы, поэтому **библиотека шейдеров не нужна**.
Старый путь `shd_renderer = "me"` может использовать офлайн-библиотеку `shd_shaders.mesp`, собранную из вашего
диска (Сборка, шаг 5). Ни её, ни скомпилированные шейдеры мы не раздаём: они сделаны из шейдеров самой игры.
Файл прогрева тоже не раздаём: помимо ключей конвейеров в нём лежит микрокод Xenos. При первом запуске игра
собирает конвейеры в фоне из микрокода игры, первые минуты возможны подёргивания. Затем кеш драйвера и список
прогрева создаются локально на вашей карте.

### Проверенное издание

Xbox 360, title ID 4B4E0823, без обновления. SHA-256 `default.xex`:
`c686cbff8facad6fb6b78ed98e5a5a2f4f19337ef83517f56195a56b0db2c8d7`.

Код рекомпилирован под конкретный `default.xex`, другие издания могут не запуститься.

## Настройки

`shd.toml` рядом с NRO. После изменения перезапустите игру.

| Ключ | По умолчанию | |
|---|---|---|
| `shd_render_width` x `shd_render_height` | 640x360 | внутреннее разрешение: 640x360, 960x540 или 1280x720 |
| `present_effect` | `"fsr"` | `"bilinear"` чуть быстрее и мягче |
| `p2_native_max_anisotropy` | 16 | 8 или 4 быстрее |
| `p2_native_max_mip_levels` | 1 | **оставьте 1**: с мип-уровнями возвращаются квадраты |
| `user_language` | 1 | 1 английский, 12 русский (зависит от вашей копии) |

Если у вас Horizon OC / sys-clk, задайте игре (`0153484400000000`) отдельный профиль частот. На старой ревизии
(Erista) высокий разгон не используйте.

## Экран настроек (`rex-start/`)

Та же маленькая программа, что в [prototype-switch](https://github.com/fungalspores/prototype-switch): framebuffer
libnx, шрифт самой консоли через FreeType, без SDL. Для Silent Hill предлагает только внутреннее разрешение, пишет
его прямо в `shd.toml`, остальные строки переносит без изменений. Ничего не нажали, игра запускается как обычно.

## Что исправлено

Порт построен на нативном рендере Prototype 2 NX (том же, что в нашем Prototype) и SDK masseffect-nx со слоем
Horizon. Silent Hill сделан на Unreal Engine 3, и ему понадобились свои правки:

- **Интерпретатор вершинных выборок SDK.** В беззнаковой ветке упакованных форматов компоненты не сдвигались на
  `packed_offsets[i]`, и у `k_16_16_16_16` компоненты y и w были копиями x и z (`0x12345678` превращалось
  в `0x56785678`). Знаковая ветка была правильной, поэтому ошибка и дожила. В Prototype 1 и 2 она тоже есть,
  просто там не проявляется.
- **memexport на процессоре.** UE3 перекладывает текстуры в своём пуле вершинным шейдером, который пишет в память
  (memexport). Рендер такие отрисовки пропускал, у перенесённых текстур каждый второй 8-байтный блок оставался
  пустым, и стены с полом шли полосами. Теперь шейдер выполняется на процессоре через интерпретатор SDK
  (`src/native_p2/native_memexport_cpu.cpp`).
- **Выбор копии сцены.** По одному адресу жили две копии HDR-сцены в разных форматах. При равном номере кадра рендер
  брал ту, что попалась последней в переборе, то есть запасную, а не записанную последней. После правки ушёл
  перекос по синему в лифте и вернулось отражение логотипа в луже главного меню.
- **Стопки resolve.** Игра пишет 16 срезов цветокоррекции в один ряд и читает их как 2D-стопку, а рендер отдавал всю
  полосу одним слоем, и титульный экран выходил красно-голубым.
- **Внутреннее разрешение.** Координаты самой игры не меняются. Цели отрисовки сохраняют гостевой размер во всей
  логике игры (плитки EDRAM, прямоугольники resolve, опознание текстур по адресу и размеру); масштабируются только
  образы и команды Vulkan, шагами по четверти, а resolve растягивает уменьшенную картинку обратно до гостевого
  размера. Глубину, которую игра читает потексельно, сначала копируют в полноразмерный образ (`DepthFullSize`).
- **Кеш конвейеров.** Конвейеры собираются в рабочих потоках, новый материал больше не останавливает кадр; кеш
  сбрасывается на карту (fsync) перед переименованием и сохраняется при выходе (выход через HOME его терял).
- Исправления SDK из Prototype (сохранения, подпись консоли, критические секции) и ускоряющие правки
  сгенерированного кода (`shd/tools/`).

Перед игрой 3 секунды показывается заставка с благодарностью (пропуск A/B/+).

## Сборка

Всё собрано **на Android-телефоне** (Alpine Linux в PRoot, aarch64) без Docker; на обычном Linux так же.
Шаги 1-3 те же, что в [prototype-switch](https://github.com/fungalspores/prototype-switch/blob/main/README.ru.md#сборка)
(`tools/proot/` это копия).

1. **devkitA64**: `tools/proot/pull_dka64.py`; на musl `tools/proot/patch_dkp.sh` и `wrap_dkp.sh`.
2. **SDK**: [masseffect-nx](https://github.com/nebadasSwifty/masseffect-nx) на `a85b0b4` +
   `patches/rexglue-sdk-masseffect-nx.patch`, сторонние исходники его `tools/fetch_thirdparty.py`, хостовый
   `rexglue` как в prototype-switch.
3. **Mesa/NVK**: [mesa-switch](https://github.com/danfromtico/mesa-switch) на `d4a00ea0ab3` +
   `patches/mesa-switch.patch`, затем `tools/proot/mesa_p2_build.sh`.
4. **Silent Hill**: распакуйте диск в `game/` рядом с `shd/`, затем `REXGLUE=... REXSDK_DIR=... NVK_SDK=... shd/build.sh`
   (кодогенерация несколько минут, NRO около 35 минут; на хосте нужен `glslangValidator`).
5. **Библиотека шейдеров, только для старого рендера `me`** (основной рендер `p2` её не использует):
   `R=<папка с game/> ME=<masseffect-nx на 650e14e + patches/masseffect-nx-xenosrecomp.patch>
   REXSDK=<SDK с патчем> DXC=<DXC под хост> sh shd/tools/build_shaders.sh` (около 25 минут). Скрипт достаёт
   15 702 контейнера шейдеров из пакетов UE3, переводит их в HLSL через XenosRecomp, собирает SPIR-V через DXC
   и упаковывает в `shd_shaders.mesp` + `.idx`.
6. **Экран настроек**: `rex-start/build.sh` (devkitA64 + FreeType из portlibs).
7. **NSP-форвардер**: `forwarder/build_forwarder.py --launcher sdmc:/switch/rex-start.nro --nro sdmc:/switch/shd-nx/shd.nro --icon art/icon_shd.jpg --title "Silent Hill Downpour" --author fungalspores --version 0.2.0 --tid 0153484400000000 --keys <ваши prod.keys> --out out/`
   (нужен [Forwarder-Mod](https://github.com/Skywalker25/Forwarder-Mod) `9853bd9` в `forwarder/loader`).
   Только свои `prod.keys`, никогда их не публикуйте.

`docs/D3D_SHD.md` описывает объекты XDK Direct3D в игре.

## Отчёты об ошибках

Логи пишутся в `switch/shd-nx/logs/`. Лог захода и скриншот, по ним дефект можно найти.
Телеграм: [t.me/fspores](https://t.me/fspores)

## Благодарности

- **penettrator**: [prototype2-nx](https://github.com/penettrator/prototype2-nx), нативный рендер, которым рисует порт.
- **NebadasSwifty**: [masseffect-nx](https://github.com/nebadasSwifty/masseffect-nx), SDK со слоем Horizon и сборка
  шейдеров (XenosRecomp, библиотека `.mesp`).
- **StevensND**: слой Horizon для ReXGlue и [nfsmw-nx](https://github.com/StevensND/nfsmw-nx).
- **R-drg**: [project8-sw](https://github.com/R-drg/project8-sw), основа нативного рендера.
- **DownpourRecomp**: адреса и путь цветокоррекции, с ними мы сверяли свои находки.
- **Tom Clay** и [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk); проект [Xenia](https://xenia.jp).
- **danfromtico**: [mesa-switch](https://github.com/danfromtico/mesa-switch); проект [Mesa](https://mesa3d.org).
- **Skywalker25**: [Forwarder-Mod](https://github.com/Skywalker25/Forwarder-Mod); switchbrew (libnx); devkitPro.
- AMD FidelityFX Super Resolution 1.

## Лицензия

GPL-3.0 (см. `LICENSE`); сторонние компоненты под своими лицензиями. Silent Hill является торговой маркой Konami.
Проект не связан с Konami, Vatra Games, Microsoft или Nintendo и не содержит их файлов.
