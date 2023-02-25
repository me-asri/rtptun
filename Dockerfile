FROM alpine:latest as build

RUN apk add gcc musl-dev make libev-dev libsodium-dev libsodium-static

WORKDIR /app
COPY . .

RUN make -j$(nproc) DEBUG=0 STATIC=1

FROM scratch

WORKDIR /app
COPY --from=build /app/bin/rel/rtptun-static /app/rtptun

ENTRYPOINT ["./rtptun"]