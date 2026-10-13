# RT-Spark ES8388 Audio Repair

This version repairs the playback path so the firmware follows the RT-Spark
board support package more closely and gives an immediate audible test at boot.

## Root causes fixed

1. **Wrong RT-Spark DAC mixer values**
   - `ES8388_DACCONTROL17` was `0x90`; changed to `0x9C`.
   - `ES8388_DACCONTROL20` was `0x90`; changed to `0x9C`.
   - The official RT-Spark ES8388 BSP uses `0x9C` for its normal playback
     initialization.

2. **Missing ES8388 DAC start sequence**
   - Added `es8388_start_dac()`.
   - It selects DAC playback, performs the state-machine transition when
     needed, powers the DAC/output stages, and clears the DAC mute bit.

3. **Unsafe register overwrite while muting/unmuting**
   - Mute now uses a read/modify/write operation so unrelated
     `DACCONTROL3` bits are preserved.

4. **I2S DMA started before the codec playback path was ready**
   - `audio_init()` now only prepares the synthesizer and zeroed DMA buffer.
   - New `audio_start()` starts DMA only after codec initialization/start.
   - Main startup is now: I2S peripheral configured -> PCM prepared -> ES8388
     initialized -> DAC started -> register readback verified -> I2S DMA started.

5. **No codec register verification**
   - Added `es8388_verify_playback_config()` to read back the critical format,
     mixer, playback-mode and DAC-power registers and confirm the codec is
     really in the intended state.

6. **Boot looked silent even when hardware was healthy**
   - The original firmware deliberately started in `STOPPED` state.
   - This version automatically starts Song 1 after initialization.
  - USER / KEY_LEFT remains pause/resume.

7. **Embedded hardware test updated**
   - The codec hardware test now initializes, starts, sets volume and verifies
     the ES8388 playback configuration.
   - The I2S test explicitly starts DMA before checking its transfer counter.

## Expected boot behavior

With the board connected to a speaker/headphones and the serial monitor at
115200 baud, the successful status line should contain:

```
ES8388 READY
```

Song 1 then starts automatically. The blue playing LED should be active.

## Hardware path used

- ES8388 I2C address: `0x10`
- I2C2: PF1 SCL, PF0 SDA
- I2S3 MCLK: PC7
- I2S3 WS/LRCK: PA15
- I2S3 BCLK/CK: PB3
- I2S3 SD: PB5
- I2S TX DMA: DMA1 Stream 7, Channel 0
- Output: ES8388 LOUT1/ROUT1 -> RT-Spark 3.5 mm audio jack

## Verification performed in this repair environment

- Reviewed the complete firmware audio path and startup order.
- Cross-checked codec initialization/start behavior against the official
  RT-Thread RT-Spark ES8388 and sound drivers.
- Ran an off-target synthesizer smoke test at the firmware's computed 32609 Hz
  sample rate; the generated 440 Hz PCM stream was non-zero with a measured
  peak of 29000, confirming the tone generator itself produces valid PCM.
- Checked edited source delimiter/structure sanity and all project references
  to the new audio-start API.

A physical RT-Spark board and the ARM/PlatformIO toolchain are still required
to prove electrical output at the 3.5 mm jack. The current execution container
does not contain PlatformIO/arm-none-eabi and has no attached RT-Spark board,
so it cannot truthfully perform the final flash-and-listen test here.
