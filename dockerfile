# Этап 1: сборка приложения
FROM ubuntu:22.04 AS builder

# Устанавливаем зависимости для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpq-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Копируем исходный код
WORKDIR /app
COPY . .

# Скачиваем header-only библиотеки (если их нет в репозитории)
# В вашем проекте они уже лежат в папке libs, поэтому просто копируем.
# Но для чистоты можно убедиться, что они есть.
RUN mkdir -p /app/libs
# (Если файлы уже есть в репозитории, ничего делать не надо)

# Сборка
RUN mkdir build && cd build && cmake .. && make

# Этап 2: финальный образ (только исполняемый файл и статика)
FROM ubuntu:22.04

# Устанавливаем runtime-зависимости (только libpq и OpenSSL)
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Копируем собранный бинарник и статические файлы
WORKDIR /app
COPY --from=builder /app/build/task_planner_web .
COPY --from=builder /app/public ./public

# Открываем порт
EXPOSE 8080

# Запускаем
CMD ["./task_planner_web"]