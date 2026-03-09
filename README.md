# bitget_rate_reaper
An automated funding rate arbitrage system built for Bitget perpetual futures markets. It identifies and exploits funding rate differentials by simultaneously holding long and short positions across correlated instruments, capturing periodic funding payments while maintaining market-neutral exposure. Features include real-time rate monitoring, position management, risk controls, and trade execution via Bitget's REST APIs.

## env
Copy config.example.properties to config.properties, then fill your credentials.

## Install Dependencies
```
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libsodium-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libpoco-dev
```