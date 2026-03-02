#pragma once

// ===== BinTest UI Config =====
// 尽量只改这里，避免频繁改 .ino/.cpp

// SD SPI pins (M5PaperS3)
#ifndef BINFONT_SD_SPI_CS_PIN
  #define BINFONT_SD_SPI_CS_PIN   47
#endif
#ifndef BINFONT_SD_SPI_SCK_PIN
  #define BINFONT_SD_SPI_SCK_PIN  39
#endif
#ifndef BINFONT_SD_SPI_MOSI_PIN
  #define BINFONT_SD_SPI_MOSI_PIN 38
#endif
#ifndef BINFONT_SD_SPI_MISO_PIN
  #define BINFONT_SD_SPI_MISO_PIN 40
#endif

// Font directory and default font
#ifndef BINFONT_FONT_DIR
  #define BINFONT_FONT_DIR "/font"
#endif

// 注意：开源时不建议把未授权字体文件一并分发。
#ifndef BINFONT_DEFAULT_FONT_PATH
  #define BINFONT_DEFAULT_FONT_PATH "/font/京华老宋体36.bin"
#endif

// Max fonts to enumerate under /font
#ifndef BINFONT_MAX_FONTS
  #define BINFONT_MAX_FONTS 32
#endif

// Preheat text (a lot of glyphs)
#ifndef BINFONT_WARM_TEXT
  #define BINFONT_WARM_TEXT \
    "预热文本：的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说产种面而方后多定行学法所民得经十三之进着等部度家电力里如水化高自二理起小物现实加量都两体制机当使点从业本去把性好应开它合还因由其些然前外天政四日那社义事平形相全表间样与关各重新线内数正心反你明看原又么利比或但质气第向道命此变条只没结解问意建月公无系军很情者最立代想已通并提直题党程展五果料象员革位入常文总次品式活设及管特件长求老头基资边流路级少图山统接知较将组见计别她手角期根论运农指几九区强放决西被干做必战先回则任取据处理府判合选足光适联件布"
#endif

// UI tuning
#ifndef BINFONT_TOUCH_DEBOUNCE_MS
  #define BINFONT_TOUCH_DEBOUNCE_MS 80
#endif
