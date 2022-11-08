// タイマ割り込み
#include <TimerTC3.h>

// タッチセンサライブラリ
#include "Adafruit_FreeTouch.h"
Adafruit_FreeTouch qt_1 = Adafruit_FreeTouch(A1, OVERSAMPLE_4, RESISTOR_20K, FREQ_MODE_SPREAD_MEDIAN);
Adafruit_FreeTouch qt_2 = Adafruit_FreeTouch(A6, OVERSAMPLE_4, RESISTOR_20K, FREQ_MODE_SPREAD_MEDIAN);

// 波形テーブル
#include "wavetable.h"
// 音階名 定義ファイル
#include "pitches.h"

// センサ数
#define MAXS 2

// 和音数（1つの音で、同時に鳴らす音）
#define CH 2

int POT = 8;                // 音のたかさ
int ENVPOT = 6;             // 音のながさ
#define OSC_COUNT 8        // 12 和音 x 2
#define TICKS_LIMIT 300     // 楽譜よみすすめるスピード
#define CLIP 508            // 音量計算の上限下限（クリップ）

uint16_t increments_pot[CH][ OSC_COUNT ];
uint32_t phase_accu_pot[CH][ OSC_COUNT ];
uint32_t envelope_positions_envpot[3][ OSC_COUNT ];

uint8_t next_osc = 0;

const uint32_t sizeof_sin_pot = ( (uint32_t)sizeof( sin_wave ) << POT );
const uint32_t sizeof_envelope_table_envpot = ( (uint32_t)sizeof( envelope_table ) << ENVPOT );

static const uint8_t scale[CH][5] = {
  {NOTE_D8, NOTE_E8, NOTE_G8, NOTE_A8, NOTE_B8},
  {NOTE_G8, NOTE_A8, NOTE_B8, NOTE_D9, NOTE_E9},
};


float average[MAXS];
float baseline[MAXS];
float diff[2][MAXS] = {{20, 5}, {20, 5}};
float hyst[MAXS] = {5, 5};
int   touch[MAXS];
int   touch_st[MAXS] ;
int   sound_num = 0;
int   mode = 0;

uint16_t ticks[MAXS];
int   note_count[MAXS];
int   dip[8] = {4, 5, 3, 2, 9, 7, 8, 10};
int SWITCH[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int toneMode = 0;

int unari;

void setup() {
  // タッチセンサ 開始
  qt_1.begin();
  qt_2.begin();

  // DIPスイッチピンの設定
  for (int i = 0; i < 8; i++) {
    pinMode(dip[i], INPUT_PULLUP);
  }

  // タッチセンサのキャリブレーション
  for (int j = 0; j < 100; j++) {
    for (int i = 0; i < MAXS; i++) {
      float result = 0;
      switch (i) {
        case 0: result = qt_1.measure(); break;
        case 1: result = qt_2.measure(); break;
      }
      average[i] = (result / 4.0) + average[i] * ( 3.0 / 4.0 );
      baseline[i] = average[i];
    }
  }

  // サウンド生成のための変数初期化
  for (int i = 0; i < CH; i++) {
    for ( uint8_t osc = 0; osc < OSC_COUNT; osc++ ) {
      increments_pot[i][ osc ] = 0;
      phase_accu_pot[i][ osc ] = 0;
      envelope_positions_envpot[i][ osc ] = 0;
    }
  }

  // DACを10bitにセット
  analogWriteResolution(10);
  // 32usでタイマー割り込み
  TimerTc3.initialize(32);
  TimerTc3.attachInterrupt(timerIsr);
}



//
void loop() {
  // DIPスイッチ情報更新
  switchCheck();

  // タッチセンサチェック
  for ( int i = 0; i < MAXS;  i++) {
    float result = 0;
    int counter = 0;
    int t = 0;
    int touch_sim = 0;

    switch (i) {
      case 0: result = qt_1.measure(); break;
      case 1: result = qt_2.measure(); break;
    }

    // 4回平均する
    average[i] = (result / 4.0) + average[i] * ( 3.0 / 4.0 );

    // タッチしたかどうか調べる
    float threshold = (diff[i][0] + hyst[i]);
    if (i == 0) {
      if (SWITCH[3] == 1) {
        threshold = (diff[i][1] + hyst[i]);
      }
    } else if (i == 1) {
      if (SWITCH[4] == 1) {
        threshold = (diff[i][1] + hyst[i]);
      }
    }
    if (average[i] - baseline[i] > threshold) {
      if (touch[i] == 0) {
        touch[i] = 1;
        ticks[i] = 0;
      }
    }
  }

  // センサの数だけ
  for (int i = 0; i < MAXS; i++) {
    // 時間を進める
    ticks[i]++;

    //
    if ( ticks[i] >= TICKS_LIMIT ) {
      ticks[i] = 0;

      if (touch[i] == 1 && touch_st[i] == 0) {

        if (SWITCH[note_count[i]] == 1) {

          // 8番スイッチのオンオフで、2つの音を入れ替える
          if (SWITCH[7] == 0) {

            ///音を鳴らす （
            // wave 1
            increments_pot[i][ next_osc ] = scale_table[ scale[i][ note_count[i] ] ];
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }
            increments_pot[i][ next_osc ] = scale_table[ scale[i][ note_count[i] ] - 24];
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }

            //音を鳴らす
            // wave 2
            increments_pot[i][ next_osc ] = scale_table[ scale[i][ note_count[i] ] ]  + unari;
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }

          } else {

            ///音を鳴らす
            // wave 1
            increments_pot[i][ next_osc ] = scale_table[ scale[1 - i][ note_count[i] ] ] ;
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }

            increments_pot[i][ next_osc ] = scale_table[ scale[1 - i][ note_count[i] ] - 24];
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }

            //音を鳴らす
            // wave 2
            increments_pot[i][ next_osc ] = scale_table[ scale[1 - i][ note_count[i] ] ]  + unari;
            phase_accu_pot[i][ next_osc ] = 0;
            envelope_positions_envpot[i][ next_osc ] = 0 + (note_count[i]*(50<<6));
            ++next_osc;
            if ( next_osc >= OSC_COUNT ) {
              next_osc = 0;
            }
          }

        }

        if (note_count[i] >= 2) {
          note_count[i] = 0;
          touch[i] = 0;
          touch_st[i] = 0;
        } else {
          note_count[i]++;
        }

      }
    }
  }

  //  //感度調整のための数値を確認
  //  for (int i = 0; i < 2; i++) {
  //    Serial.print(average[i] - baseline[i]);
  //    Serial.print(",");
  //  }
  //  Serial.println();

}

// DIPスイッチを監視してSWITCH配列に格納する
void switchCheck() {
  for (int i = 0; i < 8; i++) {
    SWITCH[i] = !digitalRead(dip[i]);
  }

  if (SWITCH[5] == 0 && SWITCH[6] == 0) {
    toneMode = 0;
  } else if (SWITCH[5] == 1 && SWITCH[6] == 0) {
    toneMode = 1;
  } else if (SWITCH[5] == 0 && SWITCH[6] == 1) {
    toneMode = 2;
  } else if (SWITCH[5] == 1 && SWITCH[6] == 1) {
    toneMode = 3;
  }

  switch (toneMode) {
    case 0:
      unari = 48;
      break;
    case 1:
      unari = 48;
      break;
    case 2:
      unari = 144;
      break;
    case 3:
      unari = 144;
      break;
  }
}


void timerIsr() {

  int32_t value = 0;

  for (int i = 0; i < CH; i++) {
    for ( uint8_t osc = 0; osc < OSC_COUNT; osc++ ) {

      phase_accu_pot[i][ osc ] += increments_pot[i][ osc ];
      if ( phase_accu_pot[i][ osc ] >= sizeof_sin_pot ) {
        phase_accu_pot[i][ osc ] -= sizeof_sin_pot;
      }

      uint16_t phase_accu = ( phase_accu_pot[i][ osc ] >> POT );

      switch (toneMode) {
        case 0:
        case 2:
          value += envelope_table[ envelope_positions_envpot[i][ osc ] >> ENVPOT ] * sin_wave[ phase_accu ];
          break;
        case 1:
        case 3:
          value += envelope_table[ envelope_positions_envpot[i][ osc ] >> ENVPOT ] * sin2_wave[ phase_accu ];
          break;
      }

      if ( envelope_positions_envpot[i][ osc ] < sizeof_envelope_table_envpot - 1 )
      {
        envelope_positions_envpot[i][ osc ]++;
      }
    }
  }

  //音の計算が、+512 ~ -511 になるようにする
  value >>= 10; // envelope_table resolution

  if ( value > CLIP ) {
    value = CLIP;
  } else if ( value < -CLIP ) {
    value = -CLIP;
  }

  //DAC は、0 ~ 1023 なので整える
  uint16_t dac = (value + 512);
  analogWrite(A0, dac);
}
