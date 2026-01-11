@echo off
chcp 65001 >nul
echo ========================================
echo Створення тегу для релізу v1.0
echo ========================================
echo.

REM Перевірка, чи існує git репозиторій
if not exist .git (
    echo ПОМИЛКА: Не знайдено .git папку. Переконайтеся, що ви в корені репозиторію.
    pause
    exit /b 1
)

echo Крок 1: Створення тегу v1.0...
git tag -a v1.0 -m "Release v1.0: Interrupt-based encoder with fixed menu navigation"

if errorlevel 1 (
    echo ПОМИЛКА: Не вдалося створити тег. Можливо, тег вже існує.
    echo Для видалення старого тегу виконайте: git tag -d v1.0
    pause
    exit /b 1
)

echo Тег v1.0 успішно створено!
echo.
echo Крок 2: Відправка тегу на GitHub...
echo (Можливо, потрібно буде ввести дані авторизації)
git push origin v1.0

if errorlevel 1 (
    echo УВАГА: Не вдалося відправити тег автоматично.
    echo Спробуйте виконати вручну: git push origin v1.0
    echo Або створіть реліз через GitHub веб-інтерфейс.
) else (
    echo Тег v1.0 успішно відправлено на GitHub!
)

echo.
echo ========================================
echo Реліз v1.0 готово!
echo Перевірте на GitHub.com у розділі Releases
echo ========================================
pause
