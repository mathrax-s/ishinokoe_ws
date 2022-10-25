// This sketch is for Seeduino Xiao.
// A0--DAC, A1,A6,A7,A8,A9,A10--touch sensor
//
// Original source is 'Music Box / Wavetable Synthesizer / DDS' for PIC32MX220F032B.
// http://dangerousprototypes.com/forum/index.php?topic=3472.0#p34613
// https://youtu.be/dt3rpgWyKno


#include <TimerTC3.h>   //タイマ割り込み

#include "Adafruit_FreeTouch.h"
Adafruit_FreeTouch qt_1 = Adafruit_FreeTouch(A1, OVERSAMPLE_4, RESISTOR_20K, FREQ_MODE_SPREAD_MEDIAN);
Adafruit_FreeTouch qt_2 = Adafruit_FreeTouch(A6, OVERSAMPLE_4, RESISTOR_20K, FREQ_MODE_SPREAD_MEDIAN);

#include "tune_still_alive.h"
#include "wavetable.h"
#include "pitches.h"

int POT = 8;             // 音のたかさ
int ENVPOT = 5;          // 音のながさ
#define OSCILLATOR_COUNT 32 // 32 和音
#define TICKS_LIMIT 70      // 楽譜よみすすめるスピード
#define CLIP 508            // 音量計算の上限下限（クリップ）

uint16_t increments_pot[ OSCILLATOR_COUNT ];
uint32_t phase_accu_pot[ OSCILLATOR_COUNT ];
uint32_t envelope_positions_envpot[ OSCILLATOR_COUNT ];

uint8_t next_osc = 0;
const uint16_t event_count = sizeof( tune_still_alive ) / sizeof( tune_still_alive[ 0 ] );
const uint32_t sizeof_wt_pot = ( (uint32_t)sizeof( wt ) << POT );
const uint32_t sizeof_wt_sustain_pot = ( (uint32_t)sizeof( wt_sustain ) << POT );
const uint32_t sizeof_wt_attack_pot = ( (uint32_t)sizeof( wt_attack ) << POT );
const uint32_t sizeof_envelope_table_envpot = ( (uint32_t)sizeof( envelope_table ) << ENVPOT );

static const uint8_t scale[] = {
  NOTE_E9, NOTE_G9,
  NOTE_A9, NOTE_B9,
  NOTE_D10, NOTE_E10,
};


float average[6];
float baseline[6];
float diff[6] = {20, 10, 20, 12, 8, 12};
float hysterisis[6] = {10, 10, 10, 10, 10, 10};
int touch[6];
int touch_st[6] ;
int note[6] = {0, 1, 2, 3, 4, 5};
int sound_num = 0;


void setup() {
  //  Serial.begin(115200);
  qt_1.begin();
  qt_2.begin();

  pinMode(2,INPUT_PULLUP);
  pinMode(3,INPUT_PULLUP);
  pinMode(4,INPUT_PULLUP);
  pinMode(5,INPUT_PULLUP);
  pinMode(7,INPUT_PULLUP);
  pinMode(8,INPUT_PULLUP);
  pinMode(9,INPUT_PULLUP);
  pinMode(10,INPUT_PULLUP);

  for (int j = 0; j < 100; j++) {
    for (int i = 0; i < 2; i++) {
      float result = 0;
      switch (i) {
        case 0: result = qt_1.measure(); break;
        case 1: result = qt_2.measure(); break;
      }
      average[i] = (result / 4.0) + average[i] * ( 3.0 / 4.0 );
      baseline[i] = average[i];
    }

  }

  for ( uint8_t osc = 0; osc < OSCILLATOR_COUNT; ++osc ) {
    increments_pot[ osc ] = 0;
    phase_accu_pot[ osc ] = 0;
    envelope_positions_envpot[ osc ] = 0;
  }

  //DACを10bitにセット
  analogWriteResolution(10);
  //32usで割り込み
  TimerTc3.initialize(32);
  //  TimerTc3.initialize(64);
  TimerTc3.attachInterrupt(timerIsr);
}


void loop() {

  for ( int i = 0; i < 2;  i++) {
    float result = 0;
    int counter = 0;
    int t = 0;
    int touch_sim = 0;
    switch (i) {
      case 0: result = qt_1.measure(); break;
      case 1: result = qt_2.measure(); break;
    }
    average[i] = (result / 4.0) + average[i] * ( 3.0 / 4.0 );

    if (average[i] - baseline[i] > diff[i]) {
      touch[i] = 1;
      sound_num = note[i];
    } else {
      touch[i] = 0;
      touch_st[i] = 0;
    }
  }

  //感度調整のための数値を確認
  //  for (int i = 0; i < 6; i++) {
  //    Serial.print(average[i] - baseline[i]);
  //    Serial.print(",");
  //  }
  //  Serial.println();
  //  delay(10);

  for (int i = 0; i < 2; i++) {
    if (touch[i] == 1 && touch_st[i] == 0) {
      touch_st[i] = 1;

      //音を鳴らす
      increments_pot[ next_osc ] = scale_table[ scale[ sound_num ] ];
      phase_accu_pot[ next_osc ] = 0;
      envelope_positions_envpot[ next_osc ] = 0;
      ++next_osc;
      if ( next_osc >= OSCILLATOR_COUNT ) {
        next_osc = 0;
      }

      //音を鳴らす
      increments_pot[ next_osc ] = scale_table[ scale[ sound_num ] ];
      phase_accu_pot[ next_osc ] = 0;
      envelope_positions_envpot[ next_osc ] = 0;
      ++next_osc;
      if ( next_osc >= OSCILLATOR_COUNT ) {
        next_osc = 0;
      }

      //音を鳴らす
      increments_pot[ next_osc ] = scale_table[ scale[ sound_num ] - 12 ] + 4;
      phase_accu_pot[ next_osc ] = 0;
      envelope_positions_envpot[ next_osc ] = 0;
      ++next_osc;
      if ( next_osc >= OSCILLATOR_COUNT ) {
        next_osc = 0;
      }
      //音を鳴らす
      increments_pot[ next_osc ] = scale_table[ scale[ sound_num ] - 24 ] + 32;
      phase_accu_pot[ next_osc ] = 0;
      envelope_positions_envpot[ next_osc ] = 0;
      ++next_osc;
      if ( next_osc >= OSCILLATOR_COUNT ) {
        next_osc = 0;
      }

    }
  }
}


void timerIsr() {

  int32_t value = 0;
  for ( uint8_t osc = 0; osc < OSCILLATOR_COUNT; ++osc ) {
    phase_accu_pot[ osc ] += increments_pot[ osc ];
    if ( phase_accu_pot[ osc ] >= sizeof_wt_pot ) {
      phase_accu_pot[ osc ] -= sizeof_wt_sustain_pot;
    }
    uint16_t phase_accu = ( phase_accu_pot[ osc ] >> POT );
    value += envelope_table[ envelope_positions_envpot[ osc ] >> ENVPOT ] * wt[ phase_accu ];
    if ( phase_accu_pot[ osc ] >= sizeof_wt_attack_pot &&
         envelope_positions_envpot[ osc ] < sizeof_envelope_table_envpot - 1 )
    {
      ++envelope_positions_envpot[ osc ];
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
