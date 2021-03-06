; inflate - uncompress data stored in the DEFLATE format
; by Piotr Fusik <fox@scene.pl>
; Last modified: 2007-06-17

; inflate is 509 bytes of code and initialized data
; inflate_data is 764 bytes of uninitialized data
; inflate_zp is 10 bytes on page zero

; SNES version by Mic, 2011-2013

.include "hdr.asm"
.include "snes_io.inc"
.include "myth_io.inc"

.bank 3 slot 0
.section "text_inflate"


.EQU inflate_zp $80


; Pointer to compressed data
.EQU inputPointer                    inflate_zp    ; 3 bytes

; Pointer to uncompressed data
.EQU outputPointer                   inflate_zp+3  ; 3 bytes

; Local variables

.EQU getBit_buffer                   inflate_zp+6  ; 1 byte

.EQU getBits_base                    inflate_zp+7  ; 1 byte
.EQU inflateStoredBlock_pageCounter  inflate_zp+7  ; 1 byte

.EQU inflateCodes_sourcePointer      inflate_zp+8  ; 3 bytes

.EQU inflateDynamicBlock_lengthIndex inflate_zp+11  ; 1 byte
.EQU inflateDynamicBlock_lastLength	inflate_zp+12  ; 1 byte
.EQU inflateDynamicBlock_tempCodes   inflate_zp+12  ; 1 byte

.EQU inflateCodes_lengthMinus2       inflate_zp+13  ; 1 byte
.EQU inflateDynamicBlock_allCodes    inflate_zp+13  ; 1 byte

.EQU inflateCodes_primaryCodes       inflate_zp+14  ; 1 byte

;.EQU wordOutputPointer               inflate_zp+16  ; 3 bytes

; Argument values for getBits
.EQU GET_1_BIT                       $81
.EQU GET_2_BITS                      $82
.EQU GET_3_BITS                      $84
.EQU GET_4_BITS                      $88
.EQU GET_5_BITS                      $90
.EQU GET_6_BITS                      $a0
.EQU GET_7_BITS                      $c0

; Maximum length of a Huffman code
.EQU MAX_CODE_LENGTH                 15

; Huffman trees
.EQU TREE_SIZE                       MAX_CODE_LENGTH+1
.EQU PRIMARY_TREE                    0
.EQU DISTANCE_TREE                   TREE_SIZE

; Alphabet
.EQU LENGTH_SYMBOLS                  1+29+2
.EQU DISTANCE_SYMBOLS                30
.EQU CONTROL_SYMBOLS                 LENGTH_SYMBOLS+DISTANCE_SYMBOLS
.EQU TOTAL_SYMBOLS                   256+CONTROL_SYMBOLS

.EQU INFL_CODE_OFS					$DE0000


inflate_ram_code_begin:



; Uncompress DEFLATE stream starting from the address stored in inputPointer
; to the memory starting from the address stored in outputPointer


; DWORD inflate(DWORD dest, DWORD src)
;
.EQU _inflate_save_regs 7
.EQU _inflate_dest 4+_inflate_save_regs
.EQU _inflate_src _inflate_dest+4
;
inflate:
	php
	rep 	#$30
	pha
	phx
	phy
	stz	tcc__r0
	stz	tcc__r1
	lda	#2
	sta 	tcc__r0h
	sep 	#$30	; 8-bit A/X/Y

	LDA    #$20      		; OFF A21
	STA.L  MYTH_GBAC_ZIO
	JSR    SET_NEOCMA  		;
	JSR    SET_NEOCMB  		;
	JSR    SET_NEOCMC  		; ON_NEO CARD A24 & A25 + SA16 & SA17
	LDA    #$01
	STA.L  MYTH_EXTM_ON  	; A25,A24 ON
	LDA    #$04       		; COPY MODE !
	STA.L  MYTH_OPTION_IO
	LDA    #$01       		; PSRAM WE ON !
	STA.L  MYTH_WE_IO
	LDA    #$F8
	STA.L  MYTH_GBAC_ZIO  	; GBA CARD 8M SIZE
	STA.L  MYTH_PRAM_ZIO  	; PSRAM    8M SIZE
	LDA    #$00
	STA.L  MYTH_PRAM_BIO

	; DEBUG
;	ldx		#0
;-:
;	txa
;	sta.l	$500000,x
;	lda		#$77
;	sta.l	pfmountbuf
;	sta.l	pfmountbuf+1
;	inx
;	cpx		#8
;	bne		-
	;jsr		show_copied_data
	;-:		bra	-
	
	stz 	getBit_buffer

	lda 	_inflate_src,s
	sta 	inputPointer
	lda 	_inflate_src+1,s
	sta 	inputPointer+1
	lda 	_inflate_src+2,s
	sta 	inputPointer+2

	lda 	_inflate_dest,s
	sta 	outputPointer
	sta 	tcc__r2 
	lda 	_inflate_dest+1,s
	sta 	outputPointer+1
	sta 	tcc__r2+1 
	lda 	_inflate_dest+2,s
	sta 	outputPointer+2
	sta 	tcc__r2+2 

	ldy 	#0

	; Jump to a FastROM mirror of this code in PSRAM
	jml INFL_CODE_OFS+inflate_blockLoop

inflate_blockLoop:
; Get a bit of EOF and two bits of block type
;	ldy	#0
	sty	getBits_base
	lda	#GET_3_BITS
	jsr	getBits
	lsr	a 
	php
	tax
	bne	inflateCompressedBlock

; Copy uncompressed block
;	ldy	#0
	sty	getBit_buffer
	jsr	getWord
	jsr	getWord
	sta	inflateStoredBlock_pageCounter
;	jmp	inflateStoredBlock_firstByte
	bcs	inflateStoredBlock_firstByte
inflateStoredBlock_copyByte
	jsr		getByte
inflateStoreByte
	jsr		storeByte
  	bcs 	+
  	jmp.w 	inflateCodes_loop
+:
inflateStoredBlock_firstByte
	inx
	bne		inflateStoredBlock_copyByte
	inc		inflateStoredBlock_pageCounter
	bne		inflateStoredBlock_copyByte

inflate_nextBlock
	plp
	bcc		inflate_blockLoop

  	rep 	#$20
  	lda	tcc__r0h
  	and	#1
  	clc	  
  	adc 	outputPointer ;lda outputPointer
  	sec
  	sbc 	_inflate_dest,s
  	sta 	tcc__r0
  	lda 	outputPointer+2
  	and 	#$FF
  	sbc 	_inflate_dest+2,s
  	sta 	tcc__r1

	; Jump back to WRAM
	jml $7D0000+ddddd
ddddd:
  
  	sep		#$20
    	LDA     #$00       ;
    	STA.L   MYTH_WE_IO     ; PSRAM WRITE OFF
    	LDA     #MAP_MENU_FLASH_TO_ROM	; SET GBA CARD RUN
    	STA.L   MYTH_OPTION_IO
    	LDA     #$20       		; OFF A21
    	STA.L   MYTH_GBAC_ZIO
    	JSR     SET_NEOCMD		; SET MENU
    	LDA     #$00
    	STA.L   MYTH_GBAC_LIO
    	STA.L   MYTH_GBAC_HIO
    	STA.L   MYTH_GBAC_ZIO
  
	rep	#$30
  	ply
  	plx
  	pla
  	;sep #$30
  	plp
 	;-: bra -
  	rtl 


  sep #$30
inflateCompressedBlock:

; Decompress a block with fixed Huffman trees:
; :144 dta 8
; :112 dta 9
; :24  dta 7
; :6   dta 8
; :2   dta 8 ; codes with no meaning
; :30  dta 5+DISTANCE_TREE
;	ldy	#0
inflateFixedBlock_setCodeLengths
	lda	#4
	cpy	#144
	rol	a ;@
	sta.w literalSymbolCodeLength,y
	cpy	#CONTROL_SYMBOLS
	bcs	inflateFixedBlock_noControlSymbol
	lda	#5+DISTANCE_TREE
	cpy	#LENGTH_SYMBOLS
	bcs	inflateFixedBlock_setControlCodeLength
	cpy	#24
	adc	#2-DISTANCE_TREE
inflateFixedBlock_setControlCodeLength:
	sta.w	controlSymbolCodeLength,y
inflateFixedBlock_noControlSymbol:
	iny
	bne	inflateFixedBlock_setCodeLengths
	lda #LENGTH_SYMBOLS ;mva	#LENGTH_SYMBOLS	inflateCodes_primaryCodes
	sta inflateCodes_primaryCodes
	
	dex
	beq	inflateCodes

; Decompress a block reading Huffman trees first

; Build the tree for temporary codes
	jsr	buildTempHuffmanTree

; Use temporary codes to get lengths of literal/length and distance codes
	ldx	#0
;	sec
inflateDynamicBlock_decodeLength:
	php
	stx	inflateDynamicBlock_lengthIndex
; Fetch a temporary code
	jsr	fetchPrimaryCode
; Temporary code 0..15: put this length
	tax
	bpl	inflateDynamicBlock_verbatimLength
; Temporary code 16: repeat last length 3 + getBits(2) times
; Temporary code 17: put zero length 3 + getBits(3) times
; Temporary code 18: put zero length 11 + getBits(7) times
	jsr	getBits
;	sec
	adc	#1
	cpx	#GET_7_BITS
  	bcc +
  	adc #7
+:
	tay
	lda	#0
	cpx	#GET_3_BITS
  	bcs	+
  	lda inflateDynamicBlock_lastLength
  +:
inflateDynamicBlock_verbatimLength:
	iny
	ldx		inflateDynamicBlock_lengthIndex
	plp
inflateDynamicBlock_storeLength:
	bcc		inflateDynamicBlock_controlSymbolCodeLength
	sta.w	literalSymbolCodeLength,x ;+
  inx
	cpx	#1
inflateDynamicBlock_storeNext:
	dey
	bne		inflateDynamicBlock_storeLength
	sta		inflateDynamicBlock_lastLength
;	jmp		inflateDynamicBlock_decodeLength
	beq		inflateDynamicBlock_decodeLength
inflateDynamicBlock_controlSymbolCodeLength:
	cpx	inflateCodes_primaryCodes
	;scc:ora	#DISTANCE_TREE
  bcc +
  ora #DISTANCE_TREE
  +:
	sta.w	controlSymbolCodeLength,x ;+
  inx
	cpx		inflateDynamicBlock_allCodes
	bcc		inflateDynamicBlock_storeNext
	dey
;	ldy	#0
;	jmp	inflateCodes

; Decompress a block
inflateCodes:
	jsr		buildHuffmanTree
inflateCodes_loop:
	jsr		fetchPrimaryCode
  	bcs 	+
  	jmp.w 	inflateStoreByte
+:
	tax
  	bne 	+	
  	jmp.w 	inflate_nextBlock
+:
; Copy sequence from look-behind buffer
;	ldy	#0
	sty	getBits_base
	cmp	#9
	bcc	inflateCodes_setSequenceLength
	tya
;	lda	#0
	cpx	#1+28
	bcs	inflateCodes_setSequenceLength
	dex
	txa
	lsr	a 
	ror	getBits_base
	inc	getBits_base
	lsr	a 
	rol	getBits_base
	jsr	getAMinus1BitsMax8
;	sec
	adc	#0
inflateCodes_setSequenceLength:
	sta		inflateCodes_lengthMinus2
	ldx		#DISTANCE_TREE
	jsr		fetchCode
;	sec
	sbc		inflateCodes_primaryCodes
	tax
	cmp		#4
	bcc		inflateCodes_setOffsetLowByte
	inc		getBits_base
	lsr		a 
	jsr		getAMinus1BitsMax8
inflateCodes_setOffsetLowByte:
	eor		#$ff
	sta		inflateCodes_sourcePointer
	lda		getBits_base
	cpx		#10
	bcc		inflateCodes_setOffsetHighByte
	lda.w 	getNPlus1Bits_mask-10,x
	jsr		getBits
	clc
inflateCodes_setOffsetHighByte:
	eor		#$ff
	sta		inflateCodes_sourcePointer+1
	
	rep 	#$20
 	lda 	inflateCodes_sourcePointer
 	sta		tcc__r2
 	cmp  	#$FFFF
 	bne		+
 	jsr.w	fillBytes
 	bra		after_copy_fill ;inflateCodes_loop
 +:
	lda 	inflateCodes_sourcePointer
 	clc
	adc 	outputPointer
	sta 	inflateCodes_sourcePointer
	cmp 	outputPointer
	sep 	#$20
	lda 	outputPointer+2
	bcc 	+
	dea
	+:
	sta 	inflateCodes_sourcePointer+2
 
	rep 	#$30
	ldy 	#0
	lda 	inflateCodes_lengthMinus2
	and 	#$ff
	bne		+
	lda 	#$100
	+:
	ina
	ina
	tax
	sep 	#$20
	
	jsr.w 	copyBytes
after_copy_fill:
	rep		#$30
	tya
	clc
	adc		outputPointer
	sta 	outputPointer
	ldy 	#0
	sep 	#$30
	bcc		+
	inc 	outputPointer+2
	+:
	sty 	inflateCodes_lengthMinus2
	

;jsr show_copied_data
;-: bra -
	jmp.w	inflateCodes_loop


buildTempHuffmanTree:
;	ldy	#0
	tya
inflateDynamicBlock_clearCodeLengths:
	sta.w	literalSymbolCodeLength,y
	sta.w	literalSymbolCodeLength+TOTAL_SYMBOLS-256,y
	iny
	bne	inflateDynamicBlock_clearCodeLengths
; numberOfPrimaryCodes = 257 + getBits(5)
; numberOfDistanceCodes = 1 + getBits(5)
; numberOfTemporaryCodes = 4 + getBits(4)
	ldx	#3
inflateDynamicBlock_getHeader:
	lda.w	inflateDynamicBlock_headerBits-1,x
	jsr	getBits
;	sec
	adc.w	inflateDynamicBlock_headerBase-1,x
	sta	inflateDynamicBlock_tempCodes-1,x
	sta.w	inflateDynamicBlock_headerBase+1
	dex
	bne	inflateDynamicBlock_getHeader


; Get lengths of temporary codes in the order stored in tempCodeLengthOrder
;	ldx	#0
inflateDynamicBlock_getTempCodeLengths:
	lda	#GET_3_BITS
	jsr	getBits
	ldy.w	tempCodeLengthOrder,x
	sta.w	literalSymbolCodeLength,y
	ldy	#0
	inx
	cpx	inflateDynamicBlock_tempCodes
	bcc	inflateDynamicBlock_getTempCodeLengths

; Build Huffman trees basing on code lengths (in bits)
; stored in the *SymbolCodeLength arrays
buildHuffmanTree:
; Clear nBitCode_totalCount, nBitCode_literalCount, nBitCode_controlCount
	tya
;	lda	#0
	;sta:rne	nBitCode_clearFrom,y+
  -:
  sta.w	nBitCode_clearFrom,y
  iny
  bne -
; Count number of codes of each length
;	ldy	#0
buildHuffmanTree_countCodeLengths:
	ldx.w	literalSymbolCodeLength,y
	inc.w	nBitCode_literalCount,x
	inc.w	nBitCode_totalCount,x
	cpy		#CONTROL_SYMBOLS
	bcs		buildHuffmanTree_noControlSymbol
	ldx.w	controlSymbolCodeLength,y
	inc.w	nBitCode_controlCount,x
	inc.w	nBitCode_totalCount,x
buildHuffmanTree_noControlSymbol:
	iny
	bne		buildHuffmanTree_countCodeLengths
; Calculate offsets of symbols sorted by code length
;	lda	#0
	ldx		#-3*TREE_SIZE
buildHuffmanTree_calculateOffsets:
	sta.w	nBitCode_literalOffset+3*TREE_SIZE-$100,x
  clc
	adc.w nBitCode_literalCount+3*TREE_SIZE-$100,x ;add	nBitCode_literalCount+3*TREE_SIZE-$100,x
	inx
	bne	buildHuffmanTree_calculateOffsets
; Put symbols in their place in the sorted array
;	ldy	#0
buildHuffmanTree_assignCode:
	tya
	ldx.w	literalSymbolCodeLength,y
	;ldy:inc	nBitCode_literalOffset,x
  ldy.w nBitCode_literalOffset,x
  inc.w nBitCode_literalOffset,x
	sta.w	codeToLiteralSymbol,y
	tay
	cpy		#CONTROL_SYMBOLS
	bcs		buildHuffmanTree_noControlSymbol2
	ldx.w	controlSymbolCodeLength,y
	;ldy:inc	nBitCode_controlOffset,x
  ldy.w nBitCode_controlOffset,x
  inc.w nBitCode_controlOffset,x
	sta.w	codeToControlSymbol,y
	tay
buildHuffmanTree_noControlSymbol2:
	iny
	bne		buildHuffmanTree_assignCode
	rts

; Read Huffman code using the primary tree
fetchPrimaryCode:
	ldx	#PRIMARY_TREE
; Read a code from input basing on the tree specified in X,
; return low byte of this code in A,
; return C flag reset for literal code, set for length code
fetchCode:
;	ldy	#0
	tya
fetchCode_nextBit:
	;jsr	getBit

	lsr	getBit_buffer
	bne	+
	pha
	lda 	[inputPointer] 
	rep	#$20
	inc 	inputPointer
	sep 	#$20
	sec
	ror	a 
	sta	getBit_buffer
	pla
+:	
	rol	a 
	inx
  sec
  	sbc.w	nBitCode_totalCount,x ;sub	nBitCode_totalCount,x
	bcs	fetchCode_nextBit
;	clc
	adc.w	nBitCode_controlCount,x
	bcs	fetchCode_control
;	clc
	adc.w	nBitCode_literalOffset,x
	tax
	lda.w	codeToLiteralSymbol,x
	clc
	rts
fetchCode_control:
    clc
	adc.w	nBitCode_controlOffset-1,x 
	tax
	lda.w	codeToControlSymbol,x
	sec
	rts

; Read A minus 1 bits, but no more than 8
getAMinus1BitsMax8:
	rol	getBits_base
	tax
	cmp	#9
	bcs	getByte
	lda.w	getNPlus1Bits_mask-2,x
getBits:
	jsr	getBits_loop
getBits_normalizeLoop:
	lsr	getBits_base
	ror	a 
	bcc	getBits_normalizeLoop
	rts

; Read 16 bits
getWord:
	jsr	getByte
	tax
; Read 8 bits
getByte:
	lda	#$80
getBits_loop:
	;jsr	getBit
	lsr	getBit_buffer
	bne	+
	pha
	lda 	[inputPointer] 
	rep		#$20
	inc 	inputPointer
	sep 	#$20
	sec
	ror	a 
	sta	getBit_buffer
	pla
+:	
	ror	a 
	bcc	getBits_loop
	rts

; Read one bit, return in the C flag
getBit:
	lsr	getBit_buffer
	bne	getBit_return
	pha
	;	ldy	#0
	lda 	[inputPointer] 
	rep	#$20
	inc 	inputPointer
	sep 	#$20
	sec
	ror	a 
	sta	getBit_buffer
	pla
getBit_return:
	rts


; Write a byte
storeByte:
	  sta	[outputPointer]
	  inc 	outputPointer
	  bne	++
	  rep	#$20
	  inc	outputPointer+1
	  sep	#$20
++:	
	  rts 	



fillBytes:
	rep	#$20
	clc
	lda	inflateCodes_sourcePointer
	adc 	outputPointer
	sta 	inflateCodes_sourcePointer
	cmp 	outputPointer
	sep 	#$20
	lda 	outputPointer+2
	bcc 	+
	dea
+:
	sta 	inflateCodes_sourcePointer+2
 
	lda	outputPointer
	and	#1
	beq	+
	sta	[outputPointer]
	lda	[inflateCodes_sourcePointer]
	sta	[inflateCodes_sourcePointer]
	bra	++
+:
	lda	[inflateCodes_sourcePointer]
++:
 	pha
 	
	rep 	#$30
	ldy 	#0
	lda 	inflateCodes_lengthMinus2
	and 	#$ff
	bne		+
	lda 	#$100
+:
	ina
	ina
	tax
	sep 	#$20
	
	pla
fillBytesLoop:
	sta	[outputPointer],y
	iny
	dex
	bne	fillBytesLoop
	rts
	

	
copyBytes:
	rep 	#$30

	lda	tcc__r2
	cmp	#$FFFE
	beq	copyBytesSafe
	;sep 	#$20
	rep	#$20
	txa
	lsr	a
	tax
copyBytesLoop:		
	lda 	[inflateCodes_sourcePointer],y ;$0000,y
	sta	[outputPointer],y
	iny
	iny
	dex
	bne	copyBytesLoop
	sep	#$20
	bcc	+
copyBytesTail:		
	lda	[inflateCodes_sourcePointer],y ;$0000,y
	sta	[outputPointer],y
	iny
+:	
	rts 	

	nop
	
copyBytesSafe:
	rep 	#$30
	;sep 	#$20
	;rep	#$20
copyBytesSafeLoop:		
	lda 	[inflateCodes_sourcePointer],y ;$0000,y
	sta	[outputPointer],y
	iny
	dex
	bne	copyBytesSafeLoop
	sep	#$20
	;plb
	rts 	
	


;**********************************

; For core older than 2.6. Use 16-bit writes to PSRAM

storeByte16bit:
	  dec 	tcc__r0h
	  bne 	+
	  ; odd address
	  xba
	  lda 	tcc__r1h
	  rep	#$20
	  sta	[outputPointer]
	  sep	#$20
	  
	  lda 	#2
	  sta 	tcc__r0h

	  inc 	outputPointer
	  inc 	outputPointer
	  bne	++
	  rep	#$20
	  inc	outputPointer+1
	  sep	#$20
++:	
	  rts 	
+:
	  ; even address
	  sta	tcc__r1h
	  rts 	


fillBytes16bit:
	rep		#$20
	clc
	lda 	tcc__r0h
 	and 	#1
 	adc 	inflateCodes_sourcePointer	
	adc 	outputPointer
	sta 	inflateCodes_sourcePointer
	cmp 	outputPointer
	sep 	#$20
	lda 	outputPointer+2
	bcc 	+
	dea
+:
	sta 	inflateCodes_sourcePointer+2
 
 	lda	tcc__r0h
 	and	#1
 	bne	+
 	lda	[inflateCodes_sourcePointer]
 	bra	++
+:
 	lda	tcc__r1h
 ++:
 	pha
 	
	rep 	#$30
	ldy 	#0
	lda 	inflateCodes_lengthMinus2
	and 	#$ff
	bne	+
	lda 	#$100
+:
	ina
	ina
	tax
	sep 	#$20
	
	pla
	sta	tcc__r1h
	xba
	lda	tcc__r1h
	
	dec 	tcc__r0h
	bne 	fillBytesEven
fillBytesOdd:
	; odd address
	rep		#$20
	sta		[outputPointer],y
	sep		#$20
;	inc 	outputPointer
;	inc 	outputPointer
;	bne		++
;	rep		#$20
;	inc		outputPointer+1
;	sep		#$20
;++:
	iny
	iny
	dex
	bne		fillBytesEven
	lda		#2
	bra		+
fillBytesEven:
	; even address
	dex
	bne		fillBytesOdd
	lda		#1
+:
	sta		tcc__r0h
	;ldy 	#0
	;sep 	#$30
	;sty 	inflateCodes_lengthMinus2
	rts
	
	
  

copyBytes16bit:
	lda	tcc__r0h
	cmp	#2
	bne copyBytesStartOdd

copyBytesStartEven:
	rep #$30
copyBytesEven1:
	dex
	bne	copyBytesOdd1
  	sep 	#$20
readsrc1:  
  	lda 	[inflateCodes_sourcePointer],y  ;$0000,y
	sta 	tcc__r1h
	lda	#1
	sta	tcc__r0h
	;plb
	rts
  	rep #$20
copyBytesOdd1:
readsrc2:
	lda	[inflateCodes_sourcePointer],y  ;lda 	$0000,y  
	sta	[outputPointer],y
	iny
	iny
	dex
	bne	copyBytesEven1
  	sep 	#$20
	lda	#2
	sta	tcc__r0h
;	plb
	rts 	

copyBytesStartOdd:
	rep 	#$30
lda inflateCodes_sourcePointer+2
sta tcc__r3h
lda inflateCodes_sourcePointer
ina
sta tcc__r3
bne +
inc tcc__r3h
+:
;	lda 	inflateCodes_sourcePointer
;	sta.w 	readsrc4+1
;	ina
;	sta.w 	readsrc3+1
	sep 	#$20
;	lda 	inflateCodes_sourcePointer+2
;	phb
;	pha
;	plb
	lda 	tcc__r1h
copyBytesOdd2:
	xba
readsrc3:
	lda 	[tcc__r3],y  ;$0001,y
	xba
	rep	#$20
	sta	[outputPointer],y
	sep	#$20
	iny
	iny
	dex
	bne	copyBytesEven2
	lda	#2
	sta	tcc__r0h
	;plb
	rts 	
copyBytesEven2:
readsrc4:
	lda 	[inflateCodes_sourcePointer],y ;$0000,y
	dex
	bne	copyBytesOdd2
	sta 	tcc__r1h
	lda	#1
	sta	tcc__r0h
	;plb
	rts 	
	
;*************************************
	

getNPlus1Bits_mask:
	.db	GET_1_BIT,GET_2_BITS,GET_3_BITS,GET_4_BITS,GET_5_BITS,GET_6_BITS,GET_7_BITS

tempCodeLengthOrder:
	.db	GET_2_BITS,GET_3_BITS,GET_7_BITS,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15

inflateDynamicBlock_headerBits:	.db	GET_4_BITS,GET_5_BITS,GET_5_BITS
inflateDynamicBlock_headerBase:	.db	3,0,0  ; second byte is modified at runtime!


; Data for building trees

literalSymbolCodeLength:
	.dsb 256,0
controlSymbolCodeLength:
	.dsb CONTROL_SYMBOLS,0

; Huffman trees

nBitCode_clearFrom:
nBitCode_totalCount:
	.dsb TREE_SIZE*2,0
nBitCode_literalCount:
	.dsb TREE_SIZE,0
nBitCode_controlCount:
	.dsb TREE_SIZE*2,0
nBitCode_literalOffset:
	.dsb TREE_SIZE,0
nBitCode_controlOffset:
	.dsb TREE_SIZE*2,0

codeToLiteralSymbol:
	.dsb 256,0
codeToControlSymbol:
	.dsb CONTROL_SYMBOLS,0

inflate_ram_code_end:


.ends

