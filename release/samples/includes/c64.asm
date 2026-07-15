#importonce

.pseudocommand NextColorBorder {
    inc $d020
}

.pseudocommand NextColorBack {
    inc $d021
}

.macro Break() {
    .byte 02
}

.macro Store(src,dest) {
	lda #<src
	sta dest
	lda #>src
	sta dest+1
}

.macro SetBorder(col) {
    lda #col
    sta vic.borderColor
}

.macro SetBackground(col) {
    lda #col
    sta vic.backgroundColor0
}

.macro Swap32(loc1,loc2)
{
    lda loc1
    ldx loc2
    sta loc2
    stx loc1
    lda loc1+1
    ldx loc2+1
    sta loc2+1
    stx loc1+1
}

.label sprite0Ptr = $7f8
.label sprite1Ptr = $7f9
.label sprite2Ptr = $7fa
.label sprite3Ptr = $7fb
.label sprite4Ptr = $7fc
.label sprite5Ptr = $7fd
.label sprite6Ptr = $7fe
.label sprite7Ptr = $7ff

.namespace vic {
    .label sprite0X = $d000
    .label sprite0Y = $d001
    .label sprite1X = $d002
    .label sprite1Y = $d003
    .label sprite2X = $d004
    .label sprite2Y = $d005
    .label sprite3X = $d006
    .label sprite3Y = $d007
    .label sprite4X = $d008
    .label sprite4Y = $d009
    .label sprite5X = $d00a
    .label sprite5Y = $d00b
    .label sprite6X = $d00c
    .label sprite6Y = $d00d
    .label sprite7X = $d00e
    .label sprite7Y = $d00f
    .label spriteXMSB = $d010

    .label control1 = $d011
    // 0..2 - vertical scroll
    // 3    - screen height - 24 / 25
    // 4    - Blank Screen to Border Color:  0 = Blank
    // 5    - Bitmap Mode: 1 = Enable
    // 6    - Extended Color Text: 1 = Enable
    // 7    - Raster Compare

    .label rasterCounter = $d012
    .label lightPenX = $d013
    .label lightPenY = $d014
    .label spriteEnable = $d015

    .label control2 = $d016
    // 0..2 - horizontal raster scroll
    // 3    - screen width 0 = 38, 1 = 40
    // 4    - multicolor mode
    // 5    - MUST BE 0

    .label spriteYSize = $d017
    .label memoryPointer = $d018
    // 1..3 - Character Dot-Data Base Address
    // 4..7 - Video Matrix Base Address

    .label interruptRegister = $d019
    // 0    - Raster Compare IRQ
    // 1    - Sprite to Background IRQ
    // 2    - Sprite to Sprite Collision IRQ
    // 3    - Lightpen IRQ
    // 7    - Set on any enabled VIC IRQ condition

    .label interruptEnable = $d01a
    .label spritePriority = $d01b
    .label spriteMulticolor = $d01c
    .label spriteXSize = $d01d
    .label spriteToSpriteCollision = $d01e
    .label spriteToDataCollision = $d01f
    .label borderColor = $d020
    .label backgroundColor0 = $d021
    .label backgroundColor1 = $d022
    .label backgroundColor2 = $d023
    .label backgroundColor3 = $d024
    .label spriteMulticolor0 = $d025
    .label spriteMulticolor1 = $d026
    .label sprite0Color = $d027
    .label sprite1Color = $d028
    .label sprite2Color = $d029
    .label sprite3Color = $d02a
    .label sprite4Color = $d02b
    .label sprite5Color = $d02c
    .label sprite6Color = $d02d
    .label sprite7Color = $d02e
    .label colorMemory = $d800
}

.namespace cia1 {
    .label dataPortA = $dc00
    // 0..7 Keyboard Matrix
    // 0..4 Joyport2 l/r/u/d/FIRE  0==activated
    // 4    Lightpen FIRE
    // 2..3 Paddle FIRE Buttons
    // 6..7 Paddle control %01 = Paddle A, %10 = Paddle B
    .label dataPortB = $dc01
    .label dataDirectionA = $dc02
    .label dataDirectionB = $dc03
    .label timerALow = $dc04
    .label timerAHigh = $dc05
    .label timerBLow = $dc06
    .label timerBHigh = $dc07
    .label clockTenths = $dc08
    .label clockSeconds = $dc09
    .label clockMinutes = $dc0a
    .label clockHours = $dc0b
    .label serialShift = $dc0c
    .label interruptControl = $dc0d
    .label controlTimerA = $dc0e
    .label controlTimerB = $dc0f
}

.namespace cia2 {
    .label dataPortA = $dd00
    // 0..1 Select VIC bank %00 = $c000-$ffff, %01 $8000-$BFFF, %02 %4000-$7FFF
    .label dataPortB = $dc01
    .label dataDirectionA = $dc02
    .label dataDirectionB = $dc03
    .label timerALow = $dc04
    .label timerAHigh = $dc05
    .label timerBLow = $dc06
    .label timerBHigh = $dc07
    .label clockTenths = $dc08
    .label clockSeconds = $dc09
    .label clockMinutes = $dc0a
    .label clockHours = $dc0b
    .label serialShift = $dc0c
    .label interruptControl = $dc0d
    .label controlTimerA = $dc0e
    .label controlTimerB = $dc0f
}
 
.namespace rom {
    .label SETLFS = $FFBA
    .label SETNAM = $FFBD
    .label LOAD = $FFD5
}

.namespace sid {
    .label V1_FreqLow = $d400
    .label V1_FreqHi = $d401
    .label V1_PulseWaveformWidthLo = $d402
    .label V1_PulseWaveformWidthHi = $d403
    .label V1_Control = $d404
    // 0 Gate Bit
    // 1 Sync 1 with 3
    // 2 Ring Mod 1 with 3
    // 3 Test Bit: 1 = disable osc 1
    // 4 Triangle Waveform
    // 5 Sawtooth Waveform
    // 6 Pulse Waveform
    // 7 Random Noise Waveform
    .label V1_Envelope_AD = $d405
    // 0..3 Decoy Cycle Duration
    // 4..7 Attack Cycle Duration
    .label V1_Envelope_SR = $d406
    // 0..3 Release Cycle Duration
    // 4..7 Sustain Cycle Duration

    .label V2_FreqLow = $d407
    .label V2_FreqHi = $d408
    .label V2_PulseWaveformWidthLo = $d409
    .label V2_PulseWaveformWidthHi = $d40a
    .label V2_Control = $d40b
    .label V2_Envelope_AD = $d40c
    .label V2_Envelope_SR = $d40d

    .label V3_FreqLow = $d40e
    .label V3_FreqHi = $d40f
    .label V3_PulseWaveformWidthLo = $d410
    .label V3_PulseWaveformWidthHi = $d411
    .label V3_Control = $d412
    .label V3_Envelope_AD = $d413
    .label V3_Envelope_SR = $d414
    
    .label FilterCutoffFreqLo = $d415
    .label FilterCutoffFreqHi = $d416
    .label FilterResonance = $d417
    .label FilterModeAndVolume = $d418
    .label AnalogDigitalConvert1 = $d419
    .label AnalogDigitalConvert2 = $d41a
    .label Oscillator3Random = $d41b
    .label Envelope3Output = $d41c
}

