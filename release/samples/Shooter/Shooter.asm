#import "../includes/c64.lib"

:BasicUpstart2(start)

.label tmp_in = $fc
.label tmp_out = $fe
.label firstRaster = 40

	* = 2064  "Main"
	
start:
    jsr initMap
    jsr initCharSet

    lda #255
    sta sid.V3_FreqLow
    sta sid.V3_FreqHi
    lda #128
    sta sid.V3_Control

	lda #1+2                // enable all the sprites
	sta vic.spriteEnable
	lda #128+64+32+16+8                  // reset sprites to size 0
    sta vic.spriteMulticolor
    sta spriteNumber
    lda #4
	sta vic.spriteYSize
	sta vic.spriteXSize
    lda #128+64
    sta sprite0Ptr+$2400
    sta sprite0Ptr+$2800
    lda #128+64+1
    sta sprite1Ptr+$2400
    sta sprite1Ptr+$2800
    lda #128+64+4
    sta sprite3Ptr+$2400
    sta sprite4Ptr+$2400
    sta sprite5Ptr+$2400
    sta sprite6Ptr+$2400
    sta sprite7Ptr+$2400
    sta sprite3Ptr+$2800
    sta sprite4Ptr+$2800
    sta sprite5Ptr+$2800
    sta sprite6Ptr+$2800
    sta sprite7Ptr+$2800
    lda #0
	sta vic.spriteXMSB		// msb of x
    lda #2
    sta vic.sprite0Color
    lda #1
    sta vic.sprite1Color
    lda #3
    sta vic.sprite2Color
    lda #4
    sta vic.sprite3Color
    sta vic.sprite4Color
    sta vic.sprite5Color
    sta vic.sprite6Color
    sta vic.sprite7Color
    lda #3
    sta vic.spriteMulticolor0
    lda #11
    sta vic.spriteMulticolor1

    lda #0
    sta vic.borderColor
    lda #0
    sta vic.backgroundColor0
    lda #2
    sta vic.backgroundColor1
    lda #7
    sta vic.backgroundColor2

    clc
    lda #255
    ldx #0
initXLoop:
    sta vic.sprite0X,x
    sta vic.sprite4X,x
    inx
    inx
    cpx #8
    bne initXLoop

    lda #20
    ldx #0
initYLoop:
    sta vic.sprite0Y,x
    clc
    adc #20
    inx
    inx
    cpx #16
    bne initYLoop

	sei
	lda #$1b
	sta vic.control1  // clear significant bit in VICs raster register
	ldx #firstRaster
	stx nextLine
    stx vic.rasterCounter  // set raster line to interrupt on (#0)
	ldy #$7f
	sty $dc0d  // switch off interrupts from CIA-1
    Store(irq,$0314)
	lda #$01
    sta vic.interruptEnable  // enable raster interrupts
    cli
	
loop:
	jmp loop

irq:
{
    SetBorder(3)

    jsr SeedRandom

    lda scroll
    sec
    sbc #1
    and #7
    sta scroll
    pha
    asl
    tax
    lda scrollLeftTable,x
    sta scrollFunc+1
    lda scrollLeftTable+1,x
    sta scrollFunc+2
    pla
    pha
    sta vic.control2    
	ldx #0
    stx vic.rasterCounter  // set raster line to interrupt on (#0)
    asl vic.interruptRegister
    pla
    and #3
    bne noParal
    jsr applyParallax
noParal:

scrollFunc:
    jsr $ffff
    jsr GameLoop
    SetBorder(0)
    jmp $ea81
}

RandomSeed:
    .byte 0,0
RandomTable:
    .byte 11,64,127,99, 53,5,254,231, 166,117,1,97, 37,17,171,211

SeedRandom:
    lda sid.Oscillator3Random
    adc RandomSeed
    sta RandomSeed
    rts

GenerateRandom:
    lda sid.Oscillator3Random
    rol
    ror RandomSeed
    adc RandomSeed
    sta RandomSeed
    rts

scrollLeftTable:
    .word scrollLeft1, scrollLeft2, scrollLeft3, scrollLeft4, scrollLeft5, scrollLeft6, scrollLeft7, scrollLeft8

backBuffers:
    .word $2800, $2c00

backBuffersMP:
    .byte $be, $ae

currBackBuffer:
    .word $2800, $2c00

scrollLeft1:
    jsr introduceNewMapData
    rts

scrollLeft2:
    jsr updateMapHeight
    rts

scrollLeft3:
    jsr clearNewMapData
    rts

scrollLeft4:
    jsr copyBackBuffer
    rts

scrollLeft5:
    jsr copyBackBuffer
    rts

scrollLeft6:
    jsr copyBackBuffer
    rts

scrollLeft7:
    jsr copyBackBuffer
    rts

scrollLeft8:
    jsr swapBackBuffers
    jsr copyBackBuffer
    rts

swapBackBuffers:
    // swap buffers & reset the current back buffer pointer
    lda backBuffers+2
    ldx backBuffers
    sta backBuffers
    stx backBuffers+2
    sta currBackBuffer
    stx currBackBuffer+2

    lda backBuffers+2+1
    ldx backBuffers+1
    sta backBuffers+1
    stx backBuffers+2+1
    sta currBackBuffer+1
    stx currBackBuffer+1+2

    lda backBuffersMP+1
    ldx backBuffersMP
    sta backBuffersMP
    stx backBuffersMP+1
    sta vic.memoryPointer
    rts


copyBackBuffer:
{
    ldx currBackBuffer
    lda currBackBuffer+1
    stx loop+4
    sta loop+5
    ldx currBackBuffer+2
    lda currBackBuffer+3
    inx
    stx loop+1
    sta loop+2
    ldy #0
loop:
    lda $2001,y
    sta $2000,y
    iny
    cpy #40*5
    bne loop

    // point to next block
    clc
    lda currBackBuffer
    adc #40*5
    sta currBackBuffer
    lda currBackBuffer+1
    adc #0
    sta currBackBuffer+1

    lda currBackBuffer+2
    adc #40*5
    sta currBackBuffer+2
    lda currBackBuffer+3
    adc #0
    sta currBackBuffer+3
    rts
}

clearNewMapData:
{
    lda backBuffers
    ldx backBuffers+1
    sta outPtr+1
    stx outPtr+2
    ldx #24
    ldy #39
loop:
    lda #0
outPtr:
    sta $2000,y
    clc
    lda outPtr+1
    adc #40
    sta outPtr+1
    lda outPtr+2
    adc #0
    sta outPtr+2
    dex
    bne loop
    rts
}

mapHeight:          // map height and gap size
    .byte 3,3
maxHeight:
    .byte 1,1

change:
    .byte 0,0,$ff,$1

updateMapHeight:
{

{
    jsr GenerateRandom
    and #3
    tax
    lda mapHeight
    clc
    adc change,x
    bne notTooSmall
    lda #1
    jmp notTooBig
notTooSmall:
    cmp #16
    bcc notTooBig
    lda #16
notTooBig:
    sta mapHeight
}

{
    jsr GenerateRandom
    and #3
    tax
    lda mapHeight+1
    clc
    adc change,x
    bne notTooSmall
    lda #1
    jmp notTooBig
notTooSmall:
    cmp #16
    bcc notTooBig
    lda #16
notTooBig:
    sta mapHeight+1
}

    // max bottom = 25 - (top+5) = 20 - top
    lda #18
    sec
    sbc mapHeight
    cmp mapHeight+1
    bcs sizeOk
    sta mapHeight+1
sizeOk:
    rts
}

introduceNewMapData:
{
    lda backBuffers
    ldx backBuffers+1
    sta outPtr+1
    stx outPtr+2
    ldx mapHeight

    ldy #39
loop:
    jsr GenerateRandom
    and #1
    clc
    adc #3
outPtr:
    sta $2000,y
    clc
    lda outPtr+1
    adc #40
    sta outPtr+1
    lda outPtr+2
    adc #0
    sta outPtr+2

    dex
    bne loop

    lda outPtr+1
    sta outPtr2+1
    lda outPtr+2
    sta outPtr2+2
    lda #2
outPtr2:
    sta $2000,y
}

{
    clc
    lda backBuffers
    adc #$c0
    sta outPtr+1
    lda backBuffers+1
    adc #3
    sta outPtr+2
    
    ldx mapHeight+1
    ldy #39
loop:
    jsr GenerateRandom
    and #1
    clc
    adc #3
outPtr:
    sta $2000,y
    sec
    lda outPtr+1
    sbc #40
    sta outPtr+1
    lda outPtr+2
    sbc #0
    sta outPtr+2
    dex
    bne loop

    lda outPtr+1
    sta outPtr2+1
    lda outPtr+2
    sta outPtr2+2
    lda #1
outPtr2:
    sta $2000,y
}
    rts

applyParallax:
{
    ldy #0
loop:
    lda $3800,y

    clc
    ror
    bcc noClear1
    ora #128
noClear1:
    clc
    ror
    bcc noClear2
    ora #128
noClear2:
    sta $3800,y

    pha
    ora $3800+8*5,y
    sta $3800+8,y
    pla
    ora $3800+8*6,y
    sta $3800+16,y

    iny
    cpy #8
    bne loop
    rts
}


initCharSet:
    lda #$ae
    sta vic.memoryPointer
    lda #$00
    sta vic.control2
    rts

initMap:
{
    Store(shooter_map,loop+1)
    Store($2400,loop+4)
    Store($2800,loop+7)

    // copy screen
    ldx #4
xloop:
    ldy #0
loop:
    lda shooter_map,y
    sta $0400,y
    sta $0400,y
    iny
    cpy #250
    bne loop
    clc
    lda loop+1
    adc #250
    sta loop+1
    lda loop+2
    adc #0
    sta loop+2
    lda loop+4
    adc #250
    sta loop+4
    lda loop+5
    adc #0
    sta loop+5
    lda loop+7
    adc #250
    sta loop+7
    lda loop+8
    adc #0
    sta loop+8
    dex
    bne xloop

    // set color
    ldy #0
    lda #14
loop2:
    sta $d800,y
    sta $d800+250*1,y
    sta $d800+250*2,y
    sta $d800+250*3,y
    iny
    cpy #250
    bne loop2
    rts
}    

GameLoop:
{
    jsr UpdatePlayerPos
    jsr UpdatePlayerSpritePos
    jsr UpdateEnemySprite
    jsr UpdateBullet
    jsr UpdateBulletSprite
    rts
}
  
UpdatePlayerPos:
{
    ldx playerY                 // current Y position
    lda cia1.dataPortA          // cia chip holds joypad u/d/l/r/fire
    tay
    and #1                      // if UP then ZERO will be clear
    bne notUp                   // branch if player not pushing up
    dex                         // move up
    cpx #50                     // check we are not higher than 10
    bcs notUp                   // branch if not higher
    ldx #50                     // clamp to 10
notUp:
    tya
    and #2                      // if DOWN then ZERO will be clear
    bne notDown                 // branch if player not pushing down
    inx
    cpx #230
    bcc notDown
    ldx #230
notDown:
    stx playerY

    ldx playerX                 // current X position
    lda cia1.dataPortA          // cia chip holds joypad l/r/u/d/fire
    tay
    and #4                      // if UP then ZERO will be clear
    bne notLeft                   // branch if player not pushing up
    dex                         // move up
    cpx #30                     // check we are not higher than 10
    bcs notLeft                   // branch if not higher
    ldx #30                     // clamp to 10
notLeft:
    tya
    and #8                      // if DOWN then ZERO will be clear
    bne notRight                 // branch if player not pushing down
    inx
    cpx #254
    bcc notRight
    ldx #254
notRight:
    stx playerX
    rts
}

UpdatePlayerSpritePos:
{
    lda playerY
    sta vic.sprite0Y
    sta vic.sprite1Y

    lda playerX
    sta vic.sprite0X
    sta vic.sprite1X
    rts
}

UpdateEnemySprite:
{
    ldx enemyAnim
    inx
    stx enemyAnim
    txa
    lsr
    lsr
    and #1
    clc
    adc #128+64+4
    sta sprite3Ptr+$2400
    sta sprite4Ptr+$2400
    sta sprite5Ptr+$2400
    sta sprite6Ptr+$2400
    sta sprite7Ptr+$2400
    sta sprite3Ptr+$2800
    sta sprite4Ptr+$2800
    sta sprite5Ptr+$2800
    sta sprite6Ptr+$2800
    sta sprite7Ptr+$2800
    rts
}

UpdateBullet:
{
    lda bulletX
    bne activeBullet
    lda cia1.dataPortA          // cia chip holds joypad l/r/u/d/fire
    ldx lastCIA
    sta lastCIA
    and #16
    bne fireUp
    txa
    and #16
    beq fireUp

    // no bullet and button pressed, so fire a bullet
    lda playerX
    lsr
    clc
    adc #12
    sta bulletX
    lda playerY
    sbc #9
    sta bulletY
    lda vic.spriteEnable        // enable the sprite
	ora #4
	sta vic.spriteEnable
    lda #128+64+7
    sta bulletSprite
    rts

activeBullet:
    lda bulletX
    clc
    adc #24
    cmp #230
    bcc bulletStillAlive
    lda vic.spriteEnable        // disable the sprite
	and #255-4
	sta vic.spriteEnable
    lda #0

bulletStillAlive:
    sta bulletX
    
fireUp:
    rts
}

UpdateBulletSprite:
{
    lda bulletX
    bne activeBullet
    rts
activeBullet:
    ldx bulletSprite
    cpx #128+64+11
    beq animComplete
    inx
    stx bulletSprite
    stx sprite2Ptr+$2400
    stx sprite2Ptr+$2800
animComplete:
    lda bulletX
    bpl noHigh
    lda vic.spriteXMSB
    ora #4
    sta vic.spriteXMSB
    bne doneMSB
noHigh:
    lda vic.spriteXMSB
    and #255-4
    sta vic.spriteXMSB
doneMSB:
    lda bulletX
    asl
    sta vic.sprite2X
    lda bulletY
    sta vic.sprite2Y
    rts
}


// GAME DATA

nextLine:
    .byte 0
spriteNumber:
    .byte 0
scroll:
    .byte 0
pos:
    .byte 0,0

playerX: .byte 40           // player ship X pos
playerY: .byte 100          // player ship Y pos
bulletX: .byte 0            // bullet x pos / 2
bulletY: .byte 0            // bullet y pos
bulletSprite: .byte 0
lastCIA: .byte 0
enemyAnim: .byte 0          // enemy animation (x8)


    * = $2800 "Screens"

    * = $3000 "Sprite"

.import binary "Shooter.raw"

    * = $3800 "CharSet"
shooter_chars:
.import binary "shooter_chars.raw"

    * = $4000 "Map"

shooter_map:
.import binary "shooter_map.raw"
