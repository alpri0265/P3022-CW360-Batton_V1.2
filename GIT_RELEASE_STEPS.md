# Покрокова інструкція: Злиття в main і створення релізу v1.0

## Крок 1: Перевірка поточного стану
1. Відкрийте GitHub Desktop
2. Перевірте поточну гілку (має бути видна вгорі)
3. Переконайтеся, що всі зміни збережені

## Крок 2: Створення коміту
1. У GitHub Desktop у полі "Summary" введіть:
   ```
   Refactor encoder to use external interrupts and fix menu navigation
   ```
2. У полі "Description" вставте текст з файлу `COMMIT_MESSAGE.txt`
3. Натисніть кнопку **"Commit to [назва поточної гілки]"**

## Крок 3: Перехід на головну гілку (main або master)
1. У меню GitHub Desktop: **Branch → New branch** (якщо потрібно створити main)
   АБО
   **Branch → Update from main** (якщо main вже існує)
2. Перемкніться на гілку **main** (або **master**):
   - **Branch → Switch to another branch...** → виберіть **main**

## Крок 4: Злиття змін у main
1. У GitHub Desktop переконайтеся, що ви на гілці **main**
2. **Branch → Merge into current branch...**
3. Виберіть гілку з вашими змінами (наприклад, якщо ви були на **develop** або **feature/encoder-interrupts**)
4. Натисніть **"Merge [назва гілки] into main"**
5. Якщо конфліктів немає, злиття пройде автоматично

## Крок 5: Push у віддалений репозиторій
1. Після успішного злиття натисніть **"Push origin"** (або **"Publish branch"** якщо main ще не опублікована)

## Крок 6: Створення тегу для релізу v1.0
### Варіант А: Через GitHub Desktop
1. У GitHub Desktop: **Repository → Open in Command Prompt** (або Terminal)
2. Виконайте команди:
   ```bash
   git tag -a v1.0 -m "Release v1.0: Interrupt-based encoder with fixed menu navigation"
   git push origin v1.0
   ```

### Варіант Б: Через GitHub веб-інтерфейс
1. Перейдіть на GitHub.com у ваш репозиторій
2. Натисніть **"Releases"** (праворуч, або в меню)
3. Натисніть **"Create a new release"**
4. Заповніть:
   - **Tag version**: `v1.0`
   - **Release title**: `Release v1.0 - Stable Encoder Navigation`
   - **Description**: Скопіюйте текст з `COMMIT_MESSAGE.txt`
5. Натисніть **"Publish release"**

## Альтернативний шлях (якщо ви вже на main):
Якщо ви вже працюєте безпосередньо на гілці main:

1. Створіть коміт з вашими змінами (Крок 2)
2. Push змін (Крок 5)
3. Створіть тег/реліз (Крок 6)

## Перевірка після створення релізу:
1. Перейдіть на GitHub.com → ваш репозиторій
2. Перевірте, що:
   - У розділі "Releases" з'явився реліз v1.0
   - У "Tags" з'явився тег v1.0
   - Код на гілці main містить всі зміни
