#pragma once
// NimBLE-based HR receiver for ESP32.

static String hrPreferredAddress;
static String hrCandidateAddress;
static volatile uint32_t hrScanRounds=0;
static constexpr uint32_t HR_BACKOFF_MS=20000;
static constexpr uint32_t HR_STALLED_AFTER_MS=12000;
