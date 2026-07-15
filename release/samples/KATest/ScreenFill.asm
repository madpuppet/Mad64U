#import "../includes/c64.lib"

.var chrin=$ffe4
.var cls=$e544
BasicUpstart2(start)

start:
    lda #0
    sta vic.backgroundColor0
    sta vic.borderColor
    lda #1
    sta $286
    jsr cls
    ldx #$0
introtextloop:
    lda introtext,x
    beq doneintrotext
    sta 1024,x
    inx
    bne introtextloop
doneintrotext:
charloop:
    lda #0
    jsr chrin
    cmp #$00
    beq charloop
    cmp #3
    beq done
    sec
    sbc #64
    ldx #0
innerloop:
    sta 1024,x
    sta 1024+250,x
    sta 1024+500,x
    sta 1024+750,x
    inx
    cpx #250
    bne innerloop
    jmp charloop
done:
    jsr cls
    rts
introtext:
.text "press keys or runstop to quit"
.byte 0

