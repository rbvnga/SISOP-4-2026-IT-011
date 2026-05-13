FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    fuse3 \
    libfuse3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server fuse entrypoint.sh .
RUN chmod +x server fuse entrypoint.sh
RUN mkdir -p /app/db /app/encrypted_storage

EXPOSE 9000
CMD ["./entrypoint.sh"]
