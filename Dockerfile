FROM alpine:latest AS setup
ADD cosmotop.exe /
RUN wget -O /ape https://cosmo.zip/pub/cosmos/bin/ape-$(uname -m).elf
RUN chmod +x /ape && chmod +x /cosmotop.exe

FROM ubuntu:22.04 AS cosmotop
COPY --from=setup /cosmotop.exe /ape /
ENV LANG=en_US.UTF-8
ENTRYPOINT ["/ape", "/cosmotop.exe"]