%% ПРИМЕР: РЕЗИНА / ЭЛАСТОМЕРЫ (ISO 37 / ASTM D412)
clear; clc; close all;

%% 1. ГЕНЕРАЦИЯ ДАННЫХ (S-образная кривая)
% Образец "Гантелька" (Type 2)
L0 = 25;        % мм (рабочая база)
S0 = 2 * 6;     % 12 мм^2 (толщина 2, ширина 6)
n_points = 2000;

% Тянем до 600% (Strain = 6.0)
strain_ideal = linspace(0, 6.0, n_points);

% Модель резины (Неогуковская + упрочнение в конце)
% График пологий в начале, крутой в конце.
stress_sim = zeros(1, n_points);
for i = 1:n_points
    e = strain_ideal(i);
    lam = e + 1; % Lambda (степень вытяжки)
    
    % Эмпирическая формула для S-кривой
    val = 1.5 * (lam - 1/lam^2) + 0.05 * (lam-1)^3; 
    
    stress_sim(i) = val;
end

% Добавляем шум (резина "дрожит" при растяжении, плюс шум датчика)
Force_Raw_N = (stress_sim * S0) + randn(size(stress_sim)) * 0.2; 
Extension_Raw_mm = strain_ideal * L0;

%% 2. БЛОК АНАЛИЗА

% А. Нормализация
Stress_MPa = Force_Raw_N / S0;
Strain_Raw = Extension_Raw_mm / L0;
Strain_Percent = Strain_Raw * 100;

% Б. Коррекция нуля (Toe Compensation)
% У резины начало часто вялое. Найдем, когда нагрузка превысила порог (напр. 0.5 Н)
Force_Threshold = 0.5; 
idx_start = find(Force_Raw_N > Force_Threshold, 1);

if isempty(idx_start)
    idx_start = 1;
end

% Сдвигаем деформацию, чтобы график выходил из нуля в точке начала нагрузки
Strain_Zero_Offset = Strain_Raw(idx_start);
Strain_Corrected = Strain_Raw - Strain_Zero_Offset;
Strain_Corrected_Percent = Strain_Corrected * 100;

% В. Расчет "Модулей" (Stress at specific strain) - M100, M200, M300
% Это обязательные параметры протокола ISO 37.
Targets_Percent = [100, 200, 300, 500]; % Список точек, которые надо найти
Results_Moduli = zeros(size(Targets_Percent)); % Сюда сохраним напряжения

fprintf('Поиск контрольных точек:\n');
for k = 1:length(Targets_Percent)
    target_p = Targets_Percent(k);
    
    % Ищем индекс, где деформация пересекает цель (например, 100%)
    % Используем find, чтобы найти первую точку больше цели
    idx_target = find(Strain_Corrected_Percent >= target_p, 1);
    
    if ~isempty(idx_target)
        % Берем напряжение в этой точке
        val_stress = Stress_MPa(idx_target);
        Results_Moduli(k) = val_stress;
        fprintf('  M%d (при %.0f%%) = %.2f МПа\n', target_p, target_p, val_stress);
    else
        Results_Moduli(k) = NaN; % Образец порвался раньше (например, на 250%)
        fprintf('  M%d: Образец порвался раньше\n', target_p);
    end
end

% Г. Прочность на разрыв (Tensile Strength)
[Tensile_Strength, idx_max] = max(Stress_MPa);

% Д. Удлинение при разрыве (Elongation at Break)
Elongation_Break = Strain_Corrected_Percent(idx_max);

%% 3. ОТЧЕТ
fprintf('\n--------------------------------------\n');
fprintf(' ПРОТОКОЛ ИСПЫТАНИЯ РЕЗИНЫ (ISO 37)\n');
fprintf('--------------------------------------\n');
fprintf('1. Прочность (TS)            : %.2f МПа\n', Tensile_Strength);
fprintf('2. Удлинение при разрыве (Eb): %.1f %%\n', Elongation_Break);
fprintf('--------------------------------------\n');
fprintf('Таблица модулей (Напряжений):\n');
for k = 1:length(Targets_Percent)
    if ~isnan(Results_Moduli(k))
        fprintf('  Modulus %d%% : %.2f МПа\n', Targets_Percent(k), Results_Moduli(k));
    end
end
fprintf('--------------------------------------\n');

%% 4. ГРАФИК
figure('Color', 'w'); hold on; grid on; box on;

% Основная кривая
plot(Strain_Corrected_Percent, Stress_MPa, 'b-', 'LineWidth', 2);

% Отмечаем точки модулей (M100, M200...)
for k = 1:length(Targets_Percent)
    if ~isnan(Results_Moduli(k))
        x_pt = Targets_Percent(k);
        y_pt = Results_Moduli(k);
        
        % Рисуем линию от оси X
        plot([x_pt, x_pt], [0, y_pt], 'k:', 'LineWidth', 1);
        plot([0, x_pt], [y_pt, y_pt], 'k:', 'LineWidth', 1);
        
        % Точка
        plot(x_pt, y_pt, 'bo', 'MarkerFaceColor', 'c', 'MarkerSize', 8);
        text(x_pt+10, y_pt, sprintf('M%d', x_pt), 'FontSize', 9);
    end
end

% Разрыв
plot(Elongation_Break, Tensile_Strength, 'rx', 'MarkerSize', 12, 'LineWidth', 2);
text(Elongation_Break-50, Tensile_Strength, sprintf(' Break: %.1f%%', Elongation_Break), 'Color', 'r', 'FontWeight', 'bold');

xlabel('Удлинение (Elongation), %');
ylabel('Напряжение (Stress), МПа');
title('Диаграмма растяжения резины (ISO 37)');
xlim([0, max(Strain_Corrected_Percent)*1.1]);
ylim([0, max(Stress_MPa)*1.1]);