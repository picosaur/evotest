%% ИСПРАВЛЕННЫЙ ПРИМЕР: ПЛАСТИК (ISO 527)
clear; clc; close all;

%% 1. ГЕНЕРАЦИЯ ДАННЫХ (Гарантированно качественная имитация)
L0 = 50;        % мм
S0 = 40;        % мм^2
n_points = 2000; % Много точек, чтобы попасть в 0.05% и 0.25%

% Строго монотонно возрастающая деформация (без шума по оси X, как у энкодера)
strain_ideal = linspace(0, 0.15, n_points); % до 15%

% Модель материала (ABS пластик)
E_true = 2500; % МПа
Yield_point = 55; % МПа

stress_sim = zeros(1, n_points);
for i = 1:n_points
    e = strain_ideal(i);
    % Упругая зона + переход в пластику + небольшой спад
    if e < 0.02
        stress_sim(i) = E_true * e * (1 - e*10); 
    else
        % Имитация текучести с пиком 
        stress_sim(i) = Yield_point - (e - 0.02)*100 + randn()*0.05; 
    end
end
% Сглаживание стыка
stress_sim = smoothdata(stress_sim, 'gaussian', 50);

% Добавляем шум ТОЛЬКО на датчик силы (в реальности шумит АЦП тензодатчика)
Force_Raw_N = (stress_sim * S0) + randn(size(stress_sim)) * 5; % Шум +/- 5Н

% Имитация провисания образца (Slack) - начало не в нуле
Slack_mm = 0.15; 
Extension_Raw_mm = (strain_ideal * L0) + Slack_mm;

%% 2. БЛОК АНАЛИЗА

% А. Первичный расчет (Raw Data)
Stress_MPa = Force_Raw_N / S0;
Strain_Raw = Extension_Raw_mm / L0; 

% Б. Расчет Модуля упругости (Et) по ISO 527
% Диапазон строго: 0.05% (0.0005) ... 0.25% (0.0025)
val_e1 = 0.0005; % 0.05% от L0!!!!
val_e2 = 0.0025; % 0.025% от L0!!!!

% Ищем индексы ближайших точек
[~, idx1] = min(abs(Strain_Raw - val_e1));
[~, idx2] = min(abs(Strain_Raw - val_e2));

% ЗАЩИТА: Если точек мало и индексы совпали, раздвигаем их
if idx2 <= idx1
    idx2 = idx1 + 5; % Берем хотя бы 5 точек разницы
end
% ЗАЩИТА: Проверка границ массива
if idx2 > length(Strain_Raw), idx2 = length(Strain_Raw); end

% Считаем модуль (секущая)
d_sigma = Stress_MPa(idx2) - Stress_MPa(idx1);
d_epsilon = Strain_Raw(idx2) - Strain_Raw(idx1);

if d_epsilon == 0
    E_modulus = NaN; % Ошибка данных
    fprintf('Ошибка: d_epsilon = 0. Проверьте дискретизацию данных!\n');
else
    E_modulus = d_sigma / d_epsilon;
end

% В. Коррекция деформации (Toe Compensation)
% Если модуль не посчитался, коррекцию делать нельзя
if ~isnan(E_modulus)
    % Находим, где линия модуля пересекает ось X (Strain = 0)
    % y = E * (x - x0) + y0  -> при y=0 -> x_start = x1 - y1/E
    Strain_Start_Offset = Strain_Raw(idx1) - (Stress_MPa(idx1) / E_modulus);
    
    % Вычитаем этот оффсет из всего массива
    Strain_Corrected = Strain_Raw - Strain_Start_Offset;
else
    Strain_Corrected = Strain_Raw; % Без коррекции
end

% Г. Поиск Предела Текучести (Yield) и Разрыва
% Используем findpeaks, так как у тебя есть Toolbox
[pks, locs] = findpeaks(Stress_MPa, 'MinPeakProminence', 2.0);

Yield_Stress = NaN;
Strain_At_Yield = NaN;

if ~isempty(pks)
    % Первый значимый пик считаем пределом текучести
    Yield_Stress = pks(1);
    idx_yield = locs(1);
    Strain_At_Yield = Strain_Corrected(idx_yield) * 100; % В процентах
else
    % Если пиков нет, берем Максимум (хрупкий материал)
    [Yield_Stress, idx_yield] = max(Stress_MPa);
    Strain_At_Yield = Strain_Corrected(idx_yield) * 100;
end

% Д. Параметры разрыва (последняя точка)
Break_Stress = Stress_MPa(end);
Break_Strain = Strain_Corrected(end) * 100; % В процентах

% Е. Прочность (Rm) - Максимум за весь тест
Tensile_Strength = max(Stress_MPa);

%% 3. ОТЧЕТ
fprintf('\n--------------------------------------\n');
fprintf(' РЕЗУЛЬТАТЫ РАСЧЕТА (Corrected)\n');
fprintf('--------------------------------------\n');
fprintf('1. Модуль упругости (E)      : %.0f МПа\n', E_modulus);
fprintf('2. Предел текучести (Yield)  : %.2f МПа\n', Yield_Stress);
fprintf('   Деформация при текучести  : %.2f %%\n', Strain_At_Yield);
fprintf('3. Прочность (Rm)            : %.2f МПа\n', Tensile_Strength);
fprintf('4. Разрыв (Break Stress)     : %.2f МПа\n', Break_Stress);
fprintf('5. Удлинение при разрыве     : %.2f %%\n', Break_Strain);
fprintf('--------------------------------------\n');

%% 4. ГРАФИК
figure('Color', 'w'); hold on; grid on; box on;

% Основная линия
plot(Strain_Corrected*100, Stress_MPa, 'b-', 'LineWidth', 2);

% Линия Модуля (визуализация)
if ~isnan(E_modulus)
    x_mod = [0, 0.5]; % Рисуем линию до 0.5%
    y_mod = E_modulus * (x_mod/100); 
    plot(x_mod, y_mod, 'g--', 'LineWidth', 2);
    legend_str = sprintf('Modulus E=%.0f', E_modulus);
else
    legend_str = 'Modulus Error';
end

% Точки ISO
plot(Strain_Corrected(idx1)*100, Stress_MPa(idx1), 'r.', 'MarkerSize', 15);
plot(Strain_Corrected(idx2)*100, Stress_MPa(idx2), 'r.', 'MarkerSize', 15);

% Пик
plot(Strain_At_Yield, Yield_Stress, 'ko', 'MarkerFaceColor', 'y', 'MarkerSize', 8);

xlabel('Деформация (Corrected), %');
ylabel('Напряжение, МПа');
title('Диаграмма растяжения (Пластик)');
legend('Curve', legend_str, 'ISO Points', 'Yield');
xlim([0, max(Strain_Corrected*100)*1.1]);