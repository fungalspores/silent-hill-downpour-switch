# memexport на GPU (вместо интерпретатора на CPU) — P1/P2/SHD

## Зачем
Лог shd_137: кадры до 2196 мс при `the GPU 0.0 ms`. Виновник — `p2::memexport::RunVertex`
(интерпретатор SDK на CPU). За заход 1 874 944 элемента, в пике 337 408 за 10 с.
Нагрев: CPU 1963 МГц (horizon-oc) молотит интерпретатор.

## Что уже есть (НЕ надо писать заново)
- `sdk/src/graphics/pipeline/shader/spirv_translator_memexport.cpp` — `ExportToMemory()`,
  853 строки, пишет прямо в shared memory. Работает в Mass Effect.
- `IsMemoryExportSupported()` (spirv_translator.h:684): для VS нужен
  `features_.vertex_pipeline_stores_and_atomics`.
- ПРОВЕРЕНО: SDK (vulkan_device.cpp:133) отказывается создавать устройство без
  `vertexPipelineStoresAndAtomics` → раз игра стартует, на Tegra X1 (NVK) фича ЕСТЬ.
- P2 создаёт ТОТ ЖЕ `SpirvShaderTranslator` (native_renderer.cpp:879), т.е. шейдеры
  уже транслируются с memexport-кодом.
- P2 уже биндит `kDescriptorSetSharedMemoryAndEdram` (native_renderer.cpp:1447).
- Образец полной обвязки: `sdk/src/graphics/vulkan/command_processor.cpp:3686-3800`
  (`AddMemExportRanges`, `memexport_ranges_`, `memexport_readback_buffers_`).

## Что сделать в P2
1. `native_renderer.cpp:5566` — снять `return fail(2)` для VS с memexport
   (оставить fail для PS, если фича фрагментов не нужна).
2. Собрать диапазоны: `draw_util::AddMemExportRanges(regs, *vs, ranges)`.
3. После отрисовки — барьер + readback диапазонов в гостевую память
   (по образцу `memexport_readback_buffers_`).
4. `textures.MemoryWritten(lo, hi)` на изменённые диапазоны (метод уже добавлен вчера).
5. Выключить `p2_native_memexport_cpu` по умолчанию, оставить как запасной путь.

## Правила
- Эталон /root/shd/ЭТАЛОН_shd.nro (md5 4c664ac3) НЕ ТРОГАТЬ.
- Работа в /root/shd/shd_nx_memexport (копия от 23.10).
- Правка общая для P1/P2/SHD — по возможности в SDK, не в рендере.

## ГЛАВНОЕ ПРЕПЯТСТВИЕ (найдено 23.10 16:30)
Шейдер SDK пишет memexport ПО ГОСТЕВОМУ АДРЕСУ в буфер `buffers_shared_memory_`,
считая его отображением всей гостевой памяти с нуля
(`StoreUint32ToSharedMemory`, spirv_translator.cpp:4254: при одном биндинге —
`createAccessChain(.., address_dwords_int)` без всякого смещения).

У Mass Effect/Xenia так и есть: shared memory = вся гостевая память на GPU.
У P2 (native_renderer.cpp:350) `kStreamSize = 160 МБ` — это КОЛЬЦЕВОЙ БУФЕР
ПОТОКА (stream_buffer), куда на каждую отрисовку заливаются вершины/константы
по скользящему смещению `stream_head`. Биндинги shared memory указывают в него же
(строки 2406-2420), и это сделано только чтобы шейдер мог читать свои вершины.

=> Запись по гостевому адресу 0x16B3A000 (373 МБ) уедет мимо буфера 160 МБ.
Нужен один из путей:
 A. Выделить ОТДЕЛЬНЫЙ storage buffer под диапазоны memexport и подменять
    биндинг shared memory на него для таких отрисовок, пересчитывая адрес
    (сместить на base диапазона). Требует правки трансляции или подмены
    смещения в дескрипторе (`VkDescriptorBufferInfo.offset`).
 B. Сделать в P2 честное отображение гостевой памяти (как в SDK/Xenia) —
    большой buffer на 512 МБ. Дорого по памяти на Switch (3 ГБ всего).
 C. Транслировать такие VS в COMPUTE-шейдер (SDK умеет: `IsSpirvComputeShader()`),
    запускать диспатчем в отдельный буфер нужного размера и копировать обратно.
    Ближе всего к тому, что делает CPU-путь, но без интерпретатора.

Вариант A выглядит дешевле всего: диапазоны уже собирает `AddMemExportRanges`,
а `VkDescriptorBufferInfo.offset` можно выставить так, чтобы гостевой адрес
попал в выделенный кусок (с учётом minStorageBufferOffsetAlignment).

## Диагностика в логе
`same 0 overwrote-nonzero 6144` — у многих вызовов ноль совпадений, т.е. даже
после вчерашней правки сдвига перекладка копирует неверно. На GPU этот вопрос
снимается сам (исполняет настоящий шейдер, а не интерпретатор).
