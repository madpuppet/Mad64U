#import "../includes/c64.asm"

.file [name="Bootcode.bin", type="prg", segments="bootCode"]
.segment bootCode[]

.label ptr = $fe

BasicUpstart2(start)
start:
    lda #0
    sta vic.backgroundColor0
    sta vic.borderColor

	lda #0
	jsr loadBlock
	lda #1
	jsr loadBlock
	lda #2
	jsr loadBlock
	rts
	
file001:
.text "001"
file002:
.text "002"
file003:
.text "003"
fileAddrs:
.wo $c000,$c000,$c000
fileNames:
.wo file001,file002,file003

loadBlock:
    sta vic.backgroundColor0

	clc
	asl
	pha
	tax
	lda fileNames+1,x
	tay
	lda fileNames,x
	tax
	lda #3
	jsr rom.SETNAM

	lda #1
	ldx #8
	ldy #0
	jsr rom.SETLFS

	pla
	tax
	lda fileAddrs+1,x
	sta callFunc+2
	tay
	lda fileAddrs,x
	sta callFunc+1
	tax
	lda #0
	jsr rom.LOAD

callFunc:    
    jsr $0000
	rts

