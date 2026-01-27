# 1. Находим необходимые компоненты Qt (Обязательно нужен PrintSupport)
find_package(Qt5 COMPONENTS Widgets PrintSupport REQUIRED)

# 2. Указываем файлы исходников
set(QCUSTOMPLOT_SOURCES
    qcustomplot/qcustomplot.cpp
    qcustomplot/qcustomplot.h
)

# 3. Объявляем статическую библиотеку
add_library(QCustomPlot STATIC ${QCUSTOMPLOT_SOURCES})

# 4. Включаем автоматическую генерацию MOC-файлов (важно для Qt слотов/сигналов)
set_target_properties(QCustomPlot PROPERTIES AUTOMOC ON)

# 5. Линкуем зависимости Qt к библиотеке QCustomPlot
# Используем PUBLIC, чтобы зависимости передались и в твой основной проект
target_link_libraries(QCustomPlot PUBLIC
    Qt5::Widgets
    Qt5::PrintSupport
)

# 6. Указываем путь к заголовкам (чтобы в main.cpp можно было писать #include "qcustomplot.h")
target_include_directories(QCustomPlot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/qcustomplot
)
