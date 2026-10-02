;=============================================================
; Northstar MDS-D Boot
; Disassambled by Martin Eberhard from ROMs found on an MDS-D
; 28 November 2016
;=============================================================
; Assembled code matches MDS-D ROM code exactly when assembled
; with Digital Research's ASM.
;
; This program uses no RAM (and that means no stack).
; Subroutines are passed the return address in register pair
; hl instead.
;
; Double-density disks boot from track 0, sector 4. This is a
; normal 512-byte double-density sector.
;
; Single-density disks boot from track 1, sector 8. This is a
; special 512-byte single-density sector. (Normal single-
; density sectors are 256 bytes long.)
;
; In either case, the first byte in the sector is the RAM page
; address (high address byte) for the boot sector. The
; subsequent 511 bytes are loaded into RAM starting at byte 1
; of the specified page (xx01). The next byte after these 512
; bytes is the CRC of the first 512 bytes.
;=============================================================

;---------
;Constants
;---------
RETRYS	equ	10		;max retries
DDBSCR	equ	4		;Double-density boot sector
SDBSCR	equ	8		;Single-density boot sector

;-----------------
;Memory Allocation
;-----------------
CTLBAS	equ	0E800h		;Disk controller base address
ROMADR	equ	CTLBAS		;Standard ROM address
CTLWD	equ	CTLBAS+100h	;Write data
CTLORD	equ	CTLBAS+200h	;Controller Orders
CTLCMD	equ	CTLBAS+300h	;Controller Commands

;-----------------------------
;MDS-D Disk Controller Equates
;-----------------------------

;Order Register Bits

ORDDS0	equ	00		;No drive selected
ORDDS1	equ	01h		;Select drive 1
ORDDS2	equ	02h		;Select dribe 2
ORDDS3	equ	04h		;Select drive 3
ORDDS4	equ	08h		;Select drive 4
ORDST	equ	10h		;Step level
ORDSIN	equ	20h		;Step in
ORDSS	equ	40h		;Disk side
ORDDD	equ	80h		;Double-density

;Control register bits

CTLRSF	equ	01h		;reset sector flag
CTLDI	equ	02h		;Disarm interrupt
CTLEI	equ	03h		;Arm interrupt
CTLSB	equ	04h		;Set Body (diag)
CTLMO	equ	05h		;Motors on
CTLWR	equ	06h		;Begin write
CTLRES	equ	07h		;REset controller

CTLAS	equ	10h		;Select A-Status
CTLBS	equ	20h		;Select B-Status
CTLCS	equ	30h		;Select C-Status
CTLRD	equ	40h		;Select read data

;A-Status Bits

SABD	equ	01h		;sync chr detected (body)
SARE	equ	04h		;Read enabled
SAWI	equ	08h		;Sector window
SAMO	equ	10h		;Motor on
SADD	equ	20h		;Double density detected
SAIX	equ	40h		;Index hole on previous sector
SASF	equ	80h		;Sector flag (cleared by software)

;B-Status Bits

SBT0	equ	01h		;Track 0 detected
SBWP	equ	02h		;Write protected
SBWR	equ	08h		;Write operation in progress
SBMO	equ	10h		;Motor on
SBDD	equ	20h		;Double density detected
SBIX	equ	40h		;Index hole on previous sector
SBSF	equ	80h		;Sector flag (cleared by software)

;C-Status Bits

SCSM	equ	0Fh		;Sector mask
SCMO	equ	10h		;Motor on
SCDD	equ	20h		;Double density detected
SCIX	equ	40h		;Index hole on previous sector
SCSF	equ	80h		;Sector flag (cleared by software)

;==================
;Start of Execution
;==================
	org	CTLBAS

INIT:	mvi	c,RETRYS		;Initialize retry counter

;----------------------------------------------------------
;Start the spindle motors, select drive 1, and delay enough
;time for the motor to spin up and the head to settle
;----------------------------------------------------------
	lda	CTLCMD+CTLAS+CTLMO ;Motor on

	mvi	d,30h		;48 secs: motor spin-up delay
	lxi	h,INITR1	;'return' address
	jmp	WSECTD
INITR1:

	lda	CTLORD+ORDDS1	;Select drive 1

	lxi	h,INITR2	;'return' address
	jmp	WSECT2		;2 secs: head settling delay
INITR2:

;--------------------------------------------------
;Wait for the index mark to ensure hard-sector disk
;--------------------------------------------------
	mvi	b,0ch		;wait no more than 12 sectors

IWLOOP:	lxi	h,IWLPR1	;'return' address
	jmp	WSECT1		;wait for next setor
IWLPR1:

	lda	CTLCMD+ORDST	;Get A-Status
	ani	SAIX		;Index hole?
	jnz	HOME		;y: carry on

	dcr	b		;keep hunting for index
	jnz	IWLOOP		;unless its not there

;No index hole found after 12 sectors. Hang here forever

HANG1:	jmp	HANG1

;----------------------------------------------------------------
;Home: Seek track 0
;(There is no check for track 0 failure. The code will just loop
;here, stepping outward forever if the track 0 sensor has failed.
;----------------------------------------------------------------

;Step in once to step away from the track 0 sensor

HOME:	lda	CTLORD+ORDSIN+ORDDS1		;Step level low, step in
	lda	CTLORD+ORDSIN+ORDST+ORDDS1	;step level high, step in
	lda	CTLORD+ORDSIN+ORDDS1
	jmp	HWAIT

;Step out until on track 0

HOMER1:	lda	CTLCMD+CTLBS		;Read B-Status
	ani	SBT0			;On track 0?
	jnz	HUNT			;y: go find the right sector

;Step out once, and delay (for 2 sector times) while it steps

	lda	CTLORD+ORDDS1		;Step level low, step out
	lda	CTLORD+ORDST+ORDDS1	;Step level high, step out
	lda	CTLORD+ORDDS1		;Step level low, step out

HWAIT:	lxi	h,HOMER1		;'return' address

;Fall into WSECT2

;***Subroutine***********************
;Wait 2 sector times
;On Entry:
;  hl = return address
;On Exit:
;  a = 0
;  d = 0
;  Sector flag is cleared
;************************************
WSECT2:	mvi	d,2
	jmp	WSECTD

;---------------------------------------------
;Hunt: Wait for the double-density boot sector
;---------------------------------------------
HUNT:	lxi	h,HUNTR1		;'return' address
	jmp	WSECT1			;Wait for next sector

HUNTR1:	lda	CTLCMD+CTLCS+CTLMO	;Read C-Status
	ani	SCSM			;Sector mask
	cpi	DDBSCR			;Found our boot sector?
	jnz	HUNT			;N: Keep looking

;Wait for hardware to say that read is enabled

WAITRD:	lda	CTLCMD+CTLAS		;Read A-Status
	ani	SARE			;Read enabled?
	jz	WAITRD			;n: wait more

;Stall for about 68 uS on a 2 MHz 8080
;or about 34 uS on a 4 MHz Z80

	mvi	a,9
STAL68:	dcr	a			;(5)
	jnz	STAL68			;(10)

;-----------------------------------------------------------
;Check for single-density disk, which gets special treatment
;-----------------------------------------------------------
	lda	CTLCMD+CTLAS		;Read A-Status
	ani	SADD			;Double-density disk?
	jnz	GETSEC			;y: go read the sector

;---------------------------------------------
;Single-density boot
;Step in to track 1 and hunt for sector 8
;(Sector 8 is a special 512-byte boot sector.)
;---------------------------------------------
	lda	CTLORD+ORDSIN+ORDDS1		;Step in, step level low
	lda	CTLORD+ORDSIN+ORDST+ORDDS1	;step level high, step in
	lda	CTLORD+ORDSIN+ORDDS1		;Step in, step level low

	lxi	h,SDBR1			;'return' address
	jmp	WSECT2			;Wait for 2 sector times
SDBR1:

SDRSCT:	lxi	h,SDBR2			;'return' address
	jmp	WSECT1			;Wait for 1 sector time
SDBR2:

	lda	CTLCMD+CTLCS+CTLMO	;Read C-Status
	ani	SCSM			;Sector mask
	cpi	SDBSCR			;single-density boot sector
	jnz	SDRSCT

;Fall into GETSEC

;----------------------------------------
;Wait for the sector body.
;Time out after a ridiculously long time.
;----------------------------------------
GETSEC:	mvi	b,8ch			;timeout max (1.2 mS at 4MHz)
	lxi	d,CTLCMD+CTLRD		;Select Read Data

WATSEC:	lda	CTLCMD+CTLAS		;(7)Read A-Status
	rrc				;(4)SABD set? (Sync chr detected)
	jc	RDDATA			;(10)y: go read data

	dcr	b			;(5)
	jnz	WATSEC			;(10)

;Error during load. Retry if we can.

ERROR:	dcr	c			;More retries left?
	jnz	HOME			;y: try again

;------------------------------------------
;Can't find sector body. Hang here forever.
;------------------------------------------
HANG2:	jmp	HANG2			;Hang forever

;--------------------------------------------------------------
;Read and validate sector data
;  Byte 0: <PA> High byte of RAM address
;  Bytes 1-511: Sector data
;  Byte 512: CRC of Bytes 0-511
;Bytes1-511 get written into RAM at <PA>01 through <PA+1>FF.
; <PA> gets written to <PA>01 before getting overwritten by
; byte 1. <PA>+00 never gets written
;--------------------------------------------------------------
RDDATA:	ldax	d			;read 1st disk data byte
	mov	h,a			;This is the target RAM page		
	mvi	l,1			;byte 1 in that page
	mov	m,a			;save page address (why?)

	rlc				;Start CRC calculation
	mov	b,a			;b accumulates CRC

;Read the next 255 bytes (the remainder of the data that goes
;into the 1st RAM page) into the specified RAM page

RLOOP1:	ldax	d			;get a disk data byte
	mov	m,a			;write it to RAM
	xra	b			;compute CRC
	rlc
	mov	b,a			;..in b
	inr	l			;end of page data?
	jnz	RLOOP1			;n: goet another byte

;Read the final 256 bytes into the next RAM page

	inr	h			;Next RAM page

RLOOP2:	ldax	d			;get a disk data byte
	mov	m,a			;write it to RAM
	xra	b			;compute CRC
	rlc
	mov	b,a			;..in b
	inr	l			;end of sector data?
	jnz	RLOOP2			;n: keep reading

	ldax	d			;Get CRC byte
	xra	b			;Does it match?
	jnz	ERROR			;n: retry if possible

;-----------------------------------------
;Success. Go execute the loaded code image
;-----------------------------------------
	dcr	h			;get execution address
	mvi	l,0ah			;10th byte of load page
	pchl				;Go execute loaded code

;***Subroutine***********************
;Wait 1 sector time
;On Entry:
;  hl = return address
;On Exit:
;  a = 0
;  d = 0
;  Sector flag is cleared
;************************************
WSECT1:	mvi	d,1

;Fall into WSECTD

;***Subroutine***********************
;Wait d sector times
;On Entry:
;  d = number of sector tines to wait
;  hl = return address
;On Exit:
;  a = 0
;  d = 0
;  Sector flag is cleared
;************************************
WSECTD:	lda	CTLCMD+CTLAS+CTLRSF;REset sector flag

WSECTR:	lda	CTLCMD+CTLAS		;Read A-Status
	ora	a			;Test SASF (Sector flag)
	jp	WSECTR			;Wait for sector

	dcr	d			;next sector
	lda	CTLCMD+CTLAS+CTLRSF	;Reset sector flag
	jnz	WSECTD			;more sectors to wait?

	pchl				;'return' from subroutine

;----------------
;Unused ROM space
;----------------
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

;------------
;Spare return
;------------
	pop	h			;spurious 'return'
	pchl

	end


