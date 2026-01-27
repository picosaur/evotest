%% ПОЛНЫЙ ПРИМЕР АНАЛИЗА РАЗРЫВНОГО ТЕСТА (ISO 6892-1 / ГОСТ 1497)
clear; clc; close all;

%% 1. ИМИТАЦИЯ ДАННЫХ С ДАТЧИКОВ (Симуляция "Железа")
% В реальном ПО эти данные приходят по Modbus в цикле.
% Здесь мы генерируем их математически.

% Параметры виртуального образца (ввод оператора)
L0 = 50;        % Начальная длина, мм
D0 = 10;        % Начальный диаметр, мм
S0 = pi * (D0/2)^2; % Площадь сечения, мм^2

% Генерация идеальной кривой (Модель Ramberg-Osgood + Necking)
n_points = 2000;
true_E = 210000; % Истинный модуль упругости стали (МПа)

% Создаем массив деформации (от 0 до 15%)
strain_sim = linspace(0, 0.15, n_points); 

% Генерируем напряжения (МПа)
stress_sim = zeros(size(strain_sim));

% Зона упругости (до 0.25%)
limit_el = 0.0025;
idx_el = strain_sim <= limit_el;
stress_sim(idx_el) = true_E * strain_sim(idx_el);

% Зона пластичности (упрощенная парабола)
idx_pl = strain_sim > limit_el;
stress_start_pl = true_E * limit_el;
strain_pl_local = strain_sim(idx_pl) - limit_el;
stress_sim(idx_pl) = stress_start_pl + 400 * (strain_pl_local).^0.4;

% Имитация шейки (падение нагрузки в конце)
necking_idx = round(n_points * 0.85);
stress_sim(necking_idx:end) = stress_sim(necking_idx:end) .* linspace(1, 0.9, n_points - necking_idx + 1);

% --- ПРЕВРАЩАЕМ В "СЫРЫЕ" ДАННЫЕ ДАТЧИКОВ ---
% 1. Добавляем шум (вибрации машины, наводки АЦП)
noise_lvl = 2.0; % Шум в МПа
stress_sim = stress_sim + randn(size(stress_sim)) * noise_lvl;

% 2. Конвертируем обратно в Силу (Н) и Перемещение (мм)
% Также добавим "паразитный" сдвиг в начале (проскальзывание зажимов)
slack_mm = 0.5; % 0.5 мм холостого хода
Extension_Raw_mm = (strain_sim * L0) + slack_mm; 
Force_Raw_N = stress_sim * S0;

% Теперь у нас есть Force_Raw_N и Extension_Raw_mm - то, что ты получаешь с Modbus!

%% 2. БЛОК АНАЛИЗА (То, что должен делать твой класс Analysis)

% Шаг А: Нормализация (Пересчет в Stress/Strain)
% Stress = Force / Area
Stress_MPa = Force_Raw_N / S0;

% Strain = (Extension - Slack?) / L0.
% Пока мы не знаем Slack, считаем "как есть".
Strain_Raw = Extension_Raw_mm / L0; 

% Шаг Б: Расчет Модуля Упругости (E) и Коррекция нуля
% Ищем линейный участок. Обычно это 20-40% от максимальной силы.
F_max = max(Force_Raw_N);
idx_start = find(Force_Raw_N > 0.2 * F_max, 1);
idx_end   = find(Force_Raw_N > 0.4 * F_max, 1);

% Выделяем данные для регрессии
strain_segment = Strain_Raw(idx_start:idx_end);
stress_segment = Stress_MPa(idx_start:idx_end);

% Линейная регрессия (y = kx + b), где k - это E
coeffs = polyfit(strain_segment, stress_segment, 1);
E_calc = coeffs(1);     % Наш рассчитанный Модуль Упругости
Intercept = coeffs(2);  % Смещение по оси Y

% Коррекция нуля (Toe Compensation)
% Уравнение прямой: Stress = E * Strain + b
% В нуле напряжения (Stress=0): Strain_Zero = -b / E
strain_correction = -Intercept / E_calc;

% Создаем "Чистый" массив деформации (сдвигаем график влево)
Strain_Corrected = Strain_Raw - strain_correction;
% Обнуляем отрицательные хвосты (шум до начала нагрузки)
Strain_Corrected(Strain_Corrected < 0) = 0;

% Шаг В: Поиск Rp0.2 (Условный предел текучести)
offset_val = 0.002; % 0.2%
% Уравнение смещенной линии: y = E * (x - offset)
Rp02_Line = E_calc * (Strain_Corrected - offset_val);

% Ищем пересечение: где Реальный График становится НИЖЕ Смещенной Линии
% Начинаем поиск не с самого начала, а с зоны пластичности
search_start_idx = idx_end + 10; 
idx_Rp02 = -1;

for i = search_start_idx : length(Strain_Corrected)
    if Stress_MPa(i) < Rp02_Line(i)
        idx_Rp02 = i;
        break;
    end
end

% Интерполяция для точности (между i-1 и i)
if idx_Rp02 > 1
    p1 = [Strain_Corrected(idx_Rp02-1), Stress_MPa(idx_Rp02-1)];
    p2 = [Strain_Corrected(idx_Rp02),   Stress_MPa(idx_Rp02)];
    % Просто берем точку i для упрощения примера, в проде нужна интерполяция
    Val_Rp02 = Stress_MPa(idx_Rp02);
    Strain_at_Rp02 = Strain_Corrected(idx_Rp02);
else
    Val_Rp02 = NaN; % Ошибка, не нашли
end

% Шаг Г: Поиск Rm (Временное сопротивление)
[Val_Rm, idx_Rm] = max(Stress_MPa);
Strain_at_Rm = Strain_Corrected(idx_Rm);

% Шаг Д: Удлинение при разрыве (A / At)
% Берем последнюю точку (или точку, где сила упала ниже порога)
Val_At_Percent = Strain_Corrected(end) * 100;

% Шаг Е: Работа разрушения (Energy / Toughness)
% Интеграл Force по Extension (Дж)
Energy_Joules = trapz(Extension_Raw_mm, Force_Raw_N) / 1000; % /1000 т.к. Н*мм = мДж

%% 3. ВЫВОД РЕЗУЛЬТАТОВ (Имитация отчета)

fprintf('------------------------------------------------\n');
fprintf(' ПРОТОКОЛ ИСПЫТАНИЯ № "тристатридцатьтри"\n');
fprintf('------------------------------------------------\n');
fprintf('Образец: L0=%.1f мм, D0=%.1f мм, S0=%.1f мм^2\n', L0, D0, S0);
fprintf('------------------------------------------------\n');
fprintf('Результаты расчета:\n');
fprintf('1. Модуль упругости (E) : %.0f МПа\n', E_calc);
fprintf('2. Предел текучести (Rp0.2): %.2f МПа\n', Val_Rp02);
fprintf('3. Врем. сопротивление (Rm): %.2f МПа\n', Val_Rm);
fprintf('4. Удлинение (A)        : %.2f %%\n', Val_At_Percent);
fprintf('5. Энергия разрушения   : %.2f Дж\n', Energy_Joules);
fprintf('------------------------------------------------\n');

%% 4. ГРАФИКА (GUI оператора)
figure('Name', 'Анализ растяжения', 'Color', 'w');
hold on; grid on; box on;

% 1. Рисуем основной график (Stress - Strain %)
% Умножаем X на 100 для отображения в процентах
plot(Strain_Corrected*100, Stress_MPa, 'b-', 'LineWidth', 2, 'DisplayName', 'Кривая испытания');

% 2. Рисуем Линию Упругости (E)
x_fit = linspace(0, 0.005, 100); % Маленький кусочек для визуализации
y_fit = E_calc * x_fit;
plot(x_fit*100, y_fit, 'g--', 'LineWidth', 1.5, 'DisplayName', 'Модуль упругости (E)');

% 3. Рисуем Линию Смещения (Rp0.2)
x_offset_vis = linspace(offset_val, offset_val + 0.005, 100);
y_offset_vis = E_calc * (x_offset_vis - offset_val);
plot(x_offset_vis*100, y_offset_vis, 'k--', 'LineWidth', 1, 'DisplayName', 'Смещение 0.2%');

% 4. Отмечаем точки
if ~isnan(Val_Rp02)
    plot(Strain_at_Rp02*100, Val_Rp02, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 8, 'DisplayName', 'Rp 0.2');
    text(Strain_at_Rp02*100 + 0.5, Val_Rp02, sprintf('Rp0.2 = %.0f', Val_Rp02), 'FontSize', 10);
end

plot(Strain_at_Rm*100, Val_Rm, 'mo', 'MarkerFaceColor', 'm', 'MarkerSize', 8, 'DisplayName', 'Rm (Max)');
text(Strain_at_Rm*100, Val_Rm + 10, sprintf('Rm = %.0f', Val_Rm), 'FontSize', 10);

plot(Strain_Corrected(end)*100, Stress_MPa(end), 'kx', 'MarkerSize', 10, 'LineWidth', 2, 'DisplayName', 'Разрыв');

% Оформление
xlabel('Деформация (Strain), %');
ylabel('Напряжение (Stress), МПа');
title(['Результаты теста: Rm = ' num2str(round(Val_Rm)) ' МПа, Rp0.2 = ' num2str(round(Val_Rp02)) ' МПа']);
legend('Location', 'SouthEast');
ylim([0, max(Stress_MPa)*1.1]);
xlim([0, max(Strain_Corrected*100)*1.1]);