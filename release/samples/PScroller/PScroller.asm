#import "../includes/c64.asm"

BasicUpstxrt2(start)

.label nextX = $50
.label nextY = $51
.label nextCol = $52
.label anim = $53
.label anim2 = $5e
.label currentRaster = $56
.label spritePattern = $57
.label textCol = $58
.label textIn = $54
.label colOut = $55
.label textVal1 =  $5a
.label textVal2 =  $5b
.label textLines = $5c
.label tempVar = $5d

stxrt:
    jsr cls

    ldx #$1             // enable all the sprites
    stx vic.spriteEnable
    ldx #1
    stx vic.spriteMulticolor
    ldx #$0                 // reset sprites to size 0
    stx vic.spriteXSize
	stx vic.spriteYSize

    ldx #0
    ldx #0
    ldy #0
    jsr $1000

    ldx #0
    stx textIn
    stx colOut
    stx textLines
    stx textCol
    stx anim
    stx anim2
    
   ldx #0
   stx $d020
   ldx #0
   stx $d021
    
    ldx #120
    stx tempVar
!loop:
    jsr UpdateText
    dec tempVar
    ldx tempVar
    bne !loop-

    sei
    
   // wait for stxrt of first frame
frameLoop:
    ldx vic.control1
    bmi frameLoop
    ldx vic.rasterCounter
    cmp #20
    bne frameLoop

    ldx #21
    stx vic.sprite0Y
    stx nextY
    stx currentRaster

   // we are at the stxrt of a frame
    inc anim
    inc anim2
    inc anim2
    inc anim2
    ldx anim2
    stx nextX

    ldx anim
    stx nextCol

    ldx anim
    and #127
    tax
    ldx colors1,x
    stx vic.spriteMulticolor0
    ldx colors2,x
    stx vic.sprite0Color
    ldx colors3,x
    stx vic.spriteMulticolor1
    
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs

    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
   
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs   
    
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
    
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs

    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
    
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
       
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
    
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs
    
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_xs           

    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x

    ldx #$10
    stx $d011

    jsr line_c
    jsr line_xs
    jsr line_c
    jsr line_x

    ldx #$18
    stx $d011

    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x
    jsr line_c
    jsr line_x

    ldx #0
    stx vic.spriteMulticolor0
    stx vic.sprite0Color
    stx vic.spriteMulticolor1

    ldx anim
    lsr
    and #31
    tax
    ldx sprites,x
    stx sprite0Ptr
    stx spritePattern
    jsr UpdateText

    ldx anim
    lsr
    tax
    ldx xwave,x
    stx vic.sprite1Y
    stx vic.sprite2Y
    stx vic.sprite3Y
    stx vic.sprite4Y
    stx vic.sprite5Y
    stx vic.sprite6Y
    stx vic.sprite7Y

    
    jsr $1006
    jmp frameLoop

cls:
    ldx #0
    ldx #32
cls_loop:
    stx $0400,x
    stx $0400+250,x
    stx $0400+500,x
    stx $0400+750,x
    inx
    cpx #250
    bne cls_loop
    rts

extend_borders:
   ldx #$f8
!hack:
   cmp vic.rasterCounter
   bne !hack-

    ldx #$10
    stx $d011

   ldx #$fe
!hack:
   cmp vic.rasterCounter
   bne !hack-

    ldx #$18
    stx $d011
    rts


frame_sync:
    ldx vic.rasterCounter
    cmp #20
    bne frame_sync
    rts

line_x:
    ldx currentRaster
!loop:
    cmp vic.rasterCounter
    beq !loop-
    inc currentRaster

    inc nextX
    ldx nextX
    ldx xwave,x
    stx vic.sprite0X
    rts

line_xs:
    ldx currentRaster
!loop:
    cmp vic.rasterCounter
    beq !loop-
    inc currentRaster

    inc nextX
    ldx nextX
    ldx xwave,x
    stx vic.sprite0X

    ldx spritePattern
    and #31
    tax
    ldx sprites,x
    stx sprite0Ptr
    inc spritePattern

    ldx nextY
    adc #21
    stx nextY
    stx vic.sprite0Y
   
    rts

line_c:
    ldx currentRaster
!loop:
    cmp vic.rasterCounter
    beq !loop-
    inc currentRaster

    ldx vic.rasterCounter
    stx currentRaster

    ldx nextCol
    and #127
    tax
    inc nextCol
    
    ldx colors1,x
    stx vic.spriteMulticolor0
    ldx colors2,x
    stx vic.sprite0Color
    ldx colors3,x
    stx vic.spriteMulticolor1
   
   ldx currentRaster
!wait:
   cmp vic.rasterCounter
   beq !wait-
   rts

line_s:
    ldx currentRaster
!loop:
    cmp vic.rasterCounter
    beq !loop-
    inc currentRaster

    ldx nextY
    adc #21
    stx nextY
    stx vic.sprite0Y
    rts


UpdateText:
    ldx textLines
    bne moreLines

    inc textCol
    ldx textCol
    and #15
    bne colOk
    ldx #1
    stx textCol
colOk:
    // get next text character
    ldx textIn
    inc textIn
    ldx scrollText,x
    bpl moreCharacters

    ldx #0
    ldx scrollText,x
    inx
    stx textIn
    
moreCharacters:
    asl                            // convert character to word offset
    tax
    ldx chars,x
    stx textVal1
    ldx chars+1,x
    stx textVal2
    ldx #5
    stx textLines
    
    // black line to stxrt with
    ldx colOut
    and #127
    tay
    ldx #0
    stx colors1,y
    stx colors2,y
    stx colors3,y
    inc colOut
    rts
    

moreLines:
    ldx colOut
    and #127
    tay
    jsr WriteCol1
    jsr WriteCol2
    jsr WriteCol3
    inc colOut
    dec textLines
    rts

WriteCol1:
    ror textVal2
    ror textVal1
    bcc !noCol+
    ldx textCol
    stx colors1,y
    jmp !skip+
!noCol:
    ldx #0
    stx colors1,y
!skip:
    rts
    
WriteCol2:
    ror textVal2
    ror textVal1
    bcc !noCol+
    ldx textCol
    stx colors2,y
    jmp !skip+
!noCol:
    ldx #0
    stx colors2,y
!skip:
    rts
    
WriteCol3:
    ror textVal2
    ror textVal1
    bcc !noCol+
    ldx textCol
    stx colors3,y
    jmp !skip+
!noCol:
    ldx #0
    stx colors3,y
!skip:
    rts
    
 error1:
     ldx #1
     stx $d021
error1_forever:
     jmp error1_forever

 error2:
     ldx #2
     stx $d021
  error2_forever:
     jmp error2_forever

    
scrollText:
    .text "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@hello@friends[@@this@is@my@vertical@scrolling@demo@which@uses@a@colour@palette@cycling@technique@to@move@pixels@up@through@a@single@mulitplexed@sprite@with@borders@removed@@@@greetings@to@hitman@of@codehq@@@looking@forward@to@visiting@you@in@japan@and@going@to@revision@next@year@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
    .byte 128

chars:
    // space
    // 000
    // 000
    // 000
    // 000
    // 000
    .word 0

    // A
    // 010
    // 101
    // 111
    // 101
    // 101
    .word %101101111101010

    // B
    // 110
    // 101
    // 110
    // 101
    // 110
    .word %011101011101011

    // C
    // 110
    // 101
    // 100
    // 101
    // 110
    .word %011101001101011

    // D
    // 110
    // 101
    // 101
    // 101
    // 110
    .word %011101101101011

    // E
    // 111
    // 100
    // 110
    // 100
    // 111
    .word %111001011001111
    
    // F
    // 111
    // 100
    // 110
    // 100
    // 100
    .word %001001011001111
    
    // G
    // 011
    // 101
    // 100
    // 111
    // 010
    .word %010111001101110
    
    // H
    // 101
    // 101
    // 111
    // 101
    // 101
    .word %101101111101101
    
    // I
    // 010
    // 010
    // 010
    // 010
    // 010
    .word %010010010010010
    
    // J
    // 111
    // 001
    // 001
    // 101
    // 010
    .word %010101100100111
    
    // K
    // 101
    // 110
    // 100
    // 110
    // 101
    .word %101011001011101
    
    // L
    // 100
    // 100
    // 100
    // 100
    // 111
    .word %111001001001001
    
    // M
    // 101
    // 111
    // 101
    // 101
    // 101
    .word %101101101111101
    
    // N
    // 110
    // 101
    // 101
    // 101
    // 101
    .word %101101101101011

    // O
    // 010
    // 101
    // 101
    // 101
    // 010
    .word %010101101101010    
    
    // P
    // 110
    // 101
    // 110
    // 100
    // 100
    .word %001001011101011
    
    // Q
    // 010
    // 101
    // 101
    // 111
    // 011
    .word %110111101101010
    
    // R
    // 110
    // 101
    // 110
    // 101
    // 101
    .word %101101011101011
    
    // S
    // 011
    // 100
    // 010
    // 001
    // 110
    .word %011100010001110
    
    // T
    // 111
    // 010
    // 010
    // 010
    // 010
    .word %010010010010111
    
    // U
    // 101
    // 101
    // 101
    // 101
    // 010
    .word %010101101101101
    
    // V
    // 101
    // 101
    // 101
    // 010
    // 010
    .word %010010101101101
    
    // W
    // 101
    // 101
    // 111
    // 111
    // 101
    .word %101111111101101
    
    // X
    // 101
    // 101
    // 010
    // 101
    // 101
    .word %101101010101101
    
    // Y
    // 101
    // 101
    // 011
    // 001
    // 110
    .word %011100110101101
    
    // Z
    // 111
    // 001
    // 010
    // 100
    // 111
    .word %1110001010100111

   *=$1000-$7e
   .import binary "music.sid"

   *=$2000
colors1:
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0
    .byte 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,0

colors2:
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0
    .byte 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,0

colors3:
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
    .byte 3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,0
   
sprites:
    .byte 128+64+5, 128+64+4, 128+64+4, 128+64+3, 128+64+3, 128+64+2, 128+64+1, 128+64+6
    .byte 128+64+7, 128+64+8, 128+64+9, 128+64+10, 128+64+11, 128+64+10, 128+64+9, 128+64+8
    .byte 128+64+7, 128+64+6, 128+64+1, 128+64+2, 128+64+3, 128+64+3, 128+64+4, 128+64+4
    .byte 128+64+5, 128+64+5, 128+64+5, 128+64+5, 128+64+5, 128+64+5, 128+64+5, 128+64+5

    *=$2200
xwave:
    // Make data for a sine wave
    .for(var i=0;i<256;i++) .byte round(70 + 16*sin(toRadians(360*i/256)) + 16*sin(toRadians(360*i/128)))

   
	* = $3000  "Sprites"

     // sprite 0
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000
	.byte %00000000, %01010101, %00000000,0

     // sprite 1
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000
	.byte %00000000, %01101011, %00000000,0

     // sprite 2
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000
	.byte %00000001, %01101011, %11000000,0

     // sprite 3
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000
	.byte %00000101, %01101011, %11110000,0

     // sprite 4
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100
	.byte %00010101, %10101010, %11111100,0

     // sprite 5
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111
	.byte %01010101, %10101010, %11111111,0

     // sprite 6
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000
	.byte %00000000, %11111111, %00000000,0

     // sprite 7
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000
	.byte %00000000, %11101001, %00000000,0

     // sprite 8
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000
	.byte %00000011, %11101001, %01000000,0

     // sprite 9
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000
	.byte %00001111, %11101001, %01010000,0

     // sprite 10
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100
	.byte %00111111, %10101010, %01010100,0

     // sprite 11
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101
	.byte %11111111, %10101010, %01010101,0

     // sprite 12
     .import binary "logo.raw"

    
    
    
