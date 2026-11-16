// Decompress raw LZSA2 block.
// Create one with lzsa -r -f2 <original_file> <compressed_file>
//
// in:
// * LZSA_SRC_LO and LZSA_SRC_HI contain the compressed raw block address
// * LZSA_DST_LO and LZSA_DST_HI contain the destination buffer address
//
// out:
// * LZSA_DST_LO and LZSA_DST_HI contain the last decompressed byte address, +1

.label NIBCOUNT = $FC                  // zero-page location for temp offset

DECOMPRESS:
   ldy #$00
   sty NIBCOUNT

DECODE_TOKEN:
   jsr GETSRC                          // read token byte: XYZ|LL|MMM
   pha                                 // preserve token on stack

   and #$18                            // isolate literals count (LL)
   beq NO_LITERALS                     // skip if no literals to copy
   lsr                                 // shift literals count into place
   lsr
   lsr
   cmp #$03                            // LITERALS_RUN_LEN_V2?
   bcc PREPARE_COPY_LITERALS           // if less, count is directly embedded in token

   jsr GETNIBBLE                       // get extra literals length nibble
                                       // add nibble to len from token
   adc #$02                            // (LITERALS_RUN_LEN_V2) minus carry
   cmp #$12                            // LITERALS_RUN_LEN_V2 + 15 ?
   bcc PREPARE_COPY_LITERALS           // if less, literals count is complete

   jsr GETSRC                          // get extra byte of variable literals count
                                       // the carry is always set by the CMP above
                                       // GETSRC doesn't change it
   sbc #$EE                            // overflow?

PREPARE_COPY_LITERALS:
   tax
   bcc PREPARE_COPY_LITERALS_HIGH      // if not, literals count is complete

                                       // handle 16 bits literals count
                                       // literals count = directly these 16 bits
   jsr GETLARGESRC                     // grab low 8 bits in X, high 8 bits in A
   tay                                 // put high 8 bits in Y

PREPARE_COPY_LITERALS_HIGH:
   txa
   beq COPY_LITERALS
   iny

COPY_LITERALS:
   jsr GETPUT                          // copy one byte of literals
   dex
   bne COPY_LITERALS
   dey
   bne COPY_LITERALS
   
NO_LITERALS:
   pla                                 // retrieve token from stack
   pha                                 // preserve token again
   asl
   bcs REPMATCH_OR_LARGE_OFFSET        // 1YZ: rep-match or 13/16 bit offset

   asl                                 // 0YZ: 5 or 9 bit offset
   bcs OFFSET_9_BIT         
    
                                       // 00Z: 5 bit offset

   ldx #$FF                            // set offset bits 15-8 to 1

   jsr GETCOMBINEDBITS                 // rotate Z bit into bit 0, read nibble for bits 4-1
   ora #$E0                            // set bits 7-5 to 1
   bne GOT_OFFSET_LO                   // go store low byte of match offset and prepare match
   
OFFSET_9_BIT:                          // 01Z: 9 bit offset
   rol                                 // carry: Z bit; A: xxxxxxx1 (carry known set from BCS OFFSET_9_BIT)
   adc #$00                            // if Z bit is set, add 1 to A (bit 0 of A is now 0), otherwise bit 0 is 1
   ora #$FE                            // set offset bits 15-9 to 1. reversed Z is already in bit 0
   bne GOT_OFFSET_HI                   // go store high byte, read low byte of match offset and prepare match
                                       // (*same as JMP GOT_OFFSET_HI but shorter)

REPMATCH_OR_LARGE_OFFSET:
   asl                                 // 13 bit offset?
   bcs REPMATCH_OR_16_BIT              // handle rep-match or 16-bit offset if not

                                       // 10Z: 13 bit offset

   jsr GETCOMBINEDBITS                 // rotate Z bit into bit 8, read nibble for bits 12-9
   adc #$DE                            // set bits 15-13 to 1 and substract 2 (to substract 512)
   bne GOT_OFFSET_HI                   // go store high byte, read low byte of match offset and prepare match
                                       // (*same as JMP GOT_OFFSET_HI but shorter)

REPMATCH_OR_16_BIT:                    // rep-match or 16 bit offset
   bmi REP_MATCH                       // reuse previous offset if so (rep-match)
   
                                       // 110: handle 16 bit offset
   jsr GETSRC                          // grab high 8 bits
GOT_OFFSET_HI:
   tax
   jsr GETSRC                          // grab low 8 bits
GOT_OFFSET_LO:
   sta OFFSLO                          // store low byte of match offset
   stx OFFSHI                          // store high byte of match offset

REP_MATCH:

   // Forward decompression - add match offset

   clc                                 // add dest + match offset
   lda PUTDST+1                        // low 8 bits
.label OFFSLO = *+1
   adc #$AA
   sta COPY_MATCH_LOOP+1               // store back reference address
.label OFFSHI = *+1
   lda #$AA                            // high 8 bits
   adc PUTDST+2
   sta COPY_MATCH_LOOP+2               // store high 8 bits of address
   
   pla                                 // retrieve token from stack again
   and #$07                            // isolate match len (MMM)
   adc #$01                            // add MIN_MATCH_SIZE_V2 and carry
   cmp #$09                            // MIN_MATCH_SIZE_V2 + MATCH_RUN_LEN_V2?
   bcc PREPARE_COPY_MATCH              // if less, length is directly embedded in token

   jsr GETNIBBLE                       // get extra match length nibble
                                       // add nibble to len from token
   adc #$08                            // (MIN_MATCH_SIZE_V2 + MATCH_RUN_LEN_V2) minus carry
   cmp #$18                            // MIN_MATCH_SIZE_V2 + MATCH_RUN_LEN_V2 + 15?
   bcc PREPARE_COPY_MATCH              // if less, match length is complete

   jsr GETSRC                          // get extra byte of variable match length
                                       // the carry is always set by the CMP above
                                       // GETSRC doesn't change it
   sbc #$E8                            // overflow?

PREPARE_COPY_MATCH:
   tax
   bcc PREPARE_COPY_MATCH_Y            // if not, the match length is complete
   beq DECOMPRESSION_DONE              // if EOD code, bail

                                       // Handle 16 bits match length
   jsr GETLARGESRC                     // grab low 8 bits in X, high 8 bits in A
   tay                                 // put high 8 bits in Y

PREPARE_COPY_MATCH_Y:
   txa
   beq COPY_MATCH_LOOP
   iny

COPY_MATCH_LOOP:
   lda $AAAA                           // get one byte of backreference
   jsr PUTDST                          // copy to destination

   // Forward decompression -- put backreference bytes forward

   inc COPY_MATCH_LOOP+1
   bne GETMATCH_DONE
   inc COPY_MATCH_LOOP+2
GETMATCH_DONE:

   dex
   bne COPY_MATCH_LOOP
   dey
   bne COPY_MATCH_LOOP
   jmp DECODE_TOKEN

GETCOMBINEDBITS:
   eor #$80
   asl
   php

   jsr GETNIBBLE                       // get nibble into bits 0-3 (for offset bits 1-4)
   plp                                 // merge Z bit as the carry bit (for offset bit 0)
   rol                                 // nibble -> bits 1-4; carry(!Z bit) -> bit 0 ; carry cleared
DECOMPRESSION_DONE:
   rts

GETNIBBLE:
.label NIBBLES = *+1
   lda #$AA
   lsr NIBCOUNT
   bcs HAS_NIBBLES

   inc NIBCOUNT
   jsr GETSRC                          // get 2 nibbles
   sta NIBBLES
   lsr 
   lsr 
   lsr 
   lsr 
   sec

HAS_NIBBLES:
   and #$0F                            // isolate low 4 bits of nibble
   rts

   // Forward decompression -- get and put bytes forward

GETPUT:
   jsr GETSRC
PUTDST:
.label LZSA_DST_LO = *+1
.label LZSA_DST_HI = *+2
   sta $AAAA
   inc PUTDST+1
   bne PUTDST_DONE
   inc PUTDST+2
PUTDST_DONE:
   rts

GETLARGESRC:
   jsr GETSRC                          // grab low 8 bits
   tax                                 // move to X
                                       // fall through grab high 8 bits
GETSRC:
.label LZSA_SRC_LO = *+1
.label LZSA_SRC_HI = *+2
   lda $AAAA
   inc GETSRC+1
   bne GETSRC_DONE
   inc GETSRC+2
GETSRC_DONE:
   rts

