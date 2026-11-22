#pragma once

// NFC でかざしたときに開く URL（QR と同じ名刺ページ）
// "https://" は NDEF の短縮コードで送るので、ここには含めない
#define CARD_URL_WITHOUT_SCHEME "inai17ibar.github.io/card/"

// 起動・タッチ後、NFC を有効にしておく時間。過ぎたらディープスリープ
// （e-paper なので名刺の表示はスリープ中も残る）
#define AWAKE_MS (3 * 60 * 1000UL)

// エミュレートする NFC タグの UID（7バイト、先頭 0x04 = NXP 互換）
#define CARD_UID {0x04, 0x17, 0x1B, 0xA2, 0x5C, 0x30, 0x01}
