;================================================================
; North-Star MDS-A disk boot ROM disassembly (later version PROM)
; Disassembled by Martin Eberhard (borrowing from an unattributed
; disassembly called NSBOOT.LST 'found on the internet'.)
; * Disassembly corrected 30 Jan 2018 *
;
; This later version of the boot ROM differs from the early
; version in two areas:
;
;    1) The older version loops forever attempting to boot when
; 	a disk error occurs. This version tries 10 times and
;	then dies in a loop.
;
;    2) The older version uses a sector timeout constant of
; 	46h (70) versus 8Ch (140) used in this version. The
;       timeout was increased to support a 4mhz processor. 
;
; The Northstar MDS-A disk controller works by reading and
; writing to memory addresses - no INs or OUTs occur. Commands
; are encoded into the low byte of the address. This onboard
; ROM is hard-coded for execution at E900. There is ROM space on
; the board for another copy of this code, which may be ORG'd at
; a different address.
;
; This code is written to work especially with NSDOS. As such,
; it references code in the RAM page at 2000h. When booting,
; this RAM page gets overwritten with the bootstrap code.
;
; Assembles with Digital Reasearch's ASM. The resulting assembly
; matches the ROMS found on an MDS-A controller.
;================================================================

;RAM allocation (Set up for NSDOS)

RAMBAS	equ	2000h		;RAM base address
DRVTAB	equ	RAMBAS		;Current track for each drive (1-3)
LODADR	equ	RAMBAS		;Load address
LASTDR	equ	RAMBAS+3	;Last used drive
EXEADR	equ	RAMBAS+4	;Execution address
DOSVER	equ	RAMBAS+7	;DOS verify operation
DOSWRT	equ	RAMBAS+0Ah	;DOS write operation

STACK	equ	RAMBAS+114h	;initial stack

;Disk controller memory space allocation

CTLBAS	equ	0E800h		;Disk controller base address
ROMADR	equ	CTLBAS+100h	;Standard ROM address
CTLADR	equ	CTLBAS+300h	;Disk Control base address

;Controller commands (are at memory addresses)

CTLDS0	equ	CTLADR+00h	;Select drive 0
CTLDS1	equ	CTLADR+01h	;Select drive 1
CTLDS2	equ	CTLADR+02h	;Select drive 2
CTLDS3	equ	CTLADR+03h	;Select drive 3
CTLWRT	equ	CTLADR+04h	;Initiate write
CTLSTC	equ	CTLADR+08h	;Clear track-step f-f
CTLSTS	equ	CTLADR+09h	;Set track-step f-f
CTLDI	equ	CTLADR+0Ch	;Disarm interrupts
CTLEI	equ	CTLADR+0Dh	;Arm interrupts
CTLNOP	equ	CTLADR+10h	;No operation (read A-Status)
CTLRSF	equ	CTLADR+14h	;Reset sector flag
CTLRES	equ	CTLADR+18h	;Reset controller
CTLSTO	equ	CTLADR+1Ch	;select step-out
CTLSTI	equ	CTLADR+1Dh	;Select step-in
CTLBST	equ	CTLADR+20h	;Read B-status
CTLRD	equ	CTLADR+40h	;Read Data
CTLMO	equ	CTLADR+80h	;Drive motors-on command

;A-Status Bits

SATR0	equ	01h		;Track 0
SAWP	equ	02h		;Write protect
SABDY	equ	04h		;Sync chr found
SAWRT	equ	08h		;Ready for write data
SAMO	equ	10h		;Motor is on
SAWN	equ	40h		;Status was read during window
SASF	equ	80h		;Sector flag

;B-Status Bits

SBSECT	equ	0Fh		;Sector position mask
SBMO	equ	10h		;Motor is on
SBWN	equ	40h		;Status was read during window
SBSF	equ	80h		;Sector flag

UNINIT	equ	59h		;Uinitialized RAM value
RETRYS	equ	10		;Max retries
BDRIVE	equ	1		;Boot drive (Code assumes this is 01)
BTRACK	equ	0		;Boot track
BSECTR	equ	4		;Boot sector

;**************
;Initialization
;**************
	org	ROMADR

	lxi	sp,STACK		;Init stack
	mvi	b,RETRYS		;Init retry count (10)

;(Re)load the boot sector. This code assumes that BDRIVE=01.
;b = remaining retries. Stack is valid.

RETRY:	push	b			;Retry count
	mvi	a,UNINIT		;Invalidate drive 1 track
	sta	DRVTAB
	sta	LASTDR			;Invalidate last drive
	lxi	b,BTRACK*256+BDRIVE	;Boot track and drive
	mov	a,c			;a=1 (sector count)
	mvi	d,BSECTR		;Boot sector
	mov	e,c			;e = 01 (Read operation)
	lxi	h,LODADR		;Load address
	call	DISKOP			;Do disk operation now


	jmp	LAUNCH			;Launch if successful

;***Subroutine*******************************************
; Disk Operation
; On Entry:
;   a = Block count
;   b = track (0-39)
;   c = drive (1-3)
;   d = sector
;   e = 01 Read/00 Write/03 Verify*
;   hl = load address
; [* only Read is native, Write/Verify jump back to NSDOS]
; On Exit:
;   Z set if no errors
;*********************************************************
DISKOP:	push	psw			;Block count
	push	h			;RAM address
	push	d			;Sector & Function
	push	b			;Track & drive 

	mvi	b,CTLADR/256		;Controller access
	call	MOTON			;Motors on, select drive in c

;Look up this drive's current track. Restore if uninitialized.

	lxi	h,RAMBAS - CTLADR - 1	;Drive status table
	dad	b			;Drive is in c
	mov	a,m			;Current track
	xri	UNINIT			;Uninitialized?
	push	h			;current track
	cz	SEEK			;Y: step to track 0 (a=0)

	pop	h			;Curent track for this drive
	pop	psw			;desired track number (from bc)
	call	SEEK			;Step to track

	pop	b			;Sector & function (from de)

;Hunt for desired sector

HUNT:	call	W1SECT			;Wait one sector time

	lda	CTLBST OR CTLNOP	;Read B-status
	ani	SBSECT			;Get sector number
	cmp	b			;Desired sector?
	jnz	hunt			;N: wait for it

	pop	h			;RAM address

;If the disk operation is anything but read sector then we
;assume that NSDOS is intact - and so we let NSDOS handle it.

	dcr	c			;Test disk function
	jm	DOSWRT			;disk op 00=Write
	jnz	DOSVER			;disk op <>01=Verify

;Disk operation 01: Read one block of data from drive

GETSEC:	mvi	b,8ch			;Timeout count
	lxi	d,CTLRD OR CTLNOP	;Read data
	mvi	c,0			;Read 256 bytes

;Wait for the sector body

WATSEC:	lda	CTLNOP			;Get A-status
	ani	SABDY			;Wait for body
	jnz	RDDATA			;Body: ready for data

	dcr	b			;Timeout?
	jnz	WATSEC			;n: keep waiting

;Timeout waiting for sector body

	mvi	a,01h			;Report timeout error
	jmp	EREXIT			;And exit

;***Subroutine**************************
; Step to track
; On Entry:
;   a = desired track
;  (hl) = current track for this drive
; On Exit:
;   a = 1 if on track 0
;  (hl) = new current track
;***************************************
SEEK:	mov	d,a			;Track
	sub	m			;Compute difference
	mov	m,d			;Save new track
	rz				;Done if already there

	lxi	h,CTLSTI		;Assume Step-in
	mov	c,a			;Number of tracks to step
	jp	STEP			;positive diff = step in

	cma				;absolute value: step out
	inr	a
	mov	c,a			;Number of tracks to step
	lda	CTLNOP			;Read A-status
	ani	SATR0			;At track 0?
	rnz				;Can't step out further

	lxi	h,CTLSTO		;Set Step-out

;Step C tracks. The step direction command is in hl.

STEP:	mov	a,m			;Set step direction

STPLUP:	lda	CTLSTS			;Set track-step flipflop
	xthl				;Delay
	xthl				;Delay
	lda	CTLSTC			;Clear track-step flipflop

	mvi	d,2			;Wait 2 sector times
	call	WSECTS

	lda	CTLNOP			;Read A-status
	ani	SATR0			;At track 0?
	jz	STEP1			;N: keep stepping

	mvi	c,1			;Force end

STEP1:	dcr	c			;More steps to do?
	jnz	STPLUP			;N: step again

	ret

;***Subroutine Continuation*****************
; Read complete - launch BOOT if no errors
; On Entry:
;   Z set if no errors
;   top-of-stack = retry count
;*******************************************
LAUNCH:	pop	b			;Restore retry count
	jz	EXEADR			;OK, launch OS

	dcr	b			;Reduce retry count
	jnz	RETRY			;Try again if we can

HANG:	jmp	hang			;Give up (Hang)

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

;***Error Exit***************************
; Report failure
; On Entry:
;   a = error code
;      01 = Timeout error
;      02 = CRC error
; On Exit:
;   Z set if no errors
;****************************************
EREXIT:	pop	b			;repair stack
	ora	a			;any errors?
	ret

;***Subroutine Continuation*****************
; Read data sector data from disk
; On Entry:
;    c = 0 (byte count=256)
;   de = CTLRD OR CTLNOP (Read Data command)
;   hl = memory address
;   top-of-stack = sector count
;*******************************************
RDDATA:	mov	b,c			;Zero CRC

RDLOOP:	ldax	d			;Read disk data byte
	mov	m,a			;Write to RAM

	xra	b			;Compute CRC
	rlc
	mov	b,a

	inx	h			;Next RAM address
	dcr	c			;Reduce count
	jnz	RDLOOP			;Read them all

	ldax	d			;Read CRC
	xra	b			;Does it match?
	jz	SECDON			;Y: sector data ok

	mov	a,b			;??
	mvi	a,02			;Report CRC error
	jmp	EREXIT			;..and exit

SECDON:	pop	psw			;Sector count
	dcr	a			;Reduce count
	rz				;All done?

	push	psw			;Re-save sector count
	call	W1SECT			;Wait 1 sector time
	jmp	GETSEC			;Go find next sector

;***Subroutine**************************
; Wait 1 sector time
; trashes d
;***************************************
W1SECT:	mvi	d,1			;Single sector time

;Fall into WSECTS

;***Subroutine**************************
; Wait d sector times
; On Entry:
;   d = number of sector times to wait
;***************************************
WSECTS:	lda 	CTLRSF			;Reset sector flag
WSECT:
	lda	CTLMO OR CTLNOP		;Read A-status
	ani	SASF			;Sector pulse?
	jz	WSECT			;Wait for it
	dcr	d			;enough sectors?
	rz				;y: done

	jmp	WSECTS			;n: Keep waiting

;***Subroutine**************************
; Turn drive motors On 
; On Entry:
;   bc = drive select command
;   c = Drive number
;   (LASTDR) = last drive selected
;***************************************
MOTON:	lda	CTLMO OR CTLNOP		;Motors on & A-status
	ani	SAMO			;Motor already on?
	jnz	MOTON2			;Yes, short wait

	mvi	d,50			;Wait 50 sector times (1 sec)
	call	WSECTS			;..for motor to spin up
	jmp	MOTON3			;Now select drive

MOTON2:	lda	LASTDR			;Get last drive
	cmp	c			;Same drive?
	rz				;Yes, no need to wait

MOTON3:	ldax	b			;Select new drive
	mov	a,c			;Get drive number
	sta	LASTDR			;Set new-drive flag
	mvi	d,13			;13 sector times for index sync
	jmp	WSECTS			;Wait & return

	nop

	end
