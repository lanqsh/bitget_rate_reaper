#ifndef _DEFINES_H
#define _DEFINES_H

// API
#define API_SUCCESS "00000"
#define MARGIN_COIN "USDT"

// Console colors
#define RED "\033[31m"
#define GREEN "\033[32m"
#define RESET_COLOR "\033[0m"

// Funding rate arbitrage timing (milliseconds)
// How long before next settlement to open position (10 minutes)
#define ENTRY_BEFORE_SETTLEMENT_MS (10LL * 60 * 1000)
// How long after settlement to wait before closing position (60 seconds)
#define EXIT_AFTER_SETTLEMENT_MS (60LL * 1000)
// Strategy polling interval (30 seconds)
#define STRATEGY_POLL_INTERVAL_MS (30 * 1000)
// Symbol scan interval (5 minutes)
#define SCAN_INTERVAL_MS (5 * 60 * 1000)

// Floating point epsilon thresholds
#define FLOAT_EPSILON   1e-6f   // near-zero threshold

#endif
