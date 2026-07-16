#import "../includes/c64.asm"

.file [name="blackCode.bin", type="bin", segments="blackCode"]
.segment blackCode[]

* = $c000
{
	ldx #0
	ldy #0
l1:
    lda #00
	sta vic.backgroundColor0
    lda #01
	sta vic.backgroundColor0
    lda #00
	sta vic.backgroundColor0
    lda #02
	sta vic.backgroundColor0
    lda #00
	sta vic.backgroundColor0
    lda #01
	sta vic.backgroundColor0
    lda #00
	sta vic.backgroundColor0
	dey
	bne l1
	stx vic.borderColor
	dex
	bne l1
	rts
}




	
