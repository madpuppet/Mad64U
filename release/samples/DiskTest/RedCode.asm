.file [name="redCode.bin", type="prg", segments="redCode"]
.segment redCode[]

* = $c000
{
	ldx #0
	ldy #0
loop:
    lda #5
	stx vic.backgroundColor0
	stx vic.borderColor
    lda #6
	stx vic.backgroundColor0
	stx vic.borderColor
	dex
	bne loop
	dey
	bne loop
	rts
}

