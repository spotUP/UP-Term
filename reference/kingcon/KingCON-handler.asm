
	mc68020

	xref	KPrintF

test=1		set for test, non-public versions
rom=1		set if built for 3.x KS ROM
tbc=1		set if VisualPrefs' tbiclass is to be used for iconify gadget
scn=1		set for screennotify.library support
newmouse=1	set for newmouse input events (wheels only)
;OS4=1		set for OS4-aware build

async=1		set for async ACTION_END

;MorphOS=1	set if MorphOS-aware version

****************************************************************************
* 0.16 (1.4) - Sun Mar 07 13:19:57 1999
*
* - added ROMTAG structure
* - boot shell doesn't open diskfont and wb.libraries and does not try to
*   reference ENV:
* - added scanning of C: to command completer
* - forced ScreenToFront() when opening the console in active mode
* - increased minwidth to 104 (to accomodate for iconify gadget)
* - reduced number of sections
* - bumped revision to 1.4
*
* 0.17 - Wed Jun 07 15:35:50 2000
* 
* - fixed some small bugs
* - removed automatic ScreenToFront() - forgot about POPSCREEN option ;)
* - more cleanups
* - wb and icon libraries are not opened if no WB screen has been detected
*
* 0.18 (1.5) - 
*
* - added 'Keep closed' to 'Console' menu
* - optimized quite a few routines
* - bumped revision to 1.5
* - increased maximal path length of dropped icons from 80 to 255 bytes
* - added support for C: multiassigns to command completer
*
* 0.19 (1.5.1) -
* - asl is opened only when needed
*
* 0.20 (1.5.1) - Wed Dec 13 01:45:53 2000
* - searches of command completer are a little bit more inteligent now: it
*   tries to avoid rescanning same directories
*
* 0.21 (1.5.1) - Mon Jan 22 14:17:55 2001
* - fixed serious bug in lib close routine
*
* 0.22 (1.6)
* - fixed small mis-alignment
* - workaround serious bug in AREXX (ACTION_CHANGE_SIGNAL is not reset on
*   Arexx termination)
* - rewrote global data area handling, startup semaphore has been removed
*   (should cure some race conditions and hang-ups)
* - code tweaking; converted parameters passing from stack to registers for
*   many routines
* - fixed race condition with char '[' rendering in raw mode
* - fixed bugs in SGR ANSI support
* - fixed Enforcer hit when opening filereq with 'Show .info' selected
* - added SGR keyword to options
* - bootshell is detected and it covers (nearly) full screen now
* - added 'Clear window' to menu (uses A+Z shortcut)
* - rewritten boopsi gadgets creation code; should be compatible with
*   VisualPrefs now
* - 'Review/Clear buffer' and 'Console/Keep closed' got shortcuts: 'B' and 'K',
*   respectively
* - added support for VisualPrefs' tbiclass
* - added screennotify.library support for AddWorkbenchClient()
*
* 0.23 (1.7)
* - fixed %A bug introduced in 1.6
* - added locale support
* - once again rewrote IM_DRAW routine for iconify gadget
* - removed workaround for xData program
* - added support for NewMouse wheels
* - fixed CTRL-D bug introduced in 1.4
* - reworked break signals post logic (should fix Reaction based ClassAction
*   consoles)
* - fixed crash on window resize with NOREVIEW option (really old bug!)
* - added shell resident comands to completer
* - Complete/Enable cache didn't work as supposed (it reset NOCLOSE flag
*   instead) - bug introduced in 1.5.1
* - educated screennotify support
* - fixed another old bug: referencing random ConUnit pointer on
*   Next screen/Goto screen
* - fixed 'read from zero' Enforcer hits with some ARexx scripts (referencing
*   fl_Volume of null pr_CurentDir)
* - added support for OS4 iconify gadget
* - fixed completer expansion problem with '"' char
* - yet another Enforcer hit fixed
* - iffparse.lib is opened only if env: is present
* - ACTION_END executes asynchronously now
***************************************************************************

	SECTION	KingCONhandlers000000,CODE

	include	systab.st
	include	lvos

fdate	macro
	dc.b	'8.10.2002'
	endm

$VER	macro
	dc.b	'1.7'
	endm


	ifd	MorphOS
WA_Dummy	equ	TAG_USER+100
WA_ExtraTitlebarGadgets	equ	WA_Dummy+152
WA_ExtraGadgetsStartID	equ	WA_Dummy+151
ETG_ICONIFY	equ	1
	endc

	ifd	tbc
TBI_ICONIFYIMAGE	equ	104
	endc

	ifd	OS4
ICONIFYIMAGE	equ	$16
GA_Titlebar	equ	GA_Dummy+53
	endc

	ifd	scn
	include	libraries/screennotify.i
	endc

	ifd	newmouse
	include	devices/newmouse.i
	endc

	rsreset
_exec	rs.l	1
_mport1	rs.l	1	msg port
_sgr	rs.l	1	default SGR settings
_asdss	rs.b	1	flag: aSDSS has been used
_boot	rs.b	1	flag: bootshell
_clrw	rs.b	1	flag: menu console/clear window selected
_dtf	rs.b	1	init: DOSTask failure if set
_locb	rs.l	1	locale: libbase
_cat	rs.l	1	locale: catalog

	ifd	scn
_client	rs.l	1	scn: wb client handle
_scnm	rs.l	1	scn: message
	endc

Init:	suba.w	#_RS+124,SP
	movem.l	D2-D7/A2-A6,-(SP)
	lea	(124+11*4,sp),a4
	moveq	#_RS/4-1,d0
	movea.l	a4,a0
\c	clr.l	(a0)+
	dbf	d0,\c
	clr.w	(154,SP)
	movea.l	(4).W,A6
	move.l	A6,(_exec,A4)
	lea	(44,SP),a2
	moveq	#0,d2
	move.l	(ThisTask,a6),($5c,SP)
	bsr	_OpenLibs		open libraries
	beq	lbC000E84
	movea.l	(4,a2),A6
	sys	WaitPkt
	movea.l	d0,a5
	addq.b	#1,(_sgr+1,A4)	default char colour
	movea.l	(a2),a6
	sys	CreateMsgPort
	move.l	D0,(_mport1,A4)
	beq	_err
	movea.l	d0,a0
	moveq	#$30,D0
	sys	CreateIORequest
	move.l	D0,(96,SP)
	beq	_err1
	movea.l	D0,A1
	movea.l	d0,a3

	ifd	rom
	moveq	#ODTAG_INPUT,d0		ROM hack
	movea.l	d0,a0
	else
	lea	(inputdevice.MSG,pc),A0
	endc

	moveq	#0,D0
	move.l	D0,D1
	sys	OpenDevice
	tst.b	D0
	bne	_err2
	clr.l	-(SP)
	pea	(KingCONDOSpro.MSG,pc)
	move.l	#NP_Name,-(SP)
	pea	(Task,PC)
	move.l	#NP_Entry,-(SP)
	movea.l	(4,A2),A6
	move.l	SP,D1
	sys	CreateNewProc
	lea	(5*4,SP),SP
	movea.l	(A2),A6
	tst.l	d0
	beq	_err2
	movea.l	d0,a1
	move.l	a4,(TC_Userdata,a1)		stuff a4 to userdata
	move.l	#SIGBREAKF_CTRL_F,d0
	sys	Signal
.wait	movea.l	(_mport1,A4),A0
	sys	WaitPort
	movea.l	(_mport1,A4),A0
	sys	GetMsg
	tst.l	d0
	beq	.wait
	tst.b	(_dtf,a4)
	bne	_err2
	movea.l	D0,A0
	move.l	($1A,A0),(104,sp)	DOS Task communication msg port
	movea.l	(4,a2),A6
	move.l	($1C,A5),D1	DeviceNode
	asl.l	#2,D1
	movea.l	D1,A1
	clr.l	(8,A1)		dn_Task
	move.l	a5,d1
	moveq	#-1,D2
	moveq	#0,D3
	sys	ReplyPkt
	move.l	(20,a3),(44,a2)		input.device ptr (same as 88,a2)
	moveq	#1,D5
	suba.l	A5,A5
	movea.l	(a2),a6
	bra	lbC000E48

_err2	movea.l	a3,A0
	sys	DeleteIORequest
_err1	movea.l	(_mport1,A4),a0
	sys	DeleteMsgPort
_err	moveq	#ERROR_NO_FREE_STORE,d3
	move.l	a5,d1
	moveq	#0,d2
	movea.l	(4,a2),a6
	sys	ReplyPkt
	bra	lbC000E84

\rtag	dc.w	$4AFC
	dc.l	\rtag	;rt_MatchTag
	dc.l	_end	;rt_EndSkip
	dc.b	0	;rt_Flags
	dc.b	41	;rt_Version
	dc.b	0	;rt_Type
	dc.b	135	;rt_Pri
	dc.l	conhandler.MSG	;rt_Name
	dc.l	KingCONhandle.MSG	;rt_IdString
	dc.l	Init	;rt_Init
conhandler.MSG	dc.b	'con-handler',0

	ifnd	rom
doslib	dc.b	'dos.library',0
intuitionlibr.MSG	dc.b	'intuition.library',0
gadtoolslibra.MSG	dc.b	'gadtools.library',0
graphicslibra.MSG	dc.b	'graphics.library',0
utilitylibrar.MSG	dc.b	'utility.library',0
workbenchlibr.MSG	dc.b	'workbench.library',0
iconlibrary.MSG	dc.b	'icon.library',0
	endc
asllibrary.MSG	dc.b	'asl.library',0
diskfontlibra.MSG	dc.b	'diskfont.library',0
iffparselibra.MSG	dc.b	'iffparse.library',0
localelib	dc.b	'locale.library',0

	ifd	scn
scnlib	dc.b	'screennotify.library',0
	endc

_OpenLibs	movem.l	d3/a2,-(SP)
	move.l	a6,(a2)+		exec

	moveq	#9,d0
	movea.l	a2,a0
\clr	clr.l	(a0)+
	dbf	d0,\clr

	IFD	rom
	moveq	#OLTAG_DOS,d0
	sys	TaggedOpenLibrary
	move.l	d0,(a2)+
	beq	\end
	moveq	#OLTAG_INTUITION,d0
	sys	TaggedOpenLibrary
	move.l	d0,(a2)+
	beq	\end
	moveq	#OLTAG_GADTOOLS,d0
	sys	TaggedOpenLibrary
	move.l	d0,(a2)+
	beq	\end
	moveq	#OLTAG_GRAPHICS,d0
	sys	TaggedOpenLibrary
	move.l	d0,(a2)+
	beq	\end
	ELSE
	lea	(doslib,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	move.l	D0,(A2)+		(4,a2) = dos
	beq	\end
	lea	(intuitionlibr.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	move.l	D0,(A2)+		(8,a2) = intuition
	beq	\end
	lea	(gadtoolslibra.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	move.l	D0,(A2)+		(12,a2) = gadtools
	beq	\end
	lea	(graphicslibra.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	move.l	D0,(A2)+		(16,a2) = gfx
	beq	\end
	ENDC

	sys	Forbid
	lea	(Workbench.MSG,pc),a1
	sys	FindTask
	move.l	d0,d3
	beq	.skip

	IFD	rom
	moveq	#OLTAG_WORKBENCH,d0
	sys	TaggedOpenLibrary
	ELSE
	lea	(workbenchlibr.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	ENDC

.skip	sys	Permit
	move.l	D0,(A2)+		(20,a2) = wb

	addq.l	#4,a2			(24,a2) = diskfont (fake fail)

	IFD	rom
	moveq	#OLTAG_UTILITY,d0
	sys	TaggedOpenLibrary
	ELSE
	lea	(utilitylibrar.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	ENDC

	move.l	D0,(A2)+		(28,a2) = utility
	beq	\end

	tst.b	d2
	beq	.skip4

\more	;lea	(iffparselibra.MSG,pc),A1
	;moveq	#37,D0
	;sys	OpenLibrary
	;move.l	D0,(A2)+		(32,a2) = iffparse (fake fail)
	addq.l	#8,a2			(36,a2) = asl.library (fake fail)

	tst.l	d3
	beq	.skip4

	IFD	rom
	moveq	#OLTAG_ICON,d0
	sys	TaggedOpenLibrary
	ELSE
	lea	(iconlibrary.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	ENDC

	move.l	D0,(A2)			(40,a2) = icon

.skip4	moveq	#1,d0		dummy set CCR to nonzero
\end	movem.l	(SP)+,d3/a2
	rts

_CloseLibs	movea.l	(a2)+,a6	execbase
	moveq	#9,d2
\loop	movea.l	(A2)+,A1
	sys	CloseLibrary
	dbra	D2,\loop
	rts

lbC00018A	movem.l	D5-D7/A2/A3/A5/a6,-(SP)
	move.l	a0,D7
	movea.l	a1,A3
	movea.l	d0,A5
	moveq	#0,D6
	lea	($10a,a5),a6
	move.b	([A6]),D5
	beq.b	\1
	cmpi.b	#$9B,D5
	bne.b	\7
	tst.w	($102,A5)
	beq.b	\7
\1	addq.l	#1,(A6)
	lea	($DFC,A5),A2
	move.l	A2,D0
	addi.l	#$800,D0
	move.l	(A6),D1
	cmp.l	D0,D1
	bne.b	\2
	cmp.l	($10E,A5),D1
	beq.b	\2
	move.l	A2,(A6)
\2	tst.b	D5
	bne.b	\3
	btst	#1,($162,A5)
	beq.b	\4
\3	move.b	D5,(A3)
	moveq	#1,D0
	bra.b	\11

\4	moveq	#0,D0
	bra.b	\11

\5	movea.l	(A6),A0
	addq.l	#1,(A6)
	move.b	(A0)+,(A3)
	addq.l	#1,D6
	subq.l	#1,D7
	lea	($DFC,A5),A2
	move.l	A2,D0
	addi.l	#$800,D0
	cmpa.l	D0,a0
	bne.b	\6
	cmp.l	($10E,A5),a0
	beq.b	\6
	move.l	A2,(A6)
\6	moveq	#10,D1
	cmp.b	(A3)+,D1
	seq	D0
	neg.b	D0
	beq.b	\7
	moveq	#0,D7
\7	moveq	#0,D0
	movea.l	(A6),A2
	cmp.l	D0,D7
	bls.b	\9
	move.b	(A2),D0
	beq.b	\9
	tst.w	($102,A5)
	beq.b	\8
	cmpi.b	#$9B,D0
	beq.b	\9
\8	cmpa.l	($10E,A5),A2
	bne.b	\5
\9	cmpa.l	($10E,A5),A2
	bne.b	\10
	lea	($DFC,A5),A0
	move.l	A0,(A6)+
	move.l	A0,(A6)
\10	move.l	D6,D0
\11	movem.l	(SP)+,D5-D7/A2/A3/A5/a6
	rts

lbC000266	movem.l	A2/A3,-(SP)
	moveq	#0,D1
	movea.l	($10A,A1),A3
	movea.l	($10E,A1),A0
	bra.b	\3

\1	moveq	#10,D0
	cmp.b	(A3),D0
	bne.b	\2
	addq.l	#1,D1
\2	addq.l	#1,A3
	lea	($DFC,A1),A2
	move.l	A2,D0
	addi.l	#$800,D0
	cmpa.l	D0,A3
	bne.b	\3
	cmpa.l	($10E,A1),A3
	beq.b	\3
	movea.l	A2,A3
\3	cmpa.l	a0,A3
	bne.b	\1
	move.l	D1,D0
	movem.l	(SP)+,A2/A3
	rts

lbC0002B2	move.l	A6,-(SP)
	moveq	#22,D0
	moveq	#1,D1
	swap	D1
	movea.l	(_exec,A4),A6
	sys	AllocVec
	tst.l	D0
	beq.b	\1
	lea	($f0,a5),A0
	movea.l	D0,a1
	ADDHEAD
	move.l	a3,(14,A1)
	move.l	A1,D0
\1	movea.l	(SP)+,A6
	rts

lbC0002E6	movem.l	D2/D3/D7/A2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	moveq	#1,D7
	move.w	($76,A3),D0
	beq	\8
	bra	\7

\1	lea	($F0,A3),A0
	movea.l	(A5),A6
	sys	RemHead
	movea.l	D0,A2
	movea.l	(14,A2),A0
	move.l	(dp_Type,A0),D0
	moveq	#20,D1
	sub.l	D1,D0
	beq.b	\2
	moveq	#$3E,D1
	sub.l	D1,D0
	bne	\7
	movea.l	(dp_Arg2,A0),A1
	movea.l	(dp_Arg3,A0),a0
	move.l	A3,d0
	bsr	lbC00018A
	move.l	D0,D2
	move.l	(14,A2),D1
	movea.l	(4,A5),A6
	moveq	#0,D3
	sys	ReplyPkt
	movea.l	A2,A1
	movea.l	(A5),A6
	sys	FreeVec
	bra.b	\7

\2	movea.l	(18,A2),A1
	movea.l	(A5),A6
	sys	CheckIO
	tst.l	D0
	beq.b	\3
	move.l	(14,A2),D1
	movea.l	(4,A5),A6
	moveq	#0,D2
	move.l	D2,D3
	bra.b	\6

\3	movea.l	(18,A2),a1
	bsr	lbC001362
	btst	#1,($162,A3)
	beq.b	\4
	moveq	#0,D0
	bra.b	\5

\4	movea.l	A3,a1
	bsr	lbC000266
\5	move.l	D0,D3
	move.l	(14,A2),D1
	movea.l	(4,A5),A6
	moveq	#-1,D2
\6	sys	ReplyPkt
	clr.l	(14,A2)
\7	movea.l	($10A,A3),A0
	cmpa.l	($10E,A3),A0
	beq.b	\9
	movea.l	($F0,A3),A0
	tst.l	(A0)
	bne	\1
	bra.b	\9

\8	tst.w	($106,A3)
	beq	\9
	moveq	#0,d7
\9	move.w	D7,D0
	movem.l	(SP)+,D2/D3/D7/A2/A3/A5/A6
	rts

lbC000470	movem.l	D6/D7/A2/A5,-(SP)
;	movea.l	($1C,SP),A1
	movea.l	a5,A2
	movea.l	a0,A5
	moveq	#1,D6
	move.b	(io_Error,A1),D0
	bpl.b	\1
	moveq	#0,D0
	bra.b	\2

\1	move.l	(io_Actual,A1),D0
\2	move.l	D0,D7
	btst	#1,($162,A2)
	beq.b	\3
	move.l	D7,-(SP)
	move.l	A2,-(SP)
	move.l	A5,-(SP)
	bsr	lbC004638
	bra.b	\5

\3	btst	#5,($161,A2)	ASYNC?
	beq.b	\4
	move.w	($76,A2),D0
	beq.b	\4
	lea	($F0,A2),A0
	movea.l	($F8,A2),A1
	cmpa.l	A0,A1
	bne.b	\4
	movea.l	A2,a1
	bsr	lbC00C90E
	bne.b	\4
	move.l	D7,-(SP)
	move.l	A2,-(SP)
	move.l	A5,-(SP)
	bsr	lbC0063B0
	bra.b	\5

\4	move.l	D7,-(SP)
	move.l	A2,-(SP)
	move.l	A5,-(SP)
	bsr	lbC004C22
\5	move.l	($78,A2),D0
	beq	\13
	move.l	A2,a0
	bsr	lbC001596
	tst.w	($104,A2)
	bne.b	\6
	movea.l	A2,a0
	movea.l	A5,a1
	bsr	lbC0002E6
	move.w	D0,D6
\6	move.l	($160,a2),d0
	btst	#0,d0
	beq.b	\11
	btst	#9,d0
	bne.b	\11
	tst.w	($106,a2)
	beq.b	\11
	tst.b	($100,a2)
	bne.b	\11
	movea.l	($10A,a2),A0
	cmpa.l	($10E,a2),A0
	beq.b	\11
;	btst	#7,($161,a2)
;	beq.b	lbC000420
	tst.b	($161,a2)
	bpl	\7
	movea.l	a2,a0
	movea.l	A5,a1
	bsr	lbC000ED8
	tst.w	D0
	beq.b	\7
;lbC000420	btst	#7,($161,a2)
;	bne.b	\8
	tst.b	($161,a2)
	bmi	\8
\7	movea.l	a2,a0
	bsr	_CloseWindow
\8	clr.w	($106,a2)
	lea	($DFC,a2),A0
	movea.l	($10E,a2),A1
	cmpa.l	A1,A0
	bne.b	\9
	lea	($15FC,a2),A0
	bra.b	\10

\9	subq.l	#1,A1
	movea.l	A1,A0
\10	tst.b	(A0)
	bne.b	\12
	move.l	A0,($10E,a2)
	bra.b	\12

\11	tst.w	($104,a2)
	beq.b	\12
	clr.w	($104,a2)
	movea.l	a2,a0
	movea.l	A5,a1
	bsr	lbC000ED8
\12	move.w	D6,D0
\13	movem.l	(SP)+,D6/D7/A2/A5
	rts

;lbC0070D6	lea	(Semaphore,A4),A0
;	sys	ReleaseSemaphore
;	clr.l	($38,A2)
;	moveq	#1,D5
;	move.w	D5,($9A,SP)
;	bra	lbC000E48

lbC0005FE	movea.l	D0,A3
	movea.l	(10,A3),A3	DosPacket
	move.l	A3,($9C,SP)
	moveq	#-1,D7
	moveq	#0,D6
	move.w	($9A,SP),D5
	move.l	(8,A3),D0
	subq.l	#5,D0
	beq	_ACTION_DIE
	moveq	#15,D1
	sub.l	D1,D0
	beq	_ACTION_WAIT_CHAR
	subq.l	#5,D0
	beq	_ACTION_DISK_INFO
	subq.l	#5,D0
	beq	_ACTION_TIMER
	moveq	#$34,D1
	sub.l	D1,D0
	beq	_ACTION_READ
	subq.l	#5,D0
	beq	_ACTION_WRITE
	subi.l	#$38B,D0
	beq	_ACTION_SCREEN_MODE
	subq.l	#1,D0
	beq	_ACTION_CHANGE_SIGNAL
	subq.l	#6,D0
	beq	_ACTION_READ_RETURN
	subq.l	#3,D0
	beq.b	_ACTION_FIND_X
	subq.l	#1,D0
	beq.b	_ACTION_FIND_X
	subq.l	#1,D0
	beq.b	_ACTION_FIND_X
	subq.l	#1,D0
	beq	_ACTION_END
	subi.l	#$3E3,D0
	beq	_ACTION_STACK
	subq.l	#1,D0
	beq	_ACTION_QUEUE
	bra	lbC000D3A

_ACTION_FIND_X	tst.w	D5
	beq.b	\1
	moveq	#0,D7
	moveq	#$2E,D6
	not.b	D6
	bra	lbC000D48

\1	movea.l	(dp_Port,a3),a0
	movea.l	(MP_SIGTASK,a0),a0
	move.l	(pr_TaskNum,a0),d0	pr_TaskNum of our originator
	subq.l	#1,d0			if 1 it must be a bootshell
	seq	(_boot,a4)
	move.l	A3,-(SP)
;	move.l	A5,-(SP)
	lea	($34-4,SP),a0
	bsr	lbC0029DE
	lea	(4,sp),sp
	movea.l	D0,A5
	tst.l	D0
	bne.b	\2
	moveq	#0,D7
	move.l	($6C,SP),D6
	bra	lbC000D44

\2	move.l	A3,D1
	movea.l	($30,SP),A6
	moveq	#-1,D2
	moveq	#0,D3
	sys	ReplyPkt
	move.l	D3,($9C,SP)
	tst.l	($DC,A5)
	bne	lbC000D5E
	move.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC00CC76
	btst	#0,($163,A5)
	beq	.3
;	btst	#7,($161,A5)
;	beq	.3
	tst.b	($161,A5)
	bpl	.3
	btst	#0,($160,A5)
	beq.b	.1
	btst	#6,($160,A5)
	bne.b	.2
.1	move.l	A5,a0
	lea	($2c,SP),a1
	bsr	_addAppIcon
	btst	#6,($160,A5)
	beq	.3
.2	move.l	A5,a0
	lea	($2c,SP),a1
	bsr	_addAppMenu
.3	bra	lbC000D5E

_ACTION_READ	tst.w	D5
	beq.b	\1
	moveq	#0,D7
	bra	lbC000D48

\1	move.l	($e8,a5),($ec,a5)
	move.l	(dp_Arg1,A3),($72,A5)
	tst.l	($78,A5)
	bne.b	\3
	btst	#2,($160,A5)
	beq.b	\2
	tst.l	($D8,A5)
	bne.b	\3
	tst.l	($D4,A5)
	bne.b	\3
\2	moveq	#1,d0
	move.l	A5,a0
	lea	($34-8,SP),a1
	bsr	_OpenWindow
	tst.w	D0
	bne.b	\3
	moveq	#0,D7
	move.l	($6C,SP),D6
	bra	lbC000D48

\3	tst.l	($78,A5)
	beq.b	\6
	btst	#5,($160,A5)
	beq.b	\4
	btst	#1,($162,A5)
	bne.b	\4
	tst.l	($90,A5)
	beq.b	\4
	btst	#3,($162,A5)
	bne.b	\4
	move.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC00AE7A
\4	btst	#6,($161,A5)
	beq.b	\5
	movea.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC001954
\5	btst	#5,($161,A5)
	beq.b	\6
	btst	#1,($162,A5)
	bne.b	\6
	move.l	($116,A5),D0
	ble.b	\6
	movea.l	($A8,A5),a1
	bsr	lbC001362
\6	movea.l	($10A,A5),A0
	cmpa.l	($10E,A5),A0
	bne.b	\8
	bsr	lbC0002B2
	beq.b	\7
	clr.l	($9C,SP)
	bra	lbC000D5E

\7	moveq	#0,D7
	moveq	#$67,D6
	bra	lbC000D48

\8	movea.l	($18,A3),A1
	movea.l	($1C,A3),a0
	move.l	A5,d0
	bsr	lbC00018A
	move.l	D0,D7
	btst	#5,($161,A5)
	beq	lbC000D48
	moveq	#1,D0
	cmp.l	D0,D7
	ble	lbC000D48
	btst	#1,($162,A5)
	bne	lbC000D48
	tst.l	($78,A5)
	beq.b	\9
	movea.l	($18,A3),A0
	move.l	D7,d0
	move.l	A5,d1
	lea	($38-12,SP),a1
	bsr	lbC0031DE
\9	movea.l	($18,A3),A0
	move.l	D7,d0
	lea	($38-12,SP),a1
	move.l	A5,d1
	bsr	lbC00B3A6
	bra	lbC000D48

_ACTION_WRITE	tst.w	D5
	beq.b	\1
	moveq	#0,D7
	moveq	#$2E,D6
	not.b	D6
	bra	lbC000D48

\1	move.l	($14,A3),($72,A5)
	tst.l	($78,A5)
	bne.b	\3
	btst	#2,($160,A5)
	beq.b	\2
	tst.l	($D8,A5)
	bne.b	\3
	tst.l	($D4,A5)
	bne.b	\3
\2	moveq	#1,d0
	move.l	A5,a0
	lea	($34-8,SP),a1
	bsr	_OpenWindow
	tst.w	D0
	bne.b	\3
	moveq	#0,D7
	move.l	($6C,SP),D6
	bra	lbC000D48

\3	tst.l	($78,A5)
	beq	\10
	lea	($30-4,SP),a0
	bsr	lbC00B96A
	move.w	#1,($108,A5)
	movea.l	([$98,A5],$18),A2
	movea.l	($78,A5),A0
	move.w	(8,A0),D5
	move.w	(10,A0),D4
	move.b	($126,A2),D0
	move.b	D0,($28,SP)
	btst	#4,($160,A5)
	beq.b	\4
	bset	#4,D0
	bra.b	\5

\4	bclr	#4,D0
\5	move.b	D0,($126,A2)
	movea.l	($18,A3),A0
	bsr	lbC001386
	lea	($2C,SP),A0
	clr.l	($40,A0)
	move.l	D0,D7
	btst	#4,($126,A2)
	beq.b	\6
	bset	#4,($160,A5)
	bra.b	\7

\6	bclr	#4,($160,A5)
\7	move.b	($28,SP),D0
	andi.b	#$10,D0
	or.b	D0,($126,A2)
	movea.l	($78,A5),A0
	cmp.w	(8,A0),D5
	bne.b	\8
	cmp.w	(10,A0),D4
	beq.b	\9
\8	move.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC00AE7A
\9	move.l	($6C,SP),D6
	movea.l	($18,A3),A0
	move.l	($1C,A3),d0
	lea	($38-12,SP),a1
	move.l	A5,d1
	bsr	lbC00B3A6
	bra	lbC000D48

\10	;move.l	A5,-(SP)
	lea	($30-4,SP),a0
	bsr	lbC00B96A
	movea.l	($18,A3),A0
	move.l	($1C,A3),d0
	lea	($3C-16,SP),a1
	move.l	A5,d1
	bsr	lbC00B3A6
	move.l	($1C,A3),D7
	bra	lbC000D48

_ACTION_READ_RETURN
	tst.w	D5
	bne	lbC000D48
	movea.l	(A3),a1
	lea	($34-8,SP),a0
	bsr	lbC000470
	clr.l	($9C,SP)
	move.w	D0,($A0,SP)
	bra	lbC000D5E

_ACTION_DISK_INFO	move.l	($78,A5),d3	window
	bne.b	\1
	moveq	#1,d0
	move.l	A5,a0
	lea	($34-8,SP),a1
	bsr	_OpenWindow
	move.l	($78,A5),D3
	beq.b	\4
\1	btst	#1,($162,A5)
	beq.b	\2
	move.l	#'RAW'|0,D1
	bra.b	\3

\2	move.l	#'CON'|0,D1
\3	movea.l	(dp_Arg1,A3),A0	InfoData
	adda.l	a0,a0
	st	($100,A5)	attn: disk info packet has been called!
	adda.l	a0,a0
	lea	(id_DiskType,A0),a0
	move.l	D1,(A0)+
	move.l	d3,(a0)+	id_VolumeNode
	move.l	($A8,A5),(a0)	id_InUse
	bra	lbC000D48

\4	moveq	#0,D7
	move.l	($6C,SP),D6
	bra	lbC000D48

_ACTION_SCREEN_MODE	tst.l	(dp_Arg1,A3)
	beq.b	\1
	bset	#1,($162,A5)	RAW mode
	tst.l	($78,A5)	do we have a window?
	beq	\4
	lea	($30-4,SP),a0
	bsr	lbC00B96A
	moveq	#1,d1
	move.l	($160,A5),d0
	lea	($80-16,SP),a0
	bsr	lbC009BA2
	lea	($48-28,SP),a1
	move.l	a1,d1
	move.l	A5,a1
	bsr	_doio
	moveq	#0,d1
	move.l	#$82400,d0
	lea	($94-36,SP),a0
	bsr	lbC009BA2
	lea	($5C-48,SP),a1
	move.l	a1,d1
	move.l	A5,a1
	bsr	_doio
	bra.b	\3

\1	bclr	#1,($162,A5)	CON mode
	clr.l	($116,A5)
	tst.l	($78,A5)	do we have a window?
	beq.b	\2
	moveq	#1,d1
	move.l	($160,A5),d0
	lea	($78-8,SP),a0
	bsr	lbC009BA2
	lea	($40-8-12,SP),a1
	move.l	a1,d1
	move.l	A5,a1
	bsr	_doio
\2	movea.l	($10A,A5),A0
	cmpi.b	#$9B,(A0)
	bne.b	\3
	move.l	($10E,A5),($10A,A5)
\3	move.l	($78,A5),D0
	beq.b	\4
	move.l	($A4,A5),d1
	beq.b	\4
	movea.l	D0,A1
	move.l	(wd_MenuStrip,A1),d0
	beq	.skip		workaround for c:ed clearing menustrip
	cmp.l	d1,d0
	bne.b	\4
.skip	movea.l	a5,a3
	lea	($2c,SP),a5
	bsr	lbC000E90
	bsr	lbC00C12E
	movea.l	a3,a5
\4	bra	lbC000D48

_ACTION_END	move.l	($14,A3),($72,A5)
;	lea	(44,SP),a0
	bsr	lbC002F88
	bne	\1
	ifd	async
	move.l	a3,d1
	moveq	#-1,D2
	move.l	D6,D3
	movea.l	(48,SP),A6
	sys	ReplyPkt
	clr.l	($9c,sp)
	bsr	_cw
	endc
	suba.l	a5,a5
	bra	lbC000D44

\1	tst.w	($76,A5)
	bne	lbC000D48
	tst.w	($106,A5)
	seq	D0
	neg.b	D0
	ext.w	D0
	move.w	D0,($A0,SP)
	bra	lbC000D48

_ACTION_WAIT_CHAR	move.l	($78,A5),d3	do we have a window?
	bne.b	\2
	btst	#2,($160,A5)	KEEPCLOSED?
	beq.b	\1
	tst.l	($D8,A5)
	bne.b	\2
	tst.l	($D4,A5)
	bne.b	\2
\1	moveq	#1,d0
	move.l	A5,a0
	lea	($34-8,SP),a1
	bsr	_OpenWindow
	tst.w	D0
	bne.b	\2
	moveq	#0,D7
	move.l	($6C,SP),D6
	bra	lbC000D48

\2	btst	#5,($161,A5)	ASYNC?
	beq.b	\3
	btst	#1,($162,A5)	RAW?
	bne.b	\3
	move.l	($116,A5),D0
	ble.b	\3
	tst.l	d3		do we have a window?
	beq.b	\3
	movea.l	($A8,A5),a1
	bsr	lbC001362
\3	movea.l	($10A,A5),A0
	cmpa.l	($10E,A5),A0
	bne	\5
	bsr	lbC0002B2
	movea.l	D0,A2
	beq	lbC000D48
	moveq	#0,D3
	bra.b	.2

.1	addq.b	#1,D3
.2	moveq	#8,D0
	cmp.b	D0,D3
	bcc.b	.3
	tst.l	($AC,A5,D3.L*4)
	bne.b	.1
.3	move.l	D3,D0
	subq.b	#8,D0
	bne.b	.4
	moveq	#0,D0
	bra	\4

.4	movea.l	(44,sp),A6
	lea	(ACTION_TIMER).W,a0
	move.l	($A0,A5),d1
	lea	(44,SP),a1
	moveq	#IOSTD_SIZE+dp_SIZEOF+6,D0
	bsr	lbC00160E
	beq	\4
	move.l	A1,($AC,A5,D3.L*4)
	move.w	#9,(io_Command,A1)
	clr.l	(io_Actual,A1)
	move.l	(20,a3),(io_Length,A1)
	move.b	D3,($60,A1)
	move.l	A1,d3
	sys	SendIO
	move.l	D3,($12,A2)
	movea.l	D3,A0
	move.l	A2,($62,A0)
	clr.l	($9C,SP)
	bra	lbC000D5E

\4	lea	($F0,A5),A0
	movea.l	(_exec,A4),A6
	sys	RemTail
	movea.l	A2,A1
	sys	FreeVec
	moveq	#0,D7
	moveq	#$67,D6
	bra	lbC000D48

\5	btst	#1,($162,A5)
	bne	lbC000D48
	movea.l	A5,a1
	bsr	lbC000266
	move.l	D0,D6
	bra	lbC000D48

_ACTION_TIMER	movea.l	(A3),A2
	movea.l	($62,A2),A3
	clr.l	($9C,SP)
	move.l	(14,A3),D0
	beq.b	\1
	move.l	D0,D1
	movea.l	($30,SP),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
	movea.l	A3,A1
	movea.l	(_exec,A4),A6
	sys	Remove
\1	movea.l	A3,A1
	movea.l	(_exec,A4),A6
	sys	FreeVec
	movea.l	A5,a0
	bsr	lbC0016EE
	bra	lbC000D48

_ACTION_CHANGE_SIGNAL	movea.l	(dp_Arg1,A3),A0
	move.l	A0,($72,A5)	internal lock
	moveq	#1,D7
	moveq	#pr_MsgPort,D6
	add.l	($22,A0),D6	old MsgPort
	movea.l	(dp_Arg2,A3),a1	new MsgPort
	cmpi.b	#PA_SIGNAL,(MP_FLAGS,a1)
	bne	lbC000D48
	move.l	(MP_SIGTASK,a1),d0
	move.l	D0,($22,A0)	new process to receive signals
	movea.l	(4,A0),A1
	tst.l	(4,A1)
	bne	lbC000D48
	move.l	d0,($ec,a5)
	tst.l	($78,A5)	is the window open?
	beq	lbC000D48
	movea.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC001954
	bra	lbC000D48

_ACTION_STACK
_ACTION_QUEUE

	tst.w	D5
	beq.b	\1
	moveq	#0,D7
	moveq	#$2E,D6
	not.b	D6
	bra	lbC000D48

\1	movea.l	($10A,A5),A2
	move.l	($10E,A5),D0
	cmp.l	A2,D0
	bge.b	\2
	moveq	#$40,D3
	lsl.l	#5,D3
	sub.l	A2,D3
	add.l	D0,D3
	bra.b	\3

\2	sub.l	A2,D0
	move.l	D0,D3
\3	move.l	($1C,A3),D0
	add.l	D0,D3
	cmpi.l	#$800,D3
	bls.b	\4
	moveq	#0,D7
	moveq	#$67,D6
	bra	lbC000D48

\4	movea.l	($18,A3),A2
	cmpi.l	#$7D3,(8,A3)
	bne.b	\5
	movea.l	A2,a0
	movea.l	A5,a1
	bsr	lbC0098BA
	bra.b	\8

\5	movem.l	D7/A3/A6,-(SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	move.l	d0,D7
	adda.l	D7,A0
	lea	($DFC,A0),A1
	movea.l	($10A,A5),A0
	cmpa.l	A1,A0
	bcs.b	\6
	suba.l	D7,A0
	move.l	D7,D0
	movea.l	A0,A1
	movea.l	A2,A0
	sys	CopyMem
	sub.l	D7,($10A,A5)
	bra.b	\7

\6	lea	($DFC,A5),A0
	move.l	($10A,A5),D0
	move.l	A0,D1
	movea.l	A5,A0
	suba.l	D7,A0
	sub.l	D1,D0
	movea.l	D1,A1
	adda.l	D0,A0
	lea	($15FC,A0),A3
	move.l	A3,($10A,A5)
	sub.l	D0,D7
	movea.l	A2,A0
	adda.l	D7,A0
	sys	CopyMem
	movea.l	A2,A0
	movea.l	($10A,A5),A1
	move.l	D7,D0
	sys	CopyMem
\7	movem.l	(SP)+,D7/A3/A6

\8	movea.l	A5,a0
	lea	($2c,SP),a1
	bsr	lbC0002E6
	move.l	($1C,A3),D7
	move.w	D0,($A0,SP)
	bra.b	lbC000D48


_ACTION_DIE	move.l	A5,D0
	beq.b	lbC000D44
	move.w	#1,($9A,SP)
	bra.b	lbC000D48

lbC000D3A	moveq	#0,D7
	moveq	#$2E,D6
	not.b	D6
	move.l	A5,D0
	bne.b	lbC000D48
lbC000D44	clr.w	($A0,SP)
lbC000D48	move.l	($9C,SP),D1
	beq.b	lbC000D5E
	move.l	D7,D2
	move.l	D6,D3
	movea.l	($30,SP),A6
	sys	ReplyPkt

lbC000D5E	movea.l	($5C,SP),A0
	adda.w	#pr_MsgPort,A0
	movea.l	($2c,sp),A6
	sys	GetMsg
	tst.l	D0
	bne	lbC0005FE
	move.w	($A0,SP),D5
	beq	lbC000E38
	move.l	A5,D0
	beq	lbC000E38
	move.l	($A8,A5),D0
	beq.b	lbC000DB6
	movea.l	D0,A1
	sys	CheckIO
	tst.l	D0
	beq	lbC000DB6
	movea.l	($A8,A5),A1
	sys	WaitIO
	movea.l	($A8,A5),a1
;	move.l	A5,-(SP)
	lea	(44,SP),a0
	bsr	lbC000470
	move.w	D0,D5
lbC000DB6	move.l	A5,a0
	lea	(44,SP),a1
	bsr	lbC00BF90		check/process window messages
	tst.l	($D0,A5)
	beq.b	lbC000DE0
	movea.l	A5,a1
	bsr	lbC00C90E
	beq	lbC000E14
	move.l	($A8,A5),a1
	bsr	lbC001362
	bra	lbC000E14

lbC000DE0	tst.l	($D4,A5)
	bne	lbC000DEC
	tst.l	($D8,A5)
	beq	lbC000E14
lbC000DEC	tst.l	($78,A5)
	bne	lbC000E14

	moveq	#0,D2
	movea.l	(_exec,A4),A6
	bra	\4

\1	movea.l	D0,A1
	tst.l	(am_NumArgs,A1)
	bne	\3
	move.w	(am_Type,A1),D0
	subq.w	#AMTYPE_APPICON,D0
	beq	\2
	subq.w	#AMTYPE_APPMENUITEM-AMTYPE_APPICON,D0
	bne	\3
\2	moveq	#1,D2
\3	sys	ReplyMsg
\4	movea.l	($CC,A5),A0
	sys	GetMsg
	tst.l	D0
	bne	\1
	tst.l	D2
	beq	lbC000E14
	moveq	#1,d0
	move.l	A5,a0
	lea	(44,SP),a1
	bsr	_OpenWindow		reopen window (deiconify)
lbC000E14	tst.w	($104,A5)
	beq.b	lbC000E2A
	movea.l	A5,a0
	lea	(44,SP),a1
	bsr	lbC000ED8		close window (iconify)
	clr.w	($104,A5)

lbC000E2A	move.l	($7C,A5),D0	window signal
	or.l	($E0,A5),D0		wbapp/scn signals
	ori.w	#SIGF_DOS,D0
	bra.b	lbC000E40

lbC000E38	tst.w	D5
	beq.b	lbC000E48
	moveq	#$40,D0
	lsl.l	#2,D0
lbC000E40	movea.l	(_exec,A4),A6
	sys	Wait
lbC000E48	move.w	D5,($A0,SP)
	bne	lbC000D5E
	move.l	A5,D0
	beq.b	lbC000E60
;	lea	(44,SP),a0
	bsr	lbC002F88
	ifd	async
	bne	lbC000E60
;	lea	(44,SP),a0
	bsr	_cw
	endc

lbC000E60	lea	(44,SP),a2
	moveq	#100,d0
	movea.l	A2,a0
	bsr	_DoSyncDos
	movea.l	($60,SP),A3
	movea.l	a3,a1
	movea.l	(A2),A6
	sys	CloseDevice
	movea.l	a3,A0
	sys	DeleteIORequest
	movea.l	(_mport1,A4),a0
	sys	DeleteMsgPort
lbC000E84	bsr	_CloseLibs
	moveq	#0,d0
	movem.l	(SP)+,D2-D7/A2-A6
	adda.w	#_RS+124,SP
	rts

lbC000E90	move.l	($A4,A3),d0
	beq.b	\1
	movem.l	A2/A6,-(SP)
	movea.l	D0,a2
	movea.l	($78,A3),A0	window
	cmpa.l	(wd_MenuStrip,A0),A2
	bne.b	\2
	movea.l	(8,A5),A6
	sys	ClearMenuStrip
\2	movea.l	A2,A0
	movea.l	(12,A5),A6
	sys	FreeMenus
	clr.l	($A4,A3)
	movem.l	(SP)+,A2/A6
\1	rts


lbC000ED8	movem.l	A3/A5,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	btst	#0,($160,A3)
	beq	.1
;	btst	#7,($161,A3)
;	beq	.2
	tst.b	($161,A3)
	bpl	.2
	btst	#6,($160,A3)
	bne	.4
.1	bsr	_addAppIcon
.2	btst	#6,($160,A3)
	beq	.3
.4	move.l	A3,a0
	move.l	A5,a1
	bsr	_addAppMenu
.3	tst.l	($D8,A3)
	bne.b	.5
	tst.l	($D4,A3)
	bne	.5
	bsr	_remAppIcon
;	bsr	_remAppMenuItem
	moveq	#0,D0
	bra	.6

.5	movea.l	A3,a0
	bsr	_CloseWindow
	moveq	#1,D0
.6	movem.l	(SP)+,A3/A5
	rts


lbC00C90E	move.l	($CC,A1),d0	wb/app* msg port
	beq.b	\1
	movea.l	D0,a0
	lea	(MP_MSGLIST+LH_TAIL,A0),A1
	movea.l	(MP_MSGLIST+LH_HEAD,A0),A0
	cmpa.l	A1,A0
\1	rts


ascii2long	movem.l	D6/D7,-(SP)
	moveq	#0,D7
	moveq	#1,D6		assume number is positive
	moveq	#0,d0
\1	move.b	(a1)+,d0
	moveq	#32,D1
	cmp.b	d0,D1
	beq.b	\1
	moveq	#'-',D1
	cmp.b	D0,d1
	bne.b	\2
	moveq	#-1,D6		number is negative
	bra.b	\4

\2	subq.l	#1,a1
	bra	\4

\3	move.l	D7,D1
	asl.l	#2,D1
	add.l	D7,D1
	add.l	D1,D1
	add.l	D0,D1
	move.l	D1,D7
	moveq	#'0',D1
	sub.l	D1,D7
\4	cmpa.l	A0,A1
	bhi.b	\5
	move.b	(a1)+,d0
	moveq	#'0',D1
	cmp.b	D1,D0
	bcs.b	\5
	moveq	#'9',D1
	cmp.b	D1,D0
	bls.b	\3
\5	move.l	D7,D0
	tst.l	d6
	bpl	\6
	neg.l	d0
\6	movem.l	(SP)+,D6/D7
	rts

AUTOICONIFY.MSG	dc.b	'AUTOICONIFY',0
AUTOICON.MSG	dc.b	'AUTOICON',0
AUTO.MSG	dc.b	'AUTO',0
NOAUTO.MSG	dc.b	'NOAUTO',0
CLOSE.MSG	dc.b	'CLOSE',0
NOCLOSE.MSG	dc.b	'NOCLOSE',0
BACKDROP.MSG	dc.b	'BACKDROP',0
NOBACKDROP.MSG	dc.b	'NOBACKDROP',0
NOBORDER.MSG	dc.b	'NOBORDER',0
BORDER.MSG	dc.b	'BORDER',0
NODRAG.MSG	dc.b	'NODRAG',0
DRAG.MSG	dc.b	'DRAG',0
NOSIZE.MSG	dc.b	'NOSIZE',0
SIZE.MSG	dc.b	'SIZE',0
SIMPLE.MSG	dc.b	'SIMPLE',0
SMART.MSG	dc.b	'SMART',0
WAIT.MSG	dc.b	'WAIT',0
NOWAIT.MSG	dc.b	'NOWAIT',0
MINI.MSG	dc.b	'MINI',0
MAXI.MSG	dc.b	'MAXI',0
NOREVIEW.MSG	dc.b	'NOREVIEW',0
REVIEW.MSG	dc.b	'REVIEW',0
FNCMODE.MSG	dc.b	'FNCMODE',0
NOFNC.MSG	dc.b	'NOFNC',0
FNC.MSG	dc.b	'FNC',0
NOSHORTCUTS.MSG	dc.b	'NOSHORTCUTS',0
SHORTCUTS.MSG	dc.b	'SHORTCUTS',0
NOMENUS.MSG	dc.b	'NOMENUS',0
MENUS.MSG	dc.b	'MENUS',0
PLAIN.MSG	dc.b	'PLAIN',0
NOSTYLES.MSG	dc.b	'NOSTYLES',0
STYLES.MSG	dc.b	'STYLES',0
INACTIVE.MSG	dc.b	'INACTIVE',0
ACTIVE.MSG	dc.b	'ACTIVE',0
NOGADS.MSG	dc.b	'NOGADS',0
GADS.MSG	dc.b	'GADS',0
JUMP.MSG	dc.b	'JUMP',0
NOJUMP.MSG	dc.b	'NOJUMP',0
ASYNC.MSG	dc.b	'ASYNC',0
SYNC.MSG	dc.b	'SYNC',0
SHOWDIR.MSG	dc.b	'SHOWDIR',0
NOSHOWDIR.MSG	dc.b	'NOSHOWDIR',0
MENUFY.MSG	dc.b	'MENUFY',0
NOMENUFY.MSG	dc.b	'NOMENUFY',0
ICONTITLE.MSG	dc.b	'ICONTITLE',0
ICONPOS.MSG	dc.b	'ICONPOS',0
NOICON.MSG	dc.b	'NOICON',0
NOICONIFY.MSG	dc.b	'NOICONIFY',0
ICONIFY.MSG	dc.b	'ICONIFY',0
SCREEN.MSG	dc.b	'SCREEN',0
FONT.MSG	dc.b	'FONT',0
WINDOW.MSG	dc.b	'WINDOW',0
ALT.MSG	dc.b	'ALT',0
MAXBUF.MSG	dc.b	'MAXBUF',0
PREFS.MSG	dc.b	'PREFS',0
IMAGE.MSG	dc.b	'IMAGE',0
KEEPCLOSED.MSG	dc.b	'KEEPCLOSED',0
NOKEEPCLOSED.MSG	dc.b	'NOKEEPCLOSED',0
POPSCREEN.MSG	dc.b	'POPSCREEN',0
NOPOPSCREEN.MSG	dc.b	'NOPOPSCREEN',0
FASTUPDATE.MSG	dc.b	'FASTUPDATE',0
NICEUPDATE.MSG	dc.b	'NICEUPDATE',0
DIRCACHE.MSG	dc.b	'DIRCACHE',0
NODIRCACHE.MSG	dc.b	'NODIRCACHE',0
SGR.MSG	dc.b	'SGR',0
Workbench.MSG	dc.b	'Workbench',0
consoledevice.MSG	dc.b	'console.device',0
RAW.MSG	dc.b	'RAW',0
KRAWprefs.MSG	dc.b	'KRAW.prefs',0
KCONprefs.MSG	dc.b	'KCON.prefs',0
font.MSG	dc.b	'.font',0
	ifnd	rom
inputdevice.MSG	dc.b	'input.device',0
timerdevice.MSG	dc.b	'timer.device',0
	endc
def_KingCON.MSG	dc.b	'def_'
KingCON.MSG	dc.b	'KingCON',0
;xData.MSG	dc.b	'xData',0
info.MSG0	dc.b	'.info',0
KingCONDC1.MSG	dc.b	'KingCON DC.1',0
KingCONDOSpro.MSG	dc.b	'KingCON DOS-process'
nullbyte	dc.b	0	;static
_catname	dc.b	'kingcon.catalog',0

	ifd	MorphOS
_morph	dc.b	'MorphOS',0
	endc


lbC00117A	movea.l	d1,a0
	move.l	d0,a1
	bsr	ascii2long
	bpl	\1
	moveq	#-1,d0
\1	rts

lbC0012F6	movem.l	D2/A2,-(SP)
	movea.l	d0,A2
	bra.b	\4

\1	cmpa.l	A1,A2
	bhi.b	\2
	move.b	(A2),D0
	move.b	(A0),D1
	cmp.b	D0,D1
	beq.b	\3
	moveq	#0,D2
	move.b	D0,D2
	moveq	#$20,D0
	sub.l	D0,D2
	moveq	#0,D0
	move.b	D1,D0
	cmp.l	D2,D0
	beq.b	\3
\2	moveq	#0,D0
	bra.b	\5

\3	addq.l	#1,A0
	addq.l	#1,A2
\4	tst.b	(A0)
	bne.b	\1
	moveq	#1,D0
\5	movem.l	(SP)+,D2/A2
	rts

lbC001338	tst.b	(LN_TYPE,A1)	a1 must be preserved!
	beq.b	\1
	move.l	A6,-(SP)
	move.l	a1,-(sp)
	movea.l	(_exec,A4),A6
	sys	CheckIO
	movea.l	(sp),A1
	sys	AbortIO
	movea.l	(sp),A1
	sys	WaitIO
	movea.l	(sp)+,a1
	movea.l	(SP)+,A6
\1	rts

lbC001362	tst.b	(8,A1)
	beq.b	\1
	move.l	A6,-(SP)
	move.l	a1,-(sp)
	movea.l	(_exec,A4),A6
	sys	CheckIO
	movea.l	(sp)+,A1
	sys	AbortIO
	movea.l	(SP)+,A6
\1	rts

lbC001386	suba.w	#$18,SP
	movem.l	D4-D7/A2/A3/A5/A6,-(SP)
	movea.l	(_exec,A4),A6
	move.l	($1c,a3),d7	;length
;	move.l	($48,SP),D7
	movea.l	a0,a3		;string
;	movea.l	($44,SP),A3
;	movea.l	($40,SP),A5
	moveq	#0,D6
	movea.l	($98,A5),A0
	move.w	#3,($1C,A0)
	movea.l	($18,A0),A2
;	move.w	($26,A2),D4
;	move.w	($28,A2),($20,SP)
	move.l	($26,A2),D4
	move.w	d4,($20,SP)
	swap	d4
	bra.b	\2

\1	movea.l	($98,A5),A1
	move.l	A3,($28,a1)
	move.l	#$100,($24,A1)
	sys	DoIO
	move.l	D5,D6
	adda.w	#$100,a3
\2	move.l	D6,D5
	addi.l	#$100,D5
	cmp.l	D7,D5
	bcs.b	\1
	movea.l	($98,A5),A1
	move.l	A3,($28,a1)
	move.l	D7,D0
	sub.l	D6,D0
	move.l	D0,D6
	move.l	D6,($24,A1)
	sys	DoIO
	btst	#2,($161,A5)
	beq.b	\4
	move.w	($2C,A2),D0
	move.w	($28,A2),D1
	cmp.w	D0,D1
	bne.b	\4
	moveq	#8,D1
	cmp.w	D1,D0
	blt.b	\4
	moveq	#10,D1
	cmp.b	(-1,A3,D6.L),D1
	bne.b	\4
	tst.w	D4
	bne.b	\4
	move.w	($20,SP),D1
	cmp.w	D0,D1
	bne.b	\4
	ext.l	D0
	bpl.b	\3
	addq.l	#3,D0
\3	asr.l	#2,D0
	move.l	D0,D6
	movea.l	($98,A5),A2
	move.l	D6,d1
	lea	($28-4,SP),a0
	bsr	lbC009AA2
	move.l	D0,($24,A2)
;	movea.l	($98,A5),A2
	lea	($2C-8,SP),A0
;	adda.l	($24,A2),A0
	adda.l	d0,a0
	move.l	D6,d1
	bsr	lbC009B54
	add.l	D0,($24,A2)
;	movea.l	($98,A5),A0
	lea	($24,SP),A1
	move.l	A1,($28,A2)
	movea.l	A2,A1
	sys	DoIO
\4	move.l	D7,D0
	movem.l	(SP)+,D4-D7/A2/A3/A5/A6
	adda.w	#$18,SP
	rts

lbC0014D4	movem.l	D2-d4/a2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	movea.l	($a8,a3),a2
	move.l	(MN_REPLYPORT,a2),d4
	move.l	($94,A3),(MN_REPLYPORT,a2)	msg port
	movea.l	(A5),A6
	tst.w	($104,A3)
	bne	\4
	movea.l	a2,a1
	sys	SendIO
	clr.l	($40,A5)
	moveq	#1,d2
	movea.l	($94,A3),A0
	move.b	(MP_SIGBIT,A0),D1
	lsl.l	d1,d2
	move.l	d2,d3
	or.l	($E0,A3),D2		wbapp/scn signals
	or.l	($7C,A3),D2		window signal
\1	move.l	d2,d0
	sys	Wait
	cmp.l	d0,d3
	beq	\skip
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00BF90		check/process window messages
\skip	movea.l	($94,A3),A0
	sys	GetMsg
	cmp.l	a2,D0
	bne.b	\2
	move.l	(io_Actual,a2),D0
	bra.b	\5

\2	movea.l	A3,a1
	bsr	lbC00C90E
;	bne.b	\3
;	tst.w	($104,A3)
	beq.b	\1
\3	movea.l	a2,a1
	bsr	lbC001338
\4	moveq	#0,D0
\5	move.l	d4,(MN_REPLYPORT,a2)	msg port
	movem.l	(SP)+,D2-d4/a2/A3/A5/A6
	rts

lbC001596	movem.l	A5/A6,-(SP)
	movea.l	a5,a6
	movea.l	a0,a5
	btst	#1,($162,A5)
	bne.b	\1
	tst.w	($108,A5)
	beq.b	\1
	moveq	#1,d1
	move.l	($160,A5),d0
	suba.w	#$20,SP
	movea.l	sp,a0
	bsr	lbC009BA2
	move.l	A5,a1
	move.l	a6,d1
	bsr	_doio
	adda.w	#$20,SP	
	clr.w	($108,A5)
\1	movea.l	($A8,A5),A1
	movea.l	(_exec,A4),A6
	sys	SendIO
	movem.l	(SP)+,A5/A6
	rts

lbC0016EE	moveq	#0,D0
	move.b	($60,a2),D0
	lea	(A0,D0.W*4),A1
	clr.l	($AC,A1)
	movea.l	a2,a1

lbC0015EE	bsr	lbC001338
	move.l	a6,-(sp)
	movea.l	(_exec,A4),A6
	sys	FreeVec
	movea.l	(SP)+,A6
	rts

lbC00160E	movem.l	d1/a0/a1,-(sp)
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	sys	AllocVec
	movem.l	(SP)+,d1/a0/a1
	tst.l	D0
	beq	\1
	exg	d0,a1
	exg	d1,a0
	move.l	(io_Device,a0),(io_Device,A1)	prepare packet ioreq
	move.l	(io_Unit,a0),(io_Unit,a1)
	movea.l	d0,a0
	movea.l	($30,a0),A0
	adda.w	#pr_MsgPort,A0
	move.l	A0,(MN_REPLYPORT,A1)	process
	move.l	a0,d0
	lea	(IOSTD_SIZE,A1),A0	packet
	move.l	A0,(LN_NAME,A1)
	move.l	A1,(A0)+
	move.l	d0,(A0)+	
	move.l	d1,(A0)		packet type
\1	rts

lbC00170E	movem.l	A3/A5,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	moveq	#0,d1
	bsr	lbC00BA92
	moveq	#0,d1
	bsr	lbC00C786
	moveq	#1,d0
	movea.l	($78,A3),a0
	bsr	lbC00D666
	movem.l	(SP)+,A3/A5
	rts

lbC001746	movem.l	A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	moveq	#0,d0
	movea.l	($78,A3),a0
	bsr	lbC00D666
	moveq	#1,d1
	bsr	lbC00C786
	moveq	#1,d1
	bsr	lbC00BA92
	movea.l	($98,A3),A1
	move.w	#CMD_CLEAR,($1C,A1)
	movea.l	(_exec,A4),A6
	sys	DoIO
	btst	#2,($162,A3)
	bne.b	\1
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AF9E
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
\1	movem.l	(SP)+,A3/A5/A6
	rts

lbC0017B4	movem.l	D2/D3/A2/A3/A6,-(SP)
	movea.l	a0,A3
	movea.l	($88,A3),A0
	movea.l	([$30,A0],8),A2
	move.w	($48,A0),D0
	neg.w	D0
	ext.l	D0
	moveq	#1,D3
	move.w	D3,D1
	sub.w	($4A,A0),D1
	ext.l	D1
	move.w	($20,A2),D2
	sub.w	($1C,A2),D2
	addq.w	#1,D2
	ext.l	D2
	move.w	($22,A2),D3
	sub.w	($1E,A2),D3
	ext.l	D3
	movea.l	($78,A3),A0
	movea.l	(8,A1),A6
	sys	ChangeWindowBox
	movea.l	($78,A3),A0
	sys	WindowToFront
	movem.l	(SP)+,D2/D3/A2/A3/A6
	rts

lbC00181E	movem.l	D2-D7/A2/A6,-(SP)
	movea.l	a0,A2
	movea.l	a1,a6
	moveq	#-1,d3
	movea.l	($88,A2),A0
	movea.l	([$30,A0],8),A1
	move.w	($124,A2),D0
	cmp.w	d3,D0
	bne.b	\1
	move.w	($48,A0),D6
	ext.l	D6
	neg.l	D6
	bra.b	\2

\1	moveq	#0,D6
	move.w	D0,D6
\2	move.w	($126,A2),D0
	cmp.w	d3,D0
	bne.b	\3
	move.w	($4A,A0),D5
	ext.l	D5
	neg.l	D5
	move.b	($1E,A0),D7
	extb.l	D7
	addq.l	#1,D7
	cmp.l	D7,D5
	bgt.b	\4
	move.l	D7,D5
	bra.b	\4

\3	moveq	#0,D5
	move.w	D0,D5
\4	move.w	($128,A2),D0
	cmp.w	d3,D0
	bne.b	\5
	move.w	($1C,A1),D1
	ext.l	D1
	move.w	($20,A1),D4
	ext.l	D4
	sub.l	D1,D4
	addq.l	#1,D4
	bra.b	\6

\5	moveq	#0,D4
	move.w	D0,D4
\6	move.w	($12A,A2),D0
	cmp.w	d3,D0
	bne.b	\8
	move.w	($1E,A1),D1
	ext.l	D1
	move.w	($22,A1),D3
	ext.l	D3
	sub.l	D1,D3
	bpl.b	\7
	addq.l	#1,D3
\7	asr.l	#1,D3
	bra.b	\9

\8	moveq	#0,D3
	move.w	D0,D3
\9	move.w	D6,D0
	ext.l	D0
	move.w	D5,D1
	ext.l	D1
	move.w	D4,D2
	ext.l	D3
	ext.l	D2
	movea.l	($78,A2),A0
	movea.l	(8,A6),A6
	sys	ChangeWindowBox
	movea.l	($78,A2),A0
	sys	WindowToFront
	movem.l	(SP)+,D2-D7/A2/A6
	rts

lbC0018FA	movem.l	D2/D3/A6,-(SP)
	movea.l	($78,a3),A0
	movem.w	(4,A0),D0/d1
	ext.l	D0
	ext.l	D1
	movea.l	(8,A5),A6
	moveq	#$68,D2
	moveq	#$32,D3
	sys	ChangeWindowBox
	movem.l	(SP)+,D2/D3/A6
	rts

lbC001954	movem.l	A2/A5/A6,-(SP)
	movea.l	a0,A2
	movea.l	a1,A5
	movea.l	A2,a0
	bsr	lbC0094C2
	tst.l	D0
	beq.b	\1
	movea.l	d0,A0
	move.l	(pr_CurrentDir,A0),D0
	cmp.l	($E4,A2),D0
	beq.b	\1
	movea.l	($78,A2),A0
	lea	($184,A2),A1
	movea.l	($20,A0),A0
	cmpa.l	A1,A0
	bne.b	\1
	moveq	#1,d0
	move.l	($16C,A2),d1
	move.l	A2,a0
	move.l	A5,a1
	bsr	lbC00D394
	lea	($184,A2),A1
	movea.l	($78,A2),A0
	movea.w	#$FFFF,A2
	movea.l	(8,A5),A6
	sys	SetWindowTitles
\1	movem.l	(SP)+,A2/A5/A6
	rts

_OpenWindow	suba.w	#$74,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	moveq	#-1,d6
	movea.l	a0,A3
	movea.l	a1,A5
	move.b	d0,($34,sp)
	moveq	#1,D0
	move.l	D0,($8C,SP)
	clr.l	($52,SP)
;	clr.l	($98,sp)	our font backup
	moveq	#$67,D0
	move.l	D0,($40,A5)
	move.l	($168,A3),d0
	beq	\7

\1	movem.l	A2/A3,-(SP)
	movea.l	(A5),A6
	move.l	D0,a1
	movea.l	(8,a5),A0	intuition
	sys	Forbid
	move.l	(ib_FirstScreen,A0),d0
	bra.b	\4

\2	move.l	(wd_NextWindow,A2),d0
\3	beq	.nw
	movea.l	d0,a2
	cmpa.l	A1,A2
	bne.b	\2
	move.l	A1,D0
	bra	\5

.nw	move.l	(sc_NextScreen,A3),d0
\4	beq	\5
	movea.l	d0,a3
	move.l	(sc_FirstWindow,A3),d0
	bra.b	\3

\5	sys	Permit
\6	movem.l	(SP)+,A2/A3
	move.l	D0,($168,A3)
	beq	\7
	move.l	(wd_WScreen,A1),($88,A3)
	bset	#4,($161,A3)	NOGADS
	tst.l	(wd_MenuStrip,A1)
	beq	\19
	bset	#5,($162,A3)	NOMENUS
	bra	\19

\7	move.b	($15FC,A3),D0
	beq.b	\15
	moveq	#$2A,D1
	cmp.b	D1,D0
	bne.b	\15
	tst.b	($15FD,A3)
	bne.b	\15
\8	movem.l	A2/A3,-(SP)
	suba.l	A3,A3
	movea.l	(A5),A6
	sys	Forbid
	movea.l	(8,a5),A6
	movea.l	(ib_FirstScreen,A6),A2
	sys	LockPubScreenList
	movea.l	D0,A0
	movea.l	(A0),A1
	bra.b	\11

\9	movea.l	(14,A1),A0
	cmpa.l	A2,A0
	bne.b	\10
	movea.l	(10,A1),A3
\10	movea.l	(A1),A1
\11	tst.l	(A1)
	beq.b	\12
	move.l	A3,D0
	beq.b	\9
\12	sys	UnlockPubScreenList
	move.l	A3,D0
	beq.b	\13
	movea.l	A3,A0
	sys	LockPubScreen
\13	movea.l	(A5),A6
	sys	Permit
	movem.l	(SP)+,A2/A3
	tst.l	d0
	bne.b	\18
	bra.b	\14

\15	tst.b	D0
	bne.b	\16
	suba.l	a0,a0
	bra.b	\17

\16	lea	($15FC,A3),A0
\17	movea.l	(8,A5),A6
	sys	LockPubScreen
	tst.l	d0
	bne	\18
\14	lea	(Workbench.MSG,PC),A0
	movea.l	(8,A5),A6
	sys	LockPubScreen
\18	move.l	D0,($88,A3)
\19	move.l	($88,A3),d0
	beq	\72
	movea.l	d0,a2
	move.l	($8C,A3),d4
	move.l	($174,A3),D0
	beq	\20
	lea	($80,SP),A1
	movea.l	a1,a0
	move.l	D0,(A0)+	font name
	move.w	($17C,A3),(A0)+	its YSize
	move.w	#2,(a0)		Style+flags = diskfont
	movea.l	A5,a0
	bsr	_OpenDiskFont
	move.l	D0,d4
	beq.b	\20
	movea.l	D4,A1
	btst	#FPB_PROPORTIONAL,(tf_Flags,A1)
	beq	\24
	movea.l	(16,A5),A6
	sys	CloseFont
	moveq	#0,d4
\20	btst	#0,(sc_Flags+1,A2)	workbench screen?
	bne.b	\23
	movea.l	(sc_Font,A2),a1
	move.l	A5,a0
	bsr	_OpenDiskFont
	move.l	D0,d4
\21	tst.l	d4
	beq.b	\23
\22	movea.l	D4,a1
	btst	#FPB_PROPORTIONAL,(tf_Flags,A1)
	beq.b	\24
	movea.l	(16,A5),A6
	sys	CloseFont
	moveq	#0,d4
\23	movea.l	([16,A5],gb_DefaultFont),A0
	lea	($80,SP),A6
	movea.l	a6,a1
	move.l	(LN_NAME,A0),(a6)+
	move.l	(tf_YSize,A0),(a6)
	move.l	A5,a0
	bsr	_OpenDiskFont
	move.l	D0,d4
\24	move.l	d4,($8c,a3)
	movea.l	a2,A0
	movea.l	(12,A5),A6
	suba.l	A1,A1
	sys	GetVisualInfoA
	move.l	D0,($80,A3)
	beq	\72
	bsr	lbC00A672
	move.l	D0,($52,SP)
	bne.b	\25
	move.l	($90,A3),d0
	beq	\25
	btst	#4,($161,A3)	NOGADS?
	beq	\72
\25	movea.l	([sc_ViewPort+vp_ColorMap,A2],cm_vpe),A1
	move.w	($13C,A3),D0
	cmp.w	d6,D0
	bne	\34
	move.w	($124,A3),D1
	cmp.w	d6,D1
	bne.b	\26
	move.w	($48,A2),D2
	ext.l	D2
	neg.l	D2
	bra.b	\27

\26	moveq	#0,D2
	move.w	D1,D2
\27	move.w	D2,($5A,SP)
	move.w	($126,A3),D1
	cmp.w	d6,D1
	bne.b	\28
	move.w	($4A,A2),D2
	ext.l	D2
	neg.l	D2
	move.b	($1E,A2),D7
	extb.l	D7
	addq.l	#1,D7
	cmp.l	D7,D2
	bgt.b	\29
	move.l	D7,D2
	bra.b	\29

\28	moveq	#0,D2
	move.w	D1,D2
\29	movea.l	A1,A2
	move.w	D2,($58,SP)
	move.w	($128,A3),D1
	cmp.w	d6,D1
	bne.b	\30
	move.w	($1C,A2),D2
	ext.l	D2
	move.w	($20,A2),D3
	ext.l	D3
	sub.l	D2,D3
	addq.l	#1,D3
	bra.b	\31

\30	moveq	#0,D2
	move.w	D1,D2
	move.l	D2,D3
\31	move.w	D3,($7C,SP)
	move.w	($12A,A3),D1
	cmp.w	d6,D1
	bne.b	\33
	move.w	($1E,A2),D2
	ext.l	D2
	move.w	($22,A2),D3
	ext.l	D3
	tst.b	(_boot,a4)	is this a bootshell?
	bne	\35
	sub.l	D2,D3
	bpl.b	\32
	addq.l	#1,D3
\32	asr.l	#1,D3
	bra.b	\35

\33	move.w	D1,D3
	bra.b	\35

\34	lea	($13E,A3),A6
	movem.w	(a6)+,d1-d3
;	move.w	(A6)+,D1
;	move.w	(A6)+,D2
;	move.w	(A6)+,D3
	move.w	D0,($5A,SP)
	move.w	D1,($58,SP)
	move.w	D2,($7C,SP)
\35	move.w	D3,($56,SP)
	btst	#0,($162,A3)	MINI?
	beq.b	\44
	move.w	($12C,A3),D0
	cmp.w	d6,D0
	bne.b	\36
	move.w	($5A,SP),D1
	ext.l	D1
	bra.b	\37

\36	move.w	D0,D1
\37	move.w	D1,($14C,A3)
	move.w	($12E,A3),D0
	cmp.w	d6,D0
	bne.b	\38
	move.w	($58,SP),D1
	ext.l	D1
	bra.b	\39

\38	move.w	D0,D1
\39	move.w	D1,($14E,A3)
	move.w	($130,A3),D0
	cmp.w	d6,D0
	bne.b	\40
	moveq	#$50,D1
	bra.b	\41

\40	move.w	D0,D1
\41	move.w	D1,($150,A3)
	move.w	($132,A3),D0
	cmp.w	d6,D0
	bne.b	\42
	moveq	#$32,D1
	bra.b	\43

\42	move.w	D0,D1
\43	move.w	D1,($152,A3)
	bra.b	\53

\44	move.w	($12C,A3),D0
	cmp.w	d6,D0
	bne.b	\45
	moveq	#0,D1
	bra.b	\46

\45	move.w	D0,D1
\46	move.w	D1,($14C,A3)
	move.w	($12E,A3),D0
	cmp.w	d6,D0
	bne.b	\47
	moveq	#0,D1
	bra.b	\48

\47	move.w	D0,D1
\48	move.w	D1,($14E,A3)
	move.w	($130,A3),D0
	cmp.w	d6,D0
	bne.b	\49
	move.w	($1C,A1),D1
	ext.l	D1
	move.w	($20,A1),D2
	ext.l	D2
	sub.l	D1,D2
	addq.l	#1,D2
	bra.b	\50

\49	move.w	D0,D2
\50	move.w	D2,($150,A3)
	move.w	($132,A3),D0
	cmp.w	d6,D0
	bne.b	\51
	move.w	($1E,A1),D1
	ext.l	D1
	move.w	($22,A1),D2
	ext.l	D2
	sub.l	D1,D2
	addq.l	#1,D2
	bra.b	\52

\51	move.w	D0,D2
\52	move.w	D2,($152,A3)
\53	moveq	#1,d0
	move.l	($16C,A3),d1
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00D394
	move.l	($168,A3),D0
	beq.b	\54
	move.l	D0,($78,A3)
	st	($100,A3)
	clr.l	($7C,A3)
	bra	\64

\54	move.w	($5A,SP),D6
	ext.l	D6
	move.w	($58,SP),D5
	ext.l	D5
	move.w	($7C,SP),D0
	ext.l	D0
	ext.l	D3
	move.l	($160,a3),D1
	btst	#5,D1		NOSIZE?
	beq.b	\55
	moveq	#1,D2
	bra.b	\56

\55	move.l	#WA_Zoom,D2
\56	move.l	D2,D4
	btst	#6,D1		SIMPLE?
	beq.b	\57
	move.l	#WA_SimpleRefresh,D2
	bra.b	\58

\57	move.l	#WA_SmartRefresh,D2
\58	move.l	D2,D7
	movea.l	($88,A3),A2
	moveq	#WBENCHSCREEN|PUBLICSCREEN,D2
	and.w	(sc_Flags,A2),D2
	beq.b	\59
	move.l	#WA_PubScreen,D2
	bra.b	\60

\59	move.l	#WA_CustomScreen,D2
\60	lea	($36,SP),A1
	move.l	D2,(A1)+
	btst	#13,D1		NOMENUS?
	seq	D2
	neg.b	D2
	extb.l	D2
	move.l	D2,(A1)+
	btst	#17,d1		INACTIVE?
	seq	D2
	neg.b	D2
	extb.l	D2
	moveq	#8,D1
	lea	($162,a3),A0
	and.w	(A0),D1
	move.l	D2,(A1)+
	moveq	#0,D2
	move.w	D1,D2
	moveq	#4,D1
	and.w	(A0),D1
	lea	($46,SP),A1
	move.l	D2,(A1)+
	moveq	#0,D2
	move.w	D1,D2
	moveq	#2,D1
	and.w	(A0)+,D1
	move.l	D2,(A1)+
	moveq	#0,D2
	move.w	D1,D2
	move.l	D2,(A1)+
	btst	#4,($163,a3)	NODRAG?
	seq	D1
	neg.b	D1
	extb.l	D1
	btst	#5,($163,a3)	NOSIZE?
	seq	D2
	neg.b	D2
	extb.l	D2
	clr.l	-(SP)
	move.l	A2,-(SP)
	move.l	($3E,SP),-(SP)
	move.l	($46,SP),-(SP)
	move.l	#WA_NewLookMenus,-(SP)
;	pea	(1).W
;	move.l	#WA_AutoAdjust,-(SP)
	pea	(1).W
	move.l	D7,-(SP)
	move.l	($62-8,SP),-(SP)
	move.l	#WA_Activate,-(SP)
	pea	($14C,A3)
	move.l	D4,-(SP)		WA_Zoom
	move.l	($86-8,SP),-(SP)
	move.l	#WA_Gadgets,-(SP)
	move.l	($82-8,SP),-(SP)
	move.l	#WA_Borderless,-(SP)
	move.l	($8E-8,SP),-(SP)
	move.l	#WA_Backdrop,-(SP)
	move.l	($9A-8,SP),-(SP)
	move.l	#WA_CloseGadget,-(SP)
	pea	(1).W
	move.l	#WA_DepthGadget,-(SP)
	move.l	D1,-(SP)
	move.l	#WA_DragBar,-(SP)
	move.l	D2,-(SP)
	move.l	#WA_SizeGadget,-(SP)
	pea	($184,A3)
	move.l	#WA_Title,-(SP)
	moveq	#-1,d1
	move.l	d1,-(sp)
	move.l	#WA_MaxHeight,-(SP)
	move.l	d1,-(sp)
	move.l	#WA_MaxWidth,-(SP)
	move.l	D0,-(SP)
	move.l	#WA_Width,-(SP)
	move.l	($90+132,sp),d0
	move.l	d0,-(sp)
	move.l	#WA_MinHeight,-(SP)
	pea	($68).W
	move.l	#WA_MinWidth,-(SP)
	cmp.l	d0,d3
	bge	1$
	move.l	d0,d3
1$	move.l	D3,-(SP)
	move.l	#WA_Height,-(SP)
	move.l	D5,-(SP)
	move.l	#WA_Top,-(SP)
	move.l	D6,-(SP)
	move.l	#WA_Left,-(SP)
	move.l	#IDCMP_IDCMPUPDATE!IDCMP_GADGETUP,-(SP)
	move.l	#WA_IDCMP,-(SP)

	ifd	MorphOS
	pea	($65).w
	move.l	#WA_ExtraGadgetsStartID,-(sp)
	pea	(ETG_ICONIFY).w
	move.l	#WA_ExtraTitlebarGadgets,-(sp)
	endc

	movea.l	(8,A5),A6
	suba.l	A0,A0
	movea.l	SP,A1
	sys	OpenWindowTagList

	ifd	MorphOS
	lea	($BC-8+16,SP),SP
	else
	lea	($BC-8,SP),SP
	endc

	move.l	D0,($78,A3)
	beq	\72

	movea.l	a5,a0
	moveq	#23,d0		Open locale.lib/OpenCatalogA()
	bsr	_DoSyncDos

	move.l	($8C,A3),D1
	beq.b	\61
	movea.l	D1,A0
	movea.l	($78,A3),A1
	movea.l	(wd_RPort,A1),A1
	movea.l	(16,A5),A6
	sys	SetFont
\61	btst	#0,($160,A3)		NOICONIFY?
	beq.b	\62
	btst	#6,($160,A3)		MENUFY?
	beq.b	\63
\62	btst	#4,($161,A3)		NOGADS?
	bne.b	\63

	ifd	MorphOS
	movea.l	(a5),a6
	lea	(_morph,pc),a1
	sys	Forbid
	sys	FindResident
	sys	Permit
	tst.l	d0
	bne	\63
	endc

	bsr	lbC00D17A
	beq	\72
\63	movea.l	([$78,A3],wd_UserPort),A1
	moveq	#1,D0
	move.b	(MP_SIGBIT,A1),D1
	lsl.l	D1,d0
	move.l	D0,($7C,A3)
\64	movea.l	(8,A5),A6
	movea.l	($88,a3),A2
	movea.l	A2,A1
	suba.l	A0,A0
	sys	UnlockPubScreen
	move.l	(20,a5),d2	wblib
	beq	\nowb
	bsr	_remAppIcon
;	bsr	_remAppMenuItem

	ifd	scn
	moveq	#WBENCHSCREEN,D0
	and.w	(sc_Flags,A2),D0
	beq	\ta
	tst.l	(_client,a4)
	bne	\na
	move.l	(32,a5),d0
	bne	\is
	lea	(scnlib,pc),a1
	moveq	#1,d0
	movea.l	(a5),a6
	sys	OpenLibrary
	move.l	d0,(32,a5)
	beq	\na
\is	movea.l	d0,a6
	moveq	#-128,d0		priority
	movea.l	($cc,a3),a0
	sys	AddWorkbenchClient
	move.l	d0,(_client,a4)
\na	endc

\ta	movea.l	d2,A6
	movea.l	($78,A3),A0		window
	moveq	#0,D0
	movea.l	($CC,A3),A1
	move.l	D0,D1
	move.l	d0,A2
	sys	AddAppWindowA
	move.l	D0,($D0,A3)
;	bne	\nowb
;	movea.l	(4,a5),a6
;	moveq	#5,d1
;	sys	Delay
;	bra	\ta

\nowb	btst	#3,($160,A3)	POPSCREEN?
	beq.b	\67
	movea.l	($88,A3),A0	screen
	movea.l	(8,A5),A6
	sys	ScreenToFront
\67	btst	#5,($162,A3)	NOMENUS?
	bne.b	\68
	bsr	lbC00C12E
	tst.l	D0
	beq	\72
\68	movea.l	(a5),A6
	sys	CreateMsgPort
	move.l	D0,($94,A3)
	beq	\72
	movea.l	D0,A0
	moveq	#$30,D0
	sys	CreateIORequest
	move.l	D0,($98,A3)
	beq	\72
	movea.l	D0,A1
	move.l	($78,A3),(io_Data,A1)	window ptr
	move.l	#wd_SIZEOF,(io_Length,A1)
	moveq	#CONU_STANDARD,D0
	btst	#6,($163,a3)		SIMPLE?
	beq	.0
	moveq	#CONU_SNIPMAP,D0
.0	lea	(consoledevice.MSG,PC),A0
	moveq	#0,D1
	sys	OpenDevice
	move.l	D0,($8C,SP)
	bne	\72
	movea.l	($94,A3),A0
	moveq	#$30,D0
	sys	CreateIORequest
	move.l	D0,($9C,A3)
	beq	\72
	movea.l	D0,A1
	lea	(consoledevice.MSG,PC),A0
	moveq	#CONU_LIBRARY,D0
	moveq	#0,D1
	sys	OpenDevice
	tst.l	D0
	bne	\72
	movea.l	($98,A3),A0	ior
	clr.b	(LN_TYPE,A0)
	move.l	A0,d1
	lea	(ACTION_READ_RETURN).W,a0
	move.l	A5,a1
	move.l	#IOSTD_SIZE+dp_SIZEOF,D0
	bsr	lbC00160E
	move.l	a1,($A8,A3)
	beq	\72
	moveq	#-1,D1
	lea	($134,A3),A0
	move.l	D1,(A0)+
	move.l	D1,(A0)
	move.w	#CMD_READ,(io_Command,A1)
	lea	($1FC,A3),A0
	move.l	A0,(io_Data,A1)
	move.l	#$400,(io_Length,A1)
	lea	($32,A3),A0
	move.l	A0,($60,A3)
	moveq	#1,d0
	move.w	d0,($108,A3)
	lea	(-48,sp),sp
	movea.l	sp,a1
	tst.b	(_asdss,a4)
	beq	\n
	lea	(_sgr,a4),a0
	bsr	sgr2ascii
	move.b	#$9b,(a0)+	CSI
	move.b	#32,(a0)+	space
	move.b	#'s',(a0)+	aSDSS
	movea.l	a0,a1
\n	move.l	($90,a3),d0
	lea	(_sgr,a4),a0
	beq	\z
	movea.l	d0,a0
	lea	($74,a0),a0
\z	bsr	sgr2ascii
	subq.l	#1,a0
	move.b	(a0),d1		save 'm'
	move.b	#';',(a0)+
	move.b	#'>',(a0)+
	moveq	#'0',d0
	add.b	(_sgr+3,a4),d0	background colour
	move.b	d0,(a0)+
	move.b	d1,(a0)+	restore m
	suba.l	sp,a0
	move.l	a0,d0		size
	move.l	sp,a0		buffer
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	lea	(48,sp),sp

	ifd	rom
	tst.b	(_boot,a4)	is this a bootshell?
	beq	\skip
	bsr	_copyright	
\skip	endc

	btst	#1,($162,A3)	history disabled?
	beq.b	\69
	moveq	#1,d1
	move.l	#$FFF7FFFF,D0
	and.l	($160,A3),D0
	lea	($5c,SP),a0
	bsr	lbC009BA2
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	bra.b	\70

\69	tst.b	($34,SP)
	bne.b	.71
	moveq	#1,d1
	move.l	($160,A3),d0
	lea	($5c,SP),a0
	bsr	lbC009BA2
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	clr.w	($108,A3)
\70	tst.b	($34,SP)
	beq.b	\71
.71	move.l	A3,a0
	bsr	lbC001596
\71	bsr	lbC00BE64
	movea.l	($78,A3),a1
	movea.l	A5,a0
	bsr	_FindTask
	clr.l	($40,A5)
	moveq	#1,D0
	bra	\85

\72	move.l	($A8,A3),d0
	beq.b	\73
	move.l	d0,a1
	bsr	lbC0015EE
\73	move.l	($9C,A3),d0
	beq.b	\75
	movea.l	d0,A1
	tst.l	(20,A1)
	beq.b	\74
	movea.l	(A5),A6
	sys	CloseDevice
\74	movea.l	($9C,A3),A0
	movea.l	(A5),A6
	sys	DeleteIORequest
\75	tst.l	($8C,SP)
	bne.b	\76
	movea.l	($98,A3),A1
	movea.l	(A5),A6
	sys	CloseDevice
\76	movea.l	($98,A3),A0
	move.l	A0,D0
	beq.b	\77
	movea.l	(A5),A6
	sys	DeleteIORequest
\77	move.l	($94,A3),D0
	beq.b	\78
	movea.l	D0,A0
	movea.l	(a5),A6
	sys	DeleteMsgPort
\78	bsr	lbC00C8E8
	move.l	($84,A3),d0
	beq.b	\79
	bsr	lbC00D346
\79	move.l	(_cat,a4),d0
	beq	.79
	movea.l	d0,a0
	movea.l	(_locb,a4),a6	locale base
	sys	CloseCatalog
.79	tst.l	($78,A3)
	beq.b	\80
	bsr	lbC000E90
	movea.l	($78,A3),A0
	movea.l	(8,A5),A6
	sys	CloseWindow
\80	bsr	lbC00A468
	move.l	($80,A3),D0
	beq.b	\82
	movea.l	D0,A0
	movea.l	(12,A5),A6
	sys	FreeVisualInfo
\82	move.l	($8C,A3),d0
	beq.b	\83
	movea.l	d0,a1
	movea.l	(16,A5),A6
	sys	CloseFont
\83	move.l	($88,a3),d0
	beq.b	\84
	movea.l	d0,A1
	movea.l	(8,A5),A6
	suba.l	A0,A0
	sys	UnlockPubScreen
\84	movea.l	A3,a0
	bsr	lbC001928
	moveq	#0,D0
\85	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$74,SP
	rts


lbC002210	move.l	(a3),d0
	beq	\5
	movem.l	D7/A2/A3/A5,-(SP)
	movea.l	d0,a5
	movea.l	(4,a3),A3
	moveq	#0,D7
	lea	(\tbl,pc),A2
\1	movea.l	(A2),a0
	movea.l	A3,a1
	move.l	A5,d0
	bsr	lbC0012F6
	beq.b	\2
	move.l	A2,D0
	bra.b	\4

\2	addq.l	#1,D7
	adda.w	#10,A2
	moveq	#(\tbls/10),D0
	cmp.l	D0,D7
	bcs.b	\1
\3	moveq	#0,D0
\4	movem.l	(SP)+,D7/A2/A3/A5
\5	rts

\tbl	dc.l	AUTOICONIFY.MSG
	dc.b	1,0
	dc.l	$800003
	dc.l	AUTOICON.MSG
	dc.b	1,0
	dc.l	$800003
	dc.l	AUTO.MSG
	dc.b	1,0
	dc.l	3
	dc.l	NOAUTO.MSG
	dc.b	2,0
	dc.l	$800001
	dc.l	CLOSE.MSG
	dc.b	1,0
	dc.l	2
	dc.l	NOCLOSE.MSG
	dc.b	2,0
	dc.l	2
	dc.l	BACKDROP.MSG
	dc.b	1,0
	dc.l	4
	dc.l	NOBACKDROP.MSG
	dc.b	2,0
	dc.l	4
	dc.l	NOBORDER.MSG
	dc.b	1,0
	dc.l	8
	dc.l	BORDER.MSG
	dc.b	2,0
	dc.l	8
	dc.l	NODRAG.MSG
	dc.b	1,0
	dc.l	$10
	dc.l	DRAG.MSG
	dc.b	2,0
	dc.l	$10
	dc.l	NOSIZE.MSG
	dc.b	1,0
	dc.l	$20
	dc.l	SIZE.MSG
	dc.b	2,0
	dc.l	$20
	dc.l	SIMPLE.MSG
	dc.b	1,0
	dc.l	$40
	dc.l	SMART.MSG
	dc.b	2,0
	dc.l	$40
	dc.l	WAIT.MSG
	dc.b	1,0
	dc.l	$80
	dc.l	NOWAIT.MSG
	dc.b	2,0
	dc.l	$80
	dc.l	MINI.MSG
	dc.b	1,0
	dc.l	$100
	dc.l	MAXI.MSG
	dc.b	2,0
	dc.l	$100
	dc.l	NOREVIEW.MSG
	dc.b	1,0
	dc.l	$400
	dc.l	REVIEW.MSG
	dc.b	2,0
	dc.l	$400
	dc.l	FNCMODE.MSG
	dc.b	5,$6B
	dc.l	0
	dc.l	NOFNC.MSG
	dc.b	1,0
	dc.l	$1000
	dc.l	FNC.MSG
	dc.b	2,0
	dc.l	$1000
	dc.l	NOSHORTCUTS.MSG
	dc.b	3,0
	dc.l	4
	dc.l	SHORTCUTS.MSG
	dc.b	4,0
	dc.l	4
	dc.l	NOMENUS.MSG
	dc.b	1,0
	dc.l	$2000
	dc.l	MENUS.MSG
	dc.b	2,0
	dc.l	$2000
	dc.l	PLAIN.MSG
	dc.b	1,0
	dc.l	$103400
	dc.l	NOSTYLES.MSG
	dc.b	1,0
	dc.l	$10000
	dc.l	STYLES.MSG
	dc.b	2,0
	dc.l	$10000
	dc.l	INACTIVE.MSG
	dc.b	1,0
	dc.l	$20000
	dc.l	ACTIVE.MSG
	dc.b	2,0
	dc.l	$20000
	dc.l	NOGADS.MSG
	dc.b	1,0
	dc.l	$100000
	dc.l	GADS.MSG
	dc.b	2,0
	dc.l	$100000
	dc.l	JUMP.MSG
	dc.b	1,0
	dc.l	$40000
	dc.l	NOJUMP.MSG
	dc.b	2,0
	dc.l	$40000
	dc.l	ASYNC.MSG
	dc.b	1,0
	dc.l	$200000
	dc.l	SYNC.MSG
	dc.b	2,0
	dc.l	$200000
	dc.l	SHOWDIR.MSG
	dc.b	1,0
	dc.l	$400000
	dc.l	NOSHOWDIR.MSG
	dc.b	2,0
	dc.l	$400000
	dc.l	MENUFY.MSG
	dc.b	1,0
	dc.l	$40000000
	dc.l	NOMENUFY.MSG
	dc.b	2,0
	dc.l	$40000000
	dc.l	ICONTITLE.MSG
	dc.b	5,$6D
	dc.l	0
	dc.l	ICONPOS.MSG
	dc.b	5,$6C
	dc.l	0
	dc.l	NOICON.MSG
	dc.b	1,0
	dc.l	$1000000
	dc.l	NOICONIFY.MSG
	dc.b	1,0
	dc.l	$1000000
	dc.l	ICONIFY.MSG
	dc.b	2,0
	dc.l	$1000000
	dc.l	SCREEN.MSG
	dc.b	5,$64
	dc.l	0
	dc.l	FONT.MSG
	dc.b	5,$65
	dc.l	0
	dc.l	WINDOW.MSG
	dc.b	5,$66
	dc.l	0
	dc.l	ALT.MSG
	dc.b	5,$67
	dc.l	0
	dc.l	MAXBUF.MSG
	dc.b	5,$68
	dc.l	0
	dc.l	PREFS.MSG
	dc.b	5,$69
	dc.l	0
	dc.l	IMAGE.MSG
	dc.b	5,$6A
	dc.l	0
	dc.l	KEEPCLOSED.MSG
	dc.b	1,0
	dc.l	$4000000
	dc.l	NOKEEPCLOSED.MSG
	dc.b	2,0
	dc.l	$4000000
	dc.l	POPSCREEN.MSG
	dc.b	1,0
	dc.l	$8000000
	dc.l	NOPOPSCREEN.MSG
	dc.b	2,0
	dc.l	$8000000
	dc.l	FASTUPDATE.MSG
	dc.b	1,0
	dc.l	$20000000
	dc.l	NICEUPDATE.MSG
	dc.b	2,0
	dc.l	$20000000
	dc.l	DIRCACHE.MSG
	dc.b	5,$6E
	dc.l	0
	dc.l	NODIRCACHE.MSG
	dc.b	4,0
	dc.l	3
	dc.l	SGR.MSG
	dc.b	5,$6F
	dc.l	0
\tbls=*-\tbl

lbC0022EA	movea.l	(a2),a0
	bsr	_strlen
	move.l	D0,D1
	movea.l	(A3),A0
	movea.l	A0,A1
	adda.l	D0,A1
	moveq	#$20,D0
	cmp.b	(A1),D0
	bne.b	\1
	move.l	D1,D0
	addq.l	#1,D0
	bra.b	\2

\1	move.l	D1,D0
\2	movea.l	A0,A1
	adda.l	D0,A1
	move.l	a1,($494,SP)
	move.l	(4,A3),D1
	sub.l	(A3),D1
	sub.l	D0,D1
	addq.l	#1,D1
	move.l	D1,D0
	rts

lbC00239C	suba.w	#$474,SP
	movem.l	D2/D4-D7/A2/A3/A5,-(SP)
	movea.l	($49C,SP),A5
	moveq	#1,D7
	lea	($22,SP),a1
	move.l	($4A4,SP),d0
	move.l	($4A0,SP),d1
lbC002250	movem.l	D2-D4/D7/A2/A3/A5,-(SP)
	move.l	d0,D7
	move.l	d1,d4
	moveq	#0,D6
	moveq	#0,D1
	movea.l	A1,a5
	suba.l	A0,A0
lbC002264	move.l	A0,(A1)+
	move.l	A0,(A1)+
	addq.l	#1,D1
	moveq	#$64,D0
	cmp.l	D0,D1
	blt.b	lbC002264
	moveq	#0,D5
	movea.l	A5,A2
	movea.l	a5,a1
	bra.b	lbC0022DE

lbC00227C	movea.l	d4,A0
	lea	(a0,d5.l),a3
	move.b	(A3),D0
	moveq	#$3A,D1
	cmp.b	D1,D0
	bne.b	lbC0022A4
	tst.l	D6
	bne.b	lbC0022A4
	move.l	A0,(A1)
	movea.l	A3,A0
	subq.l	#1,A0
	move.l	A0,(4,A1)
	lea	(1,A3),A5
	bra.b	lbC0022D8

lbC0022A4	moveq	#$2F,D1
	cmp.b	D1,D0
	beq.b	lbC0022B8
	moveq	#10,D2
	cmp.b	D2,D0
	beq.b	lbC0022B8
	move.l	D7,D3
	subq.l	#1,D3
	cmp.l	D5,D3
	bne.b	lbC0022DC
lbC0022B8	cmp.b	D1,D0
	beq.b	lbC0022C2
	moveq	#10,D1
	cmp.b	D1,D0
	bne.b	lbC0022C8
lbC0022C2	movea.l	A3,A0
	subq.l	#1,A0
	bra.b	lbC0022CA

lbC0022C8	movea.l	A3,A0
lbC0022CA	cmpa.l	A5,A0
	bcs.b	lbC0022D4
	move.l	A5,(A2)
	move.l	A0,(4,A2)
lbC0022D4	lea	(2,A0),A5
lbC0022D8	addq.l	#1,D6
	addq.l	#8,A2
lbC0022DC	addq.l	#1,D5
lbC0022DE	cmp.l	D7,D5
	blt.b	lbC00227C
	movem.l	(SP)+,D2-D4/D7/A2/A3/A5
	move.l	($4A8,SP),D5
	bne.b	lbC0023F4
	move.l	($26,SP),D0
	move.l	D0,D1
	sub.l	($22,SP),D1
	moveq	#2,D2
	cmp.l	D2,D1
	blt.b	lbC0023F4
	movea.l	D0,A1
	lea	(-2,A1),a0
	move.l	a0,d0
	lea	(RAW.MSG,PC),a0
	bsr	lbC0012F6
	beq.b	lbC0023F4
	bset	#1,(2,A5)
lbC0023F4	tst.l	D5
	bne	lbC002552
	move.b	#$3A,($343,SP)
	clr.b	($470,SP)
	moveq	#1,D5
	lea	($2A,SP),A3
	bra	lbC0024B6

lbC00240E	move.l	D5,D0
	subq.l	#1,D0
	bne.b	lbC002452
	tst.l	($2A,SP)
	beq.b	lbC002452
	movea.l	($2A,SP),A0
	cmpi.b	#$39,(A0)
	bls.b	lbC002452
	move.l	($2E,SP),D0
	sub.l	($2A,SP),D0
	addq.l	#1,D0
	moveq	#$1F,D1
	cmp.l	D1,D0
	blt.b	lbC002436
	move.l	D1,D0
lbC002436	move.l	D0,D4
	movea.l	A0,a1
	lea	($470,SP),a0
	bsr	lbC00DD34
	clr.b	($470,SP,D4.L)
	moveq	#2,D7
	bra.b	lbC0024B2

lbC002452	move.l	D7,D0
	addq.l	#4,D0
	cmp.l	D0,D5
	ble.b	lbC0024B2
;	move.l	(4,A3),-(SP)
;	move.l	(A3),-(SP)
	bsr	lbC002210
	beq.b	lbC0024B2
	movea.l	D0,A2
	moveq	#5,D0
	cmp.b	(4,A2),D0
	bne.b	lbC0024B2
	moveq	#$69,D0
	cmp.b	(5,A2),D0
	bne.b	lbC0024B2
	bsr	lbC0022EA
	move.l	D0,D4
	moveq	#$1F,D1
	cmp.l	D1,D0
	bge.b	lbC002498
	move.l	D4,D0
	bra.b	lbC00249A

lbC002498	move.l	D1,D0
lbC00249A	movea.l	($490,SP),a1
	lea	($470,SP),a0
	bsr	lbC00DD34
	clr.b	($470,SP,D4.L)
lbC0024B2	addq.l	#1,D5
	addq.l	#8,A3
lbC0024B6	cmp.l	D6,D5
	bge.b	lbC0024C2
	tst.b	($470,SP)
	beq	lbC00240E
lbC0024C2	tst.b	($470,SP)
	bne.b	lbC0024DE
	btst	#1,(2,A5)
	beq.b	lbC0024D6
	lea	(KRAWprefs.MSG,PC),A0
	bra.b	lbC0024DA

lbC0024D6	lea	(KCONprefs.MSG,PC),A0
lbC0024DA	movea.l	A0,A3
	bra.b	lbC0024E2

lbC0024DE	lea	($470,SP),A3
lbC0024E2	movea.l	($498,SP),A2

	movea.l	a2,a0
	moveq	#15,d0		GetVar()
	pea	($400).W
	pea	($12B).W
	pea	($34C,SP)
	move.l	a3,-(SP)
	bsr	_DoSyncDos
	lea	(16,SP),SP

	move.l	D0,D4
	move.l	($4A8,SP),D5
	tst.l	D0
	ble.b	lbC002524
	addq.l	#1,D4
	move.l	D5,D1
	addq.l	#1,D1
	move.l	D1,-(SP)
	move.l	D4,-(SP)
	pea	($34B,SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC00239C
	lea	(20,SP),SP
	bra.b	lbC002552

lbC002524	moveq	#-1,D0
	cmp.l	D0,D4
	bne.b	lbC002544
	lea	($470,SP),A0
	cmpa.l	A0,A3
	bne.b	lbC002544
	move.l	D7,D1
	subq.l	#1,D1
	bne.b	lbC002544
	move.l	#$CE,($40,A2)
	bra	lbC002948

lbC002544	addq.l	#1,D4
	bne.b	lbC002552
	lea	($470,SP),A0
	cmpa.l	A0,A3
	bne.b	lbC002552
	moveq	#1,D7
lbC002552	move.l	($22,SP,D7.L*8),D0
	beq.b	lbC002568
	move.l	($26,SP,D7.L*8),d1
	bsr	lbC00117A
	move.w	D0,(8,A5)
lbC002568	move.l	D7,D4
	addq.l	#1,D4
	move.l	($22,SP,D4.L*8),D0
	beq.b	lbC002582
	move.l	($26,SP,D4.L*8),d1
	bsr	lbC00117A
	move.w	D0,(10,A5)
lbC002582	addq.l	#1,D4
	move.l	($22,SP,D4.L*8),D0
	beq.b	lbC0025AC
	move.l	($26,SP,D4.L*8),d1
	bsr	lbC00117A
	move.w	D0,(12,A5)
	moveq	#$50,D1
	cmp.w	D1,D0
	bcs	lbC0025A8
	move.w	D0,D1
lbC0025A8	move.w	D1,(12,A5)
lbC0025AC	addq.l	#1,D4
	move.l	($22,SP,D4.L*8),D0
	beq.b	lbC0025D6
	move.l	($26,SP,D4.L*8),d1
	bsr	lbC00117A
	move.w	D0,(14,A5)
	moveq	#$32,D1
	cmp.w	D1,D0
	bcs	lbC0025D2
	move.w	D0,D1
lbC0025D2	move.w	D1,(14,A5)
lbC0025D6	addq.l	#1,D4
	move.l	($22,SP,D4.L*8),D0
	beq	lbC002932
	move.l	($26,SP,D4.L*8),D1
	sub.l	D0,D1
	moveq	#0,D2
	move.b	D1,D2
	addq.l	#1,D2
	move.l	D2,($24,A5)
	move.l	($22,SP,D4.L*8),($20,A5)
	bra	lbC002932

lbC0025FA	lea	($22,SP),A1
	lea	(A1,D4.L*8),A0
	movea.l	A0,A3
;	move.l	(4,A3),-(SP)
;	move.l	(A3),-(SP)
	bsr	lbC002210
	beq	lbC002932
	movea.l	D0,A2
	moveq	#0,D0
	move.b	(4,A2),D0
	subq.w	#1,D0
;	blt	lbC002932
;	cmpi.w	#5,D0
;	bge	lbC002932
	move.w	(lbW002636,PC,D0.W*2),D1
	move.l	(6,A2),D0
	movea.l	a5,a0
	jmp	(lbW002638,PC,D1.W)

lbW002636	dc.w	lbC002640-lbW002638
lbW002638	dc.w	lbC00264A-lbW002638
	dc.w	lbC002656-lbW002638
	dc.w	lbC002662-lbW002638
	dc.w	lbC002670-lbW002638

lbC002656	addq.l	#4,a0
lbC002640	or.l	D0,(A0)
	bra	lbC002932

lbC002662	addq.l	#4,a0
lbC00264A	not.l	D0
	and.l	D0,(A0)
	bra	lbC002932

lbC002670	bsr	lbC0022EA
	move.l	D0,D7
	movea.l	A2,A0
	movea.l	($498,SP),A2
	moveq	#0,D0
	move.b	(5,A0),D0
	moveq	#$64,D1
	sub.l	D1,D0
	blt	lbC002932
	cmpi.w	#12,D0
	bge	lbC002932
	move.w	(.1,PC,D0.W*2),D0
	tst.l	d7
	jmp	(.2,PC,D0.W)

.1	dc.w	.SCREEN-.2
.2	dc.w	.FONT-.2
	dc.w	.WINDOW-.2
	dc.w	.ALT-.2
	dc.w	.MAXBUF-.2
	dc.w	.PREFS-.2
	dc.w	.IMAGE-.2
	dc.w	.FNC-.2
	dc.w	.ICONPOS-.2
	dc.w	.ICONTITLE-.2
	dc.w	.DIRCACHE-.2
	dc.w	.SGR-.2

.SCREEN	ble	lbC002932
	move.l	($490,SP),($30,A5)
	move.l	D7,($34,A5)
	bra	lbC002932

.IMAGE	ble	lbC002932
	move.l	($490,SP),($18,A5)
	move.l	D7,($1C,A5)
	bra	lbC002932

.ICONTITLE	ble	lbC002932
	move.l	($490,SP),($28,A5)
	move.l	D7,($2C,A5)
	bra	lbC002932

.SGR	ble	lbC002932
	movea.l	($490,SP),a0
	lea	(_sgr,a4),a1
	bsr	ascii2sgr
	st	(_asdss,a4)
	bra	lbC002932

.FONT	beq	lbC002932
	move.l	($490,SP),($38,A5)
	bra.b	\4

\1	movea.l	($490,SP),A0
	move.b	(A0),D0
	moveq	#$2E,D1
	cmp.b	D1,D0
	beq.b	\2
	moveq	#$2C,D1
	cmp.b	D1,D0
	bne.b	\3
\2	move.l	A0,D1
	sub.l	($38,A5),D1
	moveq	#0,D2
	move.b	D1,D2
	move.l	D2,($3C,A5)
	lea	(1,A0),a1
	movea.l	(4,A3),a0
	bsr	ascii2long
	move.l	D0,($40,A5)
\3	addq.l	#1,($490,SP)
\4	movea.l	($490,SP),A0
	cmpa.l	(4,A3),A0
	bcc.b	\5
	tst.l	($40,A5)
	beq.b	\1
\5	cmpi.l	#3,($40,A5)
	bge	lbC002932
	move.l	#$CE,($40,A2)
	bra	lbC002932

.WINDOW	ble	lbC002932
\6	move.l	D6,-(SP)
	movea.l	(4,a3),A0
	move.l	($494,SP),a1
	moveq	#0,D1
	moveq	#$20,D0
	cmp.b	(A1),D0
	bne.b	\13
	bra.b	\12

\7	move.b	(A1),D6
	moveq	#$30,D0
	cmp.b	D0,D6
	bcs.b	\8
	moveq	#$39,D0
	cmp.b	D0,D6
	bhi.b	\8
	moveq	#0,D0
	move.b	D6,D0
	asl.l	#4,D1
	add.l	D0,D1
	moveq	#$30,D0
	sub.l	D0,D1
	bra.b	\12

\8	moveq	#$61,D0
	cmp.b	D0,D6
	bcs.b	\9
	moveq	#$66,D0
	cmp.b	D0,D6
	bhi.b	\9
	moveq	#0,D0
	move.b	D6,D0
	asl.l	#4,D1
	add.l	D0,D1
	moveq	#$57,D0
	sub.l	D0,D1
	bra.b	\12

\9	moveq	#$41,D0
	cmp.b	D0,D6
	bcs.b	\10
	moveq	#$46,D0
	cmp.b	D0,D6
	bhi.b	\10
	moveq	#0,D0
	move.b	D6,D0
	asl.l	#4,D1
	add.l	D0,D1
	moveq	#$37,D0
	sub.l	D0,D1
	bra.b	\12

\10	moveq	#$58,D0
	cmp.b	D0,D6
	beq.b	\11
	moveq	#$78,D0
	cmp.b	D0,D6
	bne.b	\14
\11	tst.l	D1
	bne.b	\14
\12	addq.l	#1,A1
\13	cmpa.l	A0,A1
	bls.b	\7
\14	move.l	D1,D0
	move.l	(SP)+,D6
	move.l	D0,($5C,A5)
	bra	lbC002932

.ALT	ble.b	\15
	move.l	(4,A3),d1
	move.l	($494,SP),d0
	bsr	lbC00117A
	move.w	D0,($10,A5)
\15	move.l	D4,D0
	addq.l	#1,D0
	moveq	#$64,D1
	cmp.l	D1,D0
	bge.b	\16
	lea	(8,A3),A2
	tst.l	(A2)
	beq.b	\16
	move.l	(4,A2),d1
	move.l	(A2),d0
	bsr	lbC00117A
	move.w	D0,($12,A5)
\16	move.l	D4,D0
	addq.l	#2,D0
	moveq	#$64,D1
	cmp.l	D1,D0
	bge.b	\19
	lea	($10,A3),A2
	tst.l	(A2)
	beq.b	\19
	move.l	(4,A2),d1
	move.l	(A2),d0
	bsr	lbC00117A
	move.w	D0,($14,A5)
	moveq	#$50,D1
	cmp.w	D1,D0
	bcc.b	\17
	moveq	#$50,D1
	bra.b	\18

\17	moveq	#0,D1
	move.w	D0,D1
\18	move.w	D1,($14,A5)
\19	addq.l	#3,D4
	moveq	#$64,D0
	cmp.l	D0,D4
	bge	lbC002932
	lea	($18,A3),A2
	tst.l	(A2)
	beq	lbC002932
	move.l	(4,A2),d1
	move.l	(A2),d0
	bsr	lbC00117A
	move.w	D0,($16,A5)
	moveq	#$32,D1
	cmp.w	D1,D0
	bcc.b	\20
	moveq	#$32,D1
	bra.b	\21

\20	moveq	#0,D1
	move.w	D0,D1
\21	move.w	D1,($16,A5)
	bra	lbC002932

.MAXBUF	ble.b	\22
	movea.l	(4,A3),a0
	movea.l	($490,SP),a1
	bsr	ascii2long
	move.l	D0,($60,A5)
\22	move.l	($60,A5),D0
	bpl.b	\23
	move.l	D0,D1
	neg.l	D1
	bra.b	\24

\23	move.l	D0,D1
\24	moveq	#4,D2
	cmp.l	D2,D1
	bge	lbC002932
	tst.l	D0
	bpl.b	\25
	moveq	#-4,D0
	bra.b	\26

\25	move.l	D2,D0
\26	move.l	D0,($60,A5)
	bra	lbC002932

.PREFS	tst.l	D5
	ble	lbC002932
	move.l	#$CE,($40,A2)
	bra	lbC002932

.FNC	ble	lbC002932
	movea.l	a1,a0
\27	movem.l	D6/D7,-(SP)
	moveq	#0,D6
	bra.b	\35

\28	moveq	#0,D0
	move.b	(A0),D0
	moveq	#$42,D1
	sub.l	D1,D0
	beq.b	\31
	subq.l	#1,D0
	beq.b	\32
	moveq	#9,D1
	sub.l	D1,D0
	beq.b	\30
	subq.l	#7,D0
	beq.b	\33
	subq.l	#4,D0
	beq.b	\29
	moveq	#11,D1
	sub.l	D1,D0
	beq.b	\31
	subq.l	#1,D0
	beq.b	\32
	moveq	#9,D1
	sub.l	D1,D0
	beq.b	\30
	subq.l	#7,D0
	beq.b	\33
	subq.l	#4,D0
	bne.b	\34
\29	bset	#0,D6
	bra.b	\34

\30	bset	#1,D6
	bra.b	\34

\31	bset	#2,D6
	bra.b	\34

\32	bset	#3,D6
	bra.b	\34

\33	bset	#4,D6
\34	addq.l	#1,A0
	subq.l	#1,D7
\35	tst.l	D7
	bgt.b	\28
	move.w	D6,D0
	movem.l	(SP)+,D6/D7
	move.w	D0,($64,A5)
	bra	lbC002932

.ICONPOS	ble.b	\36
	move.l	(4,A3),d1
	move.l	($494,SP),d0
	bsr	lbC00117A
	move.l	D0,($54,A5)
\36	addq.l	#1,D4
	moveq	#$64,D0
	cmp.l	D0,D4
	bge	lbC002932
	lea	(8,A3),A2
	tst.l	(A2)
	beq.b	lbC002932
	move.l	(4,A2),d1
	move.l	(A2),d0
	bsr	lbC00117A
	move.l	D0,($58,A5)
	bra.b	lbC002932

.DIRCACHE	moveq	#3,D0
	or.l	D0,(4,A5)
	tst.l	D7
	ble.b	\37
	move.l	(4,A3),d1
	move.l	($494,SP),d0
	bsr	lbC00117A
	move.l	D0,($44,A5)
\37	move.l	D4,D0
	addq.l	#1,D0
	moveq	#$64,D1
	cmp.l	D1,D0
	bge.b	\38
	lea	(8,A3),A2
	tst.l	(A2)
	beq.b	\38
	move.l	(4,A2),d1
	move.l	(A2),d0
	bsr	lbC00117A
	move.l	D0,($48,A5)
\38	addq.l	#2,D4
	moveq	#$64,D0
	cmp.l	D0,D4
	bge.b	lbC002932
	lea	($10,A3),A2
	tst.l	(A2)
	beq.b	lbC002932
	move.l	(A2),($4C,A5)
	move.l	(4,A2),D0
	sub.l	(A2),D0
	addq.l	#1,D0
	move.l	D0,($50,A5)
lbC002932	addq.l	#1,D4
	cmp.l	D6,D4
	blt	lbC0025FA
	btst	#0,($65,A5)
	beq.b	lbC002948
	moveq	#-7,D0
	and.w	D0,($64,A5)
lbC002948	movem.l	(SP)+,D2/D4-D7/A2/A3/A5
	adda.w	#$474,SP
	rts

lbC0029DE	suba.w	#$74,SP
	movem.l	D2/D7/A2/A3/A5/A6,-(SP)
	movea.l	a5,a2
	movea.l	a0,A5
	moveq	#1,D7
	clr.l	($40,A5)
	moveq	#40,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(A5),A6
	sys	AllocVec
	move.l	D0,d2		internal lock
	bne.b	\1
	moveq	#ERROR_NO_FREE_STORE,D1
	move.l	D1,($40,A5)
	bra	\28

\1	move.l	d2,($1C,SP)
	movea.l	d2,A0
	move.l	(dp_Type,A3),($18,A0)
	movea.l	(dp_Port,A3),a1
	move.l	(MP_SIGTASK,a1),d0
	move.l	D0,($1E,A0)	sender process of the packet
	move.l	d0,($22,A0)	who is to receive ctrl-x signals
	movea.l	(dp_Arg1,A3),a0	fh
	adda.l	a0,a0
	adda.l	a0,a0
	move.l	D2,(fh_Arg1,A0)
	moveq	#-1,D1
	move.l	D1,(fh_Interactive,A0)	we're interactive
	suba.l	A3,A3
	move.l	A2,D0	first open of this console?
	beq.b	\4
	addq.w	#1,($76,A2)	increase open cnt
	lea	($64,A2),A0
	movea.l	D2,A1
	sys	AddTail
	move.l	d2,($72,A2)
	move.l	A2,D0
	bra	\28

\4	lea	($26,SP),a0
	move.l	#$84040,(A0)+	0
	clr.l	(A0)+		4
	move.l	D1,(A0)+	8
	move.l	D1,(A0)+	c
	move.l	D1,(A0)+	10
	move.l	D1,(A0)+	14
	lea	(def_KingCON.MSG,pc),A1
	move.l	A1,(A0)+	18
	moveq	#11,D0
	move.l	D0,(A0)+	1c
	lea	(KingCON.MSG,pc),A1
	move.l	A1,(A0)+	20
	moveq	#7,D0
	move.l	D0,(A0)+	24
	moveq	#6,d0
.c	clr.l	(A0)+		28-40
	dbf	d0,.c
;	move.w	#$FFFF,D0
	move.l	D0,(A0)+	44
	move.l	D0,(A0)+	48
	clr.l	(A0)+		4c
	clr.l	(A0)+		50
	move.l	#$80000000,D1
	move.l	D1,(A0)+	54
	move.l	D1,(A0)+	58
	clr.l	(A0)+		5c
	moveq	#$40,D0
	move.l	D0,(A0)+	60
	move.w	#1,(A0)		64

	movea.l	($90,SP),A2
	movea.l	($1C,A2),a0
	adda.l	a0,a0
	adda.l	a0,a0
	moveq	#0,D1
	move.b	(A0)+,D1
	clr.l	-(SP)
	move.l	D1,-(SP)
	pea	(A0)
	pea	($32,SP)
	move.l	A5,-(SP)
	bsr	lbC00239C
	lea	(20,SP),SP
	move.l	($40,A5),d0
	bne	\22
	move.w	#$1688+10,D0
	add.l	($4A,SP),D0
	add.l	($62,SP),D0
	add.l	($52,SP),D0
	add.l	($42,SP),D0
	add.l	($76,SP),D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	sys	AllocVec
	tst.l	D0
	beq	\22
	movea.l	D0,A3
	lea	($26,SP),A1
	move.l	(a1)+,($160,A3)
	move.l	(a1)+,($164,A3)
	lea	($124,a3),a0
	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	move.l	(a1)+,(a0)+
	move.l	($86,SP),D1
	asl.l	#8,D1
	asl.l	#2,D1
	move.l	D1,($180,A3)
	move.w	($8A,SP),($17E,A3)
	moveq	#$40,D0
	lsl.l	#3,D0
	and.l	($26,SP),D0
	move.w	D0,($102,A3)
	moveq	#-1,d0
	lea	($13C,A3),A1
	move.l	D0,(A1)+
	move.l	D0,(A1)+
;	clr.w	($100,A3)
;	clr.w	($11E,A3)
	lea	($9FC,A3),A0
	move.l	A0,($11A,A3)
	moveq	#0,d1
	move.w	d0,D1
	move.l	($6A,SP),D0
	cmp.l	d1,D0
	beq	\7
\6	asl.l	#8,D0
	asl.l	#2,D0
\7	move.l	D0,($154,A3)
	move.l	($6E,SP),D0
	cmp.l	d1,D0
	beq	\9
\8	asl.l	#8,D0
	asl.l	#2,D0
\9	move.l	D0,($158,A3)
	lea	($1688,A3),A1
	move.l	A1,($16C,A3)
	movea.l	($46,SP),A0
	move.l	($4A,SP),D0
	sys	CopyMem
	move.l	($4A,SP),D2
	addq.l	#1,D2
	tst.l	($66,SP)
	beq.b	\10
	movea.l	A3,A0
	adda.l	D2,A0
	lea	($1688,A0),A1
	move.l	A1,($174,A3)
	movea.l	($5E,SP),A0
	move.l	($62,SP),D0
	sys	CopyMem
	movea.l	($174,A3),A1
	adda.l	($62,SP),A1
	lea	(font.MSG,PC),A0
	moveq	#5,D0
	sys	CopyMem
	move.l	($66,SP),D0
	move.w	D0,($17C,A3)
	add.l	($62,SP),D2
	addq.l	#6,D2
\10	move.l	($52,SP),D0
	beq.b	\11
	movea.l	A3,A0
	adda.l	D2,A0
	lea	($1688,A0),A1
	move.l	A1,($170,A3)
	movea.l	($4E,SP),A0
	move.l	($52,SP),D0
	sys	CopyMem
	add.l	($52,SP),D2
	addq.l	#1,D2
	bra.b	\12

\11	move.l	($16C,A3),($170,A3)
\12	lea	(a3,d2.l),a0
	lea	($1688,A0),A1
	move.l	A1,($178,A3)
	movea.l	($3E,SP),A0
	move.l	($42,SP),D0
	sys	CopyMem
	move.l	D2,D0
	add.l	($42,SP),D0
	addq.l	#1,D0
	move.l	($76,SP),D1
	beq.b	\13
	movea.l	A3,A0
	adda.l	D0,A0
	lea	($1688,A0),A1
	move.l	A1,($15C,A3)
	movea.l	($72,SP),A0
	move.l	($76,SP),D0
	sys	CopyMem
\13	move.l	($56,SP),D0
	beq.b	\15
	move.l	($5A,SP),D1
	moveq	#$74,D2
	not.b	D2
	cmp.l	D2,D1
	bgt.b	\14
	move.l	D1,D2
\14	lea	($15FC,A3),A0
	move.l	A0,($20,SP)
	movea.l	D0,A0
	move.l	D2,D0
	movea.l	($20,SP),A1
	sys	CopyMem
\15	move.l	(4,A2),D0
	moveq	#pr_MsgPort,D1
	sub.l	D1,D0
	move.l	D0,($EC,A3)
	move.l	D0,($E8,A3)
	moveq	#1,d0
	move.l	($16C,A3),d1
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00D394
	tst.w	D0
	beq.b	\16
	bset	#6,($161,A3)
\16	addq.w	#1,($76,A3)
	lea	($64,A3),A2
	movea.l	A2,a0
	bsr	_newlist
	movea.l	A2,A0
	movea.l	($1c,SP),A1
	sys	AddTail
	move.l	($1c,SP),($72,A3)
	lea	($DFC,A3),A0
	move.l	A0,($10A,A3)
	move.l	A0,($10E,A3)
	move.l	($82,SP),($168,A3)
	lea	($2E,A3),A2
	moveq	#0,d0
	lea	($2000).W,a1
	movea.l	A2,a0
	bsr	lbC009826
	tst.w	D0
	bne.b	\17
	moveq	#1,d0
	lea	($2000).W,a1
	movea.l	A2,a0
	bsr	lbC009826
\17	lea	($32,A3),A0
	move.l	A0,($60,A3)
	btst	#2,($162,A3)
	bne.b	\18
	bsr	lbC00A57A
	tst.l	D0
	beq	\22
\18	sys	CreateMsgPort
	move.l	D0,($CC,A3)
	beq	\22
	movea.l	D0,A0
	moveq	#1,D0
	move.b	(MP_SIGBIT,A0),D1
	lsl.l	D1,D0
	move.l	D0,($E0,A3)
	movea.l	($30,A5),A0
	adda.w	#pr_MsgPort,A0
	moveq	#$28,D0
	sys	CreateIORequest
	move.l	D0,($A0,A3)
	beq	\22
	movea.l	D0,A1

	ifnd	rom
	lea	(timerdevice.MSG,PC),A0
	moveq	#0,d0
	else
	moveq	#ODTAG_TIMER,D0
	movea.l	d0,a0		ROM hack
	endc

	move.l	D0,D1
	sys	OpenDevice
	move.l	D0,D7
	bne	\22
	movea.l	($A0,A3),A0
	clr.b	(8,A0)
	lea	($F0,A3),a0
	bsr	_newlist
	movea.l	A3,A0
	sys	InitSemaphore
	moveq	#1,D0
	movea.l	($1C,SP),A0
	lea	(14,A0),A6
	move.l	D0,(A6)+
	move.w	d0,(A6)+
	move.l	A3,(A6)+
	btst	#0,($167,A3)
	beq.b	\20
	tst.l	($154,A3)
	bne.b	\19
	tst.l	($158,A3)
	bne.b	\19
	movea.l	A5,a0
	bsr	lbC008A98
	moveq	#-4,D0
	and.l	D0,($164,A3)
	bra.b	\20

\19	movea.l	A3,a0
	;move.l	A5,-(SP)
	bsr	lbC0088A2
\20	btst	#0,($29,SP)
	bne.b	\21
	moveq	#1,d0
	move.l	A3,a0
	move.l	A5,a1
	bsr	_OpenWindow
	tst.w	D0
	beq.b	\22
\21	clr.l	($40,A5)
	move.l	A3,D0
	bra	\28

\22	movea.l	(A5),A6
	move.l	A3,D0
	beq.b	\26
	tst.l	D7
	bne.b	\23
	movea.l	($A0,A3),A1
	sys	CloseDevice
\23	move.l	($A0,A3),d0
	beq.b	\24
	move.l	d0,a0
	sys	DeleteIORequest
\24	move.l	($CC,A3),d0
	beq.b	\25
	move.l	D0,a0
	sys	DeleteMsgPort
\25	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00A546
	movea.l	($5C,A3),A0
	lea	($2E,A3),A1
	move.l	A1,-(SP)
	move.l	A5,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	movea.l	A3,A1
	movea.l	(A5),A6
	sys	FreeVec
\26	move.l	($1C,SP),d0
	beq.b	\27
	movea.l	d0,A1
	sys	FreeVec
\27	move.l	([$90,SP],$14),D1
	asl.l	#2,D1
	movea.l	D1,A0
	clr.l	($24,A0)
	clr.l	(4,A0)
	moveq	#0,D0
\28	movem.l	(SP)+,D2/D7/A2/A3/A5/A6
	adda.w	#$74,SP
	rts

lbC00A57A	movem.l	A2/A6,-(SP)
	movea.l	(A5),A6
	move.l	#$1396,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	sys	AllocVec
	tst.l	D0
	beq	\8
	movea.l	D0,a2
	lea	($A35,a2),A0
	move.l	A0,($62,a2)
	move.w	#$50,($60,a2)
;	lea	($A36,a2),A0
	addq.l	#1,a0
	move.l	A0,($D2,a2)
	btst	#0,($161,a3)	NOSTYLES
	bne.b	\1
	lea	($74,a2),a0
	move.l	(_sgr,a4),(a0)
	lea	($7C,a2),a1
	bsr	sgr2ascii
	move.w	D0,($78,a2)
;	move.w	#1,($70,a2)
	st	($71,a2)
	movea.l	a2,A1
	bsr	lbC00A4F4
	bra.b	\2

\1	;clr.w	($78,a2)
	lea	($D6,a2),A0
	move.l	A0,($66,a2)
	;clr.w	($70,a2)
\2	;clr.l	($56,a2)
	;move.w	#$FFFF,($5A,a2)
	not.w	($5A,a2)
	move.l	($180,a3),D0	review buffer max size
	move.l	D0,D1
	bpl.b	\3
	neg.l	D1
\3	tst.l	D0
	sgt	D0
	movea.l	D1,a1
	lea	($24,a2),a0
	bsr	lbC009826
	tst.w	D0
	bne.b	\7
	suba.l	A0,A0
	movea.l	(8,A5),A6
	sys	DisplayBeep
	move.l	($180,a3),D0
	move.l	D0,D1
	bpl.b	\5
	neg.l	D1
\5	moveq	#1,d0		force dynamic buffer
	movea.l	D1,a1
	lea	($24,a2),a0
	bsr	lbC009826
\7	move.l	a2,D0
\8	move.l	d0,($90,a3)
	movem.l	(SP)+,a2/A6
	rts

_CloseWindow:	movem.l	A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	($78,A3),A0	window
	lea	(wd_LeftEdge,A0),A0
	lea	($13C,A3),A1
	move.l	(A0)+,(A1)+
	move.l	(A0)+,(A1)+
	suba.l	a1,a1
	movea.l	A5,a0
	bsr	_FindTask
	movea.l	($A8,A3),a1
	bsr	lbC0015EE
	movea.l	($9C,A3),A1
	movea.l	(A5),A6
	sys	CloseDevice
	movea.l	($9C,A3),A0
	sys	DeleteIORequest
	movea.l	($98,A3),a1
	bsr	lbC001338
	move.l	A1,-(sp)
	sys	CloseDevice
	movea.l	(sp)+,A0
	sys	DeleteIORequest
	movea.l	($94,A3),A0
	sys	DeleteMsgPort
	bsr	lbC000E90
	bsr	lbC00C8E8
	move.l	($84,A3),d0
	beq.b	\1
	bsr	lbC00D346
\1	move.l	(_cat,a4),d0
	beq	.1
	movea.l	d0,a0
	movea.l	(_locb,a4),a6	locale base
	sys	CloseCatalog
.1	movea.l	($78,A3),A0
	movea.l	(8,A5),A6
	sys	CloseWindow
	bsr	lbC00A468
	movea.l	($80,A3),A0
	movea.l	(12,A5),A6
	sys	FreeVisualInfo
	move.l	($8C,A3),D0
	beq.b	\2
	movea.l	D0,A1
	movea.l	(16,A5),A6
	sys	CloseFont
\2	movea.l	A3,a0
	movem.l	(SP)+,A3/A5/A6
lbC001928	lea	($78,A0),A1
	clr.l	(A1)+	78
	clr.l	(A1)+	7c
	clr.l	(A1)+	80
	addq.l	#4,a1	84
	clr.l	(A1)+	88
	clr.l	(A1)+	8c
	addq.l	#4,a1	90
	clr.l	(A1)+	94
	clr.l	(A1)+	98
	clr.l	(A1)+	9c
	addq.l	#4,a1	a0
	clr.l	(A1)+	a4
	clr.l	(A1)	a8
	rts

lbC00D346	movem.l	A2/A6,-(SP)
	movea.l	d0,A2
	addq.l	#8,a2
	movea.l	($78,A3),A0
	movea.l	(A2),A1
	movea.l	(8,A5),A6
	sys	RemoveGadget
	movea.l	(A2),A0
	sys	DisposeObject
	movea.l	-(A2),A0
	sys	DisposeObject
	movea.l	-(A2),A0
	move.l	a0,d0
	beq	\1
	sys	FreeClass
\1
	ifd	tbc
	movea.l	($88,A3),A0
	movea.l	(12,A2),A1
	sys	FreeScreenDrawInfo
	endc
	ifd	OS4
	ifnd	tbc
	movea.l	($88,A3),A0
	movea.l	(12,A2),A1
	sys	FreeScreenDrawInfo
	endc
	endc
	movea.l	A2,A1
	movea.l	(A5),A6
	moveq	#16,d0
	sys	FreeMem
	clr.l	($84,A3)
	movem.l	(SP)+,A2/A6
	rts

	ifnd	async
lbC002F88	movem.l	D2/D3/D7/A2/A3/A5/A6,-(SP)
	movea.l	a5,a3
	lea	(7*4+48,sp),a5
	movea.l	(a5),A6
;	move.l	a3,D0
;	beq	\20
	move.w	($76,a3),d3
	beq	\5
	lea	($72,a3),a2
	move.l	(a2),d0
	beq.b	\1
	movea.l	d0,A1
	sys	Remove
	movea.l	(a2),A1
	sys	FreeVec
	clr.l	(a2)
\1	subq.w	#1,($76,a3)
	beq	\3
	subq.w	#2,d3
	bne.b	\4
	movea.l	($64,a3),A2
	move.l	($22,A2),d0
	bsr	_ChkTask
	tst.l	D0
	beq.b	\4
	move.l	($1E,A2),($EC,a3)
	bra.b	\4

\3	clr.l	($EC,a3)
	clr.l	($E8,a3)
;	btst	#7,($163,a3)
;	beq.b	\5
	tst.b	($163,a3)
	bpl	\5
	btst	#1,($162,a3)	RAW?
	bne.b	\5		CON mode
	move.l	($78,a3),D0
	bne.b	\4
	tst.l	($D8,a3)
	bne.b	\4
	tst.l	($D4,a3)
	beq.b	\5
\4	tst.l	($78,a3)
	beq	\flush
	movea.l	a3,a0
	movea.l	a5,a1
	bsr	lbC001954
\flush	movea.l	(16,sp),a0	our ACTION_END packet
	move.l	(dp_Arg1,a0),d7
.nxt	movea.l	($F0,a3),A2
	bra	.3

.1	movea.l	(14,A2),a0
	cmp.l	(dp_Arg1,a0),d7
	bne	.2
	move.l	a0,d1
	movea.l	a2,a1
	REMOVE
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
	movea.l	A2,A1
	movea.l	(a5),A6
	sys	FreeVec
	bra	.nxt

.2	movea.l	(a2),a2
.3	tst.l	(A2)
	bne.b	.1
	moveq	#1,d0
	bra	\20

\5	lea	(KingCONDC1.MSG,pc),A1
	sys	Forbid
	sys	FindSemaphore
	tst.l	D0
	beq.b	\7
	movea.l	D0,a0
	sys	ObtainSemaphore
	subq.w	#1,($66,a0)
	sys	ReleaseSemaphore
\7	sys	Permit
	tst.l	($78,a3)
	beq.b	\8
	movea.l	a3,a0
	bsr	_CloseWindow
\8	move.l	($DC,a3),D0
	beq.b	\9
	move.l	D0,a1
	move.l	a5,a0
	moveq	#17,d0		FreeDiskObject()
	bsr	_l1
\9	moveq	#0,D0
	moveq	#0,D7
\10	move.b	D7,D0
	move.l	($AC,a3,D0.W*4),d0
	beq.b	\11
	movea.l	D0,a2
	movea.l	A2,a1
	bsr	lbC001338
	movea.l	($62,A2),A0
	move.l	(14,A0),D1
	beq.b	\12
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
\12	movea.l	($62,A2),A0
	clr.l	(14,A0)
	movea.l	a3,a0
	bsr	lbC0016EE
\11	addq.b	#1,D7
	moveq	#8,D0
	cmp.b	D0,D7
	bcs.b	\10
	bra.b	\13

\14	movea.l	a1,a2
	REMOVE
	move.l	(14,A2),D1
	beq.b	\15
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
\15	movea.l	A2,A1
	movea.l	(a5),A6
	sys	FreeVec
\13	movea.l	($F0,a3),A1
	tst.l	(A1)
	bne.b	\14
	movea.l	($A0,a3),A1
	sys	CloseDevice
	movea.l	($A0,a3),A0
	sys	DeleteIORequest
	tst.l	($CC,a3)
	beq.b	\16
	bra.b	\17

\18	sys	ReplyMsg
\17	movea.l	($CC,a3),A0
	sys	GetMsg
	movea.l	D0,A1
	tst.l	D0
	bne.b	\18
	bsr	_remAppIcon
;	bsr	_remAppMenuItem
	movea.l	(a5),A6
	movea.l	($CC,a3),A0
	sys	DeleteMsgPort
\16	move.l	a3,a0
	move.l	a5,a1
	bsr	lbC00A546
	movea.l	($5C,a3),A0
	lea	($2E,a3),A1
	move.l	A1,-(SP)
	move.l	a5,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	movea.l	a3,A1
	movea.l	(a5),A6
	sys	FreeVec
\19	moveq	#0,D0
\20	movem.l	(SP)+,D2/D3/D7/A2/A3/A5/A6
	rts

	else

lbC002F88	movem.l	d2-d5/A2/A3/A5/A6,-(SP)
	movea.l	a5,a3
	lea	(8*4+48,sp),a5
	movea.l	(a5),A6
	move.w	($76,a3),d4
	beq	\flush
	lea	($72,a3),a2
	move.l	(a2),d0
	beq.b	\1
	movea.l	d0,A1
	sys	Remove
	movea.l	(a2),A1
	sys	FreeVec
	clr.l	(a2)
\1	subq.w	#1,($76,a3)
	beq	\3
	subq.w	#2,d4
	bne.b	\4
	movea.l	($64,a3),A2
	move.l	($22,A2),d0
	bsr	_ChkTask
	tst.l	D0
	beq.b	\4
	move.l	($1E,A2),($EC,a3)
	bra.b	\4

\3	moveq	#0,d4
	clr.l	($EC,a3)
	clr.l	($E8,a3)
;	btst	#7,($163,a3)
;	beq.b	\5
	tst.b	($163,a3)
	bpl	\flush
	btst	#1,($162,a3)	RAW?
	bne.b	\flush		CON mode
	move.l	($78,a3),D0
	bne.b	\4
	tst.l	($D8,a3)
	bne.b	\4
	move.l	($D4,a3),d4
	beq.b	\flush
\4	tst.l	($78,a3)
	beq.b	\6
	movea.l	a3,a0
	movea.l	a5,a1
	bsr	lbC001954
\6	moveq	#1,d4
\flush	movea.l	(20,sp),a0	our ACTION_END packet
	move.l	(dp_Arg1,a0),d5
.nxt	movea.l	($F0,a3),A2
	bra	.3

.1	movea.l	(14,A2),a0
	cmp.l	(dp_Arg1,a0),d5
	bne	.2
	move.l	a0,d1
	movea.l	a2,a1
	REMOVE
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
	movea.l	A2,A1
	movea.l	(a5),A6
	sys	FreeVec
	bra	.nxt

.2	movea.l	(a2),a2
.3	tst.l	(A2)
	bne.b	.1
	tst.w	d4
	movem.l	(SP)+,d2-d5/A2/A3/A5/A6
	rts

_cw	movem.l	D2-D4/A2/A3/A5/A6,-(sp)
	movea.l	a5,a3
	lea	(7*4+48,sp),a5
	movea.l	(a5),a6
	lea	(KingCONDC1.MSG,pc),A1
	sys	Forbid
	sys	FindSemaphore
	tst.l	D0
	beq.b	\7
	movea.l	D0,a0
	sys	ObtainSemaphore
	subq.w	#1,($66,a0)
	sys	ReleaseSemaphore
\7	sys	Permit
	tst.l	($78,a3)
	beq.b	\8
	movea.l	a3,a0
	bsr	_CloseWindow
\8	move.l	($DC,a3),D0
	beq.b	\9
	move.l	D0,a1
	move.l	a5,a0
	moveq	#17,d0		FreeDiskObject()
	bsr	_l1
\9	moveq	#0,D0
	moveq	#0,D4
\10	move.b	D4,D0
	move.l	($AC,a3,D0.W*4),d0
	beq.b	\11
	movea.l	D0,a2
	movea.l	A2,a1
	bsr	lbC001338
	movea.l	($62,A2),A2
	move.l	(14,A2),D1
	beq.b	\12
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
\12	clr.l	(14,A2)
	movea.l	a3,a0
	bsr	lbC0016EE
\11	addq.b	#1,D4
	moveq	#8,D0
	cmp.b	D0,D4
	bcs.b	\10
	bra.b	\13

\14	movea.l	a1,a2
	REMOVE
	move.l	(14,A2),D1
	beq.b	\15
	movea.l	(4,a5),A6
	moveq	#0,D2
	move.l	D2,D3
	sys	ReplyPkt
\15	movea.l	A2,A1
	movea.l	(a5),A6
	sys	FreeVec
\13	movea.l	($F0,a3),A1
	tst.l	(A1)
	bne.b	\14
	movea.l	($A0,a3),A1
	sys	CloseDevice
	movea.l	($A0,a3),A0
	sys	DeleteIORequest
	tst.l	($CC,a3)
	beq.b	\16
	bra.b	\17

\18	sys	ReplyMsg
\17	movea.l	($CC,a3),A0
	sys	GetMsg
	movea.l	D0,A1
	tst.l	D0
	bne.b	\18
	bsr	_remAppIcon
;	bsr	_remAppMenuItem
	movea.l	(a5),A6
	movea.l	($CC,a3),A0
	sys	DeleteMsgPort
\16	move.l	a3,a0
	move.l	a5,a1
	bsr	lbC00A546
	movea.l	($5C,a3),A0
	lea	($2E,a3),A1
	move.l	A1,-(SP)
	move.l	a5,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	movea.l	a3,A1
	movea.l	(a5),A6
	sys	FreeVec
\19	movem.l	(SP)+,D2-D4/A2/A3/A5/A6
	rts
	endc

lbC00A546	movem.l	A3/A5/A6,-(SP)
	movea.l	a0,A5
	move.l	($90,A5),d0
	beq.b	\1
	movea.l	D0,a3
	movea.l	($52,A3),A0
	pea	($24,A3)
	move.l	a1,-(SP)
	jsr	(A0)
	addq.l	#8,SP
	movea.l	A3,A1
	movea.l	(_exec,A4),A6
	sys	FreeVec
	clr.l	($90,A5)
\1	movem.l	(SP)+,A3/A5/A6
	rts

lbC0031DE	suba.w	#$200,SP
	movem.l	D5-D7/A2/A3/A5,-(SP)
	move.l	d0,D7
	movea.l	a0,A2
	movea.l	d1,A3
	movea.l	a1,A5
	bra.b	\5

\1	move.b	(A2)+,D5
	subq.l	#1,D7
	moveq	#0,D0
	move.b	D5,D0
	bsr	lbC009434
	beq.b	\2
	lea	($18,SP),A0
	moveq	#0,D1
	move.b	D0,D1
;	move.l	D1,-(SP)
	adda.l	D6,A0
;	move.l	A0,-(SP)
	bsr	lbC009A5C
	add.l	D0,D6
	bra.b	\3

\2	addq.l	#1,D6
	lea	($18,SP),A1
	move.b	D5,(-1,A1,D6.L)
\3	cmpi.l	#$1F8,D6
	bgt.b	\4
	tst.l	D7
	bne.b	\6
\4	move.l	D6,d0
	lea	($1C-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
\5	moveq	#0,D6
\6	tst.l	D7
	bne.b	\1
	movem.l	(SP)+,D5-D7/A2/A3/A5
	adda.w	#$200,SP
	rts

lbC00325C	adda.l	d1,A0
	suba.l	D0,A0
	bra.b	\2

\1	move.b	(A1,D0.L),(A1)+
\2	cmpa.l	A0,A1
	bcs.b	\1
	rts

lbC003280	movem.l	A2/A5,-(SP)
	add.l	D1,D0
	movea.l	a0,A2
	adda.l	D0,A2
	movea.l	(12,SP),A5
	bra.b	\2

\1	movea.l	A2,A0
	suba.l	D1,A0
	move.b	(A0),(A2)
	subq.l	#1,A2
\2	cmpa.l	A1,A2
	bhi.b	\1
	bra.b	\4

\3	move.b	(A5)+,(A1)+
\4	subq.l	#1,D1
	bpl	\3
	movem.l	(SP)+,A2/A5
	rtd	#4

lbC0032BC	suba.w	#12,SP
	movem.l	D7/A2/A3/A5,-(SP)
	movea.l	d0,a5
	move.l	a0,D7
	movea.l	a1,A2
	movea.l	d1,a3
;	move.l	a5,-(SP)
	move.l	a3,a0
	bsr	lbC00B96A
	tst.w	D0
	beq.b	\1
	move.l	D7,d0
	lea	($9FC,a5),a0
	move.l	a5,d1
	move.l	a3,a1
	bsr	lbC0031DE
	movea.l	a5,A0
	adda.l	D7,A0
	lea	($9FC,A0),A1
	cmpa.l	A1,A2
	bcc.b	\1
	lea	($9FC,a5),A0
	move.l	A2,D0
	sub.l	A0,D0
	sub.l	D0,D7
	move.l	D7,d1
	lea	($16-4,SP),a0
	bsr	lbC009998
	lea	($1A-8,SP),a0
	move.l	a5,a1
	move.l	a3,d1
	bsr	_doio
\1	movem.l	(SP)+,D7/A2/A3/A5
	adda.w	#12,SP
	rts

lbC003330	suba.w	#$34,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	movea.l	($64,SP),A5
	move.l	($6C,SP),D7
	move.l	($90,A5),D0
	movea.l	($60,SP),A3
	beq.b	lbC003354
	movea.l	A3,a0
	bsr	lbC00B6B2
lbC003354	movea.l	([$98,A5],io_Unit),A2
	move.w	($2A,A2),D0
	ext.l	D0
	addq.l	#1,D0
	divs.l	#$13,D0
	move.l	D0,D6
	bne.b	lbC003372
	moveq	#1,D6
lbC003372	moveq	#0,d1
	lea	($48,SP),a0
	bsr	lbC009A14
	movea.l	A5,A0
	adda.l	D7,A0
	lea	($9FC,A0),A1
	move.l	D0,D4
	move.l	A0,($2C,SP)
	move.l	A1,($30,SP)
	movea.l	($68,SP),A6
	cmpa.l	A1,A6
	bcc.b	lbC0033B6
	lea	($9FC,A5),A1
	move.l	A1,D1
	sub.l	($68,SP),D1
	lea	($48,SP),A0
	add.l	D7,D1
	adda.l	D4,A0
	bsr	lbC0099D6
	add.l	D0,D4
lbC0033B6	addq.l	#1,D4
	lea	($48,SP),A0
	move.b	#10,(-1,A0,D4.L)
	btst	#2,($161,A5)
	beq.b	lbC00342A
	move.l	([$70,SP],14),D0
	add.l	D6,D0
	subq.l	#1,D0
	divs.l	D6,D0
	move.l	D0,D3
	move.w	($2C,A2),D0
	ext.l	D0
	bpl.b	lbC0033E6
	addq.l	#1,D0
lbC0033E6	asr.l	#1,D0
	cmp.l	D3,D0
	blt.b	lbC0033EE
	move.l	D3,D0
lbC0033EE	move.w	($40,A2),D1
	ext.l	D1
	move.w	($2C,A2),D2
	ext.l	D2
	move.l	D0,D3
	sub.l	D2,D3
	add.l	D1,D3
	cmp.l	D0,D3
	blt.b	lbC003406
	move.l	D0,D3
lbC003406	move.l	D3,D5
	ble.b	lbC00342A
	adda.l	D4,A0
	move.l	D5,d1
	bsr	lbC009AA2
	add.l	D0,D4
	lea	($50-8,SP),A0
	adda.l	D4,A0
	move.l	D5,d1
	bsr	lbC009B54
	add.l	D0,D4
lbC00342A	move.l	D4,d0
	lea	($4C-4,SP),a0
	move.l	A5,a1
	move.l	A3,d1
	bsr	_doio
	movea.l	($3C-16,SP),A2
	move.b	#10,($9FC,A2)
	move.l	D7,D0
	addq.l	#1,D0
	lea	($9FC,A5),A0
	move.l	A5,d1
	movea.l	A3,a1
	move.l	A0,($44-12-16,SP)
	bsr	lbC00B3A6
	clr.b	($9FC,A2)
	movea.l	($70,SP),A0
	movea.l	(A0),A2
	moveq	#1,D5
	bra	lbC003510

lbC00346E	cmpi.l	#$12,(14,A2)
	ble.b	lbC00349C
	lea	($12,A2),A0
	lea	($34,SP),A1
	moveq	#15,D0
	movea.l	(_exec,A4),A6
	sys	CopyMem
	moveq	#$2E,D0
	move.b	D0,($45,SP)
	move.b	D0,($44,SP)
	move.b	D0,($43,SP)
	moveq	#$12,D4
	bra.b	lbC0034B4

lbC00349C	lea	($12,A2),A0
	lea	($34,SP),A1
	move.l	(14,A2),D0
	movea.l	(_exec,A4),A6
	sys	CopyMem
	move.l	(14,A2),D4
lbC0034B4	lea	($34,SP),A3
	adda.l	D4,A3
	clr.b	(A3)
	move.l	D5,D0
	divsl.l	D6,D1:D0
	tst.l	D1
	beq.b	lbC0034CC
	movea.l	(A2),A0
	tst.l	(A0)
	bne.b	lbC0034E0
lbC0034CC	addq.l	#1,D4
	move.b	#10,(A3)
	bra.b	lbC0034E6

lbC0034D4	addq.l	#1,D4
	lea	($34,SP),A0
	move.b	#$20,(-1,A0,D4.L)
lbC0034E0	moveq	#$13,D0
	cmp.l	D0,D4
	blt.b	lbC0034D4
lbC0034E6	movea.l	($60,SP),A3
	move.l	D4,d0
	lea	($38-4,SP),a0
	move.l	A5,a1
	move.l	A3,d1
	bsr	_doio
	move.l	D4,d0
	lea	($44-16,SP),a0
	move.l	A5,d1
	move.l	A3,a1
	bsr	lbC00B3A6
	movea.l	A2,A0
	addq.l	#1,D5
	movea.l	(A0),A2
lbC003510	tst.l	(A2)
	bne	lbC00346E
	move.l	($90,A5),D0
	beq.b	lbC003530
	bsr	lbC00B734

lbC00B7A8	movea.l	a5,A1
	movea.l	($90,A1),A0
	move.l	A0,D0
	beq.b	lbC003530
	lea	($D6,A0),A2
	movea.l	($66,A0),A0
	cmpa.l	A2,A0
	bls.b	lbC003530
	move.l	A0,D0
	move.l	A2,D1
	sub.l	D1,D0
;	move.l	D0,-(SP)
	move.l	D1,a0
;	move.l	A1,-(SP)
	move.l	a3,d1
	bsr	_doio
lbC003530	movea.l	($28,SP),A2
	move.l	D7,d0
	movea.l	A2,a0
	move.l	A5,d1
	movea.l	A3,a1
	bsr	lbC0031DE
	movea.l	($68,SP),A0
	cmpa.l	($30,SP),A0
	bcc.b	lbC003566
	move.l	A2,D1
	sub.l	($68,SP),D1
	add.l	D7,D1
	lea	($4C-4,SP),a0
	bsr	lbC009998
	move.l	D0,D5
	bra.b	lbC003568

lbC003566	moveq	#0,D5
lbC003568	lea	($48,SP),A0
	moveq	#1,d1
	adda.l	D5,A0
	bsr	lbC009A14
	add.l	D0,D5
	move.l	D5,d0
	lea	($4c-4,SP),a0
	move.l	A5,a1
	move.l	A3,d1
	bsr	_doio
	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$34,SP
	rts

lbC003596	movem.l	d2/A2/A3,-(SP)
	move.l	d0,d2
	movea.l	a1,a3
	moveq	#0,D0
	moveq	#'"',D1
	bra.b	\3

\1	cmp.b	(A1),D1
	bne.b	\2
	addq.l	#1,D0
	movea.l	A1,A2
\2	addq.l	#1,A1
\3	cmpa.l	A0,A1
	bcs.b	\1
	movea.l	d2,a1
;	moveq	#2,d1
;	divsl.l	d1,D1:D0
	asr.l	d0
;	subq.l	#1,D1
;	bne.b	\4
	bcc	\4
	move.l	A1,D0
	beq.b	\8
	move.l	A2,(A1)
	bra.b	\8

\4	move.l	A1,D0
	beq.b	\5
	clr.l	(A1)
\5	movea.l	A0,A2
\6	subq.l	#1,A2
	moveq	#0,D0
	move.b	(A2),D0

\7	moveq	#' ',D1
	cmp.b	d0,d1
	beq.b	\8
	moveq	#',',D1
	cmp.b	D0,D1
	beq.b	\8
	moveq	#'>',D1
	cmp.b	D0,D1
	beq.b	\8
	moveq	#'<',D1
	cmp.b	D0,D1
	beq.b	\8
	moveq	#'`',D1
	cmp.b	D0,D1
	beq.b	\8
	cmpa.l	A3,A2
	bcc.b	\6
\8	addq.l	#1,A2
	move.l	A2,D0
	movem.l	(SP)+,d2/A2/A3
	rts

lbC0035FA	movem.l	D6/D7/A2/A3/A5/A6,-(SP)
	movea.l	($30,SP),A5
	move.w	($2E,SP),D7
	movea.l	($20,SP),A3
	lea	($4C,A5),a0
	move.l	a0,d0
	movea.l	($28-4,SP),a0
	adda.w	#$9FC,A3
	movea.l	A3,a1
	bsr	lbC003596
	clr.w	($50,A5)
	movea.l	($24,SP),A0
	move.b	(A0),D6
	clr.b	(A0)
	cmp.l	A3,D0
	movea.l	D0,A2
	bls.b	\1
	tst.w	D7
	beq.b	\2
\1	cmpi.w	#$3EE,D7
	bne.b	\5
\2	move.l	A0,D0
	move.l	A2,D1
	sub.l	D1,D0
	movea.l	($1C,SP),A3
	move.l	A5,-(SP)
	move.l	D0,-(SP)
	move.l	D1,-(SP)
	move.l	($2C,SP),-(SP)
	move.l	A3,-(SP)
	bsr	lbC0080DC
	lea	($14,SP),SP
	move.l	A2,($48,A5)
	move.l	D0,D7
	beq.b	\3
	move.l	A2,D1
	movea.l	(4,A3),A6
	jsr	(-$366,A6)
\3	move.l	D0,($40,A5)
\4	move.b	#2,($3E,A5)
	bra.b	\9

\5	movea.l	($20,SP),A1
	cmpa.l	A2,A3
	bne.b	\6
	tst.w	D7
	beq.b	\7
\6	cmpi.w	#$3F0,D7
	bne.b	\8
\7	move.l	($24,SP),D0
	sub.l	a2,D0
	move.l	A5,-(SP)
	move.l	D0,-(SP)
	move.l	a2,-(SP)
	move.l	A1,-(SP)
	move.l	($2C,SP),-(SP)
	bsr	lbC008410
	movea.l	A2,A0
	lea	($14,SP),SP
	move.l	D0,D7
	move.l	A0,($48,A5)
	move.l	A0,($40,A5)
	move.b	#1,($3E,A5)
	bra.b	\9

\8	move.l	($24,SP),D0
	move.l	A2,D1
	sub.l	D1,D0
	move.l	A5,-(SP)
	move.l	D0,-(SP)
	move.l	D1,-(SP)
	move.l	A1,-(SP)
	move.l	($2C,SP),-(SP)
	bsr	lbC00828C
	movea.l	A2,A0
	move.l	D0,D7
	move.l	A0,($48,A5)
	move.l	A0,($40,A5)
	move.b	#3,($3E,A5)
\9	movea.l	($24,SP),A3
	move.b	D6,(A3)
	movea.l	($1c,SP),a0
	movem.l	D7/A2/A3,-(SP)
	movea.l	a0,A3
	moveq	#0,D7
\11	movea.l	(A5),A2
	tst.l	(A2)
	beq.b	\12
	cmp.l	(14,A2),D7
	bge.b	\12
	move.b	($12,A2,D7.L),D0
	extb.l	D0
	movea.l	($1C,A3),A6
	jsr	(-$AE,A6)
	move.b	D0,D6
	bra.b	\14

\12	move.l	D7,D0
	bra.b	\16

\13	cmp.l	(14,A2),D7
	bge.b	\15
	move.b	($12,A2,D7.L),D0
	extb.l	D0
	movea.l	($1C,A3),A6
	jsr	(-$AE,A6)
	cmp.b	D6,D0
	bne.b	\15
\14	movea.l	(A2),A2
	tst.l	(A2)
	bne.b	\13
	addq.l	#1,D7
	bra.b	\11

\15	move.l	D7,D0
\16	movem.l	(SP)+,D7/A2/A3
	move.l	D0,($36,A5)
	move.l	A3,D0
	move.l	D0,D1
	movea.l	D0,A0
	subq.l	#1,A0
	sub.l	A2,D1
	move.l	D1,($3A,A5)
	move.l	A0,($44,A5)
	move.l	D7,D0
	movem.l	(SP)+,D6/D7/A2/A3/A5/A6
	rts

lbC003720	suba.w	#$78,SP
	movem.l	D2/D4/D6/D7/A2/A3/A5,-(SP)
	movea.l	($AC,SP),A5
	move.w	($AA,SP),D4
;	move.b	#$20,($78,SP)
;	move.b	#$22,($77,SP)
	move.w	#$2220,($77,SP)
	movea.l	($A4,SP),A2
	movea.l	($9C,SP),A3
	move.l	(A2),a0
	move.l	([$A4-4,SP]),a1
	move.l	A3,d0
	move.l	($A4-12,SP),d1
	bsr	lbC0032BC
	tst.l	($32,A5)
	bne.b	\1
	move.l	A5,-(SP)
	move.l	D4,-(SP)
	move.l	(A2),-(SP)
	move.l	([$AC,SP]),-(SP)
	move.l	A3,-(SP)
	move.l	($AC,SP),-(SP)
	bsr	lbC0035FA
	lea	($18,SP),SP
	tst.l	D0
	beq	\38

\1	moveq	#1,D0
	cmp.l	(14,A5),D0
	bne.b	\2
	movea.l	(A5),A0
	lea	($12,A0),A2
	move.l	(14,A0),D7
	clr.l	($32,A5)
	bra	\28

\2	tst.l	($32,A5)
	bne.b	\4
	move.l	([$A0,SP]),D0
	sub.l	($40,A5),D0
	move.l	($36,A5),D1
	cmp.l	D0,D1
	ble.b	\4
	btst	#3,($17F,A3)
	beq.b	\4
	movea.l	(A5),A2
	move.l	D1,D7
	moveq	#5,D0
	and.w	($17E,A3),D0
	adda.w	#$12,A2
	beq.b	\3
	move.l	A5,($32,A5)
	bra	\28

\3	clr.l	($32,A5)
	bra	\28

\4	move.b	($3E,A5),D0
	btst	#0,($17F,A3)
	bne.b	\5
	cmpi.w	#$413,D4
	bne	\16
\5	cmpi.w	#$3EE,D4
	beq.b	\6
	cmpi.w	#$413,D4
	bne.b	\7
	moveq	#2,D1
	cmp.b	D1,D0
	bne.b	\7
\6	lea	(Selectfilenam.MSG,pc),A1
	bra.b	\10

\7	cmpi.w	#$3EF,D4
	beq.b	\8
	cmpi.w	#$413,D4
	bne.b	\9
	moveq	#3,D1
	cmp.b	D1,D0
	bne.b	\9
\8	lea	(Selectdevice.MSG,pc),A1
	bra.b	\10

\9	lea	(Selectcommand.MSG,pc),A1
\10	bsr	_getstr
	movea.l	a1,a2
	moveq	#2,D0
	cmp.b	($3E,A5),D0
	bne	\13
	tst.l	($3A,A5)
	bne	\13
	move.l	A3,a1
	move.l	($98,SP),a0
	bsr	lbC0071B4
	move.l	D0,D6
	beq.b	\11
	move.l	D6,a1
	move.l	($98,SP),a0
	bsr	_CurrentDir
\11	pea	($26,SP)
	move.l	A2,a1
	moveq	#$7B,d0
	move.l	A3,d1
	move.l	($9c,SP),a0
	bsr	_filereq
	move.l	D0,D7
	moveq	#$4F,D1
	cmp.l	D1,D0
	bge.b	\12
	tst.l	D7
	ble.b	\12
	lea	($26,SP),A3
	adda.l	D7,A3
	move.b	(-1,A3),D0
	moveq	#$2F,D1
	cmp.b	D1,D0
	beq.b	\12
	moveq	#$3A,D1
	cmp.b	D1,D0
	beq.b	\12
	addq.l	#1,D7
	move.b	#$20,(A3)
	clr.b	($26,SP,D7.L)
\12	lea	($26,SP),A2
	tst.l	D6
	beq.b	\15
	movea.l	($98,SP),A3
	suba.l	a1,a1
	move.l	A3,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A3,a0
	bsr	_UnLock
	bra.b	\15

\13	move.l	A2,-(SP)
	move.l	($32,A5),-(SP)
	move.l	A5,-(SP)
	move.l	A3,-(SP)
	move.l	($A8,SP),-(SP)
	bsr	lbC007932
;	illegal
	tst.l	D0
	beq.b	\14
	movea.l	D0,A0
	lea	($12,A0),A2
	move.l	(14,A0),D7
	bra.b	\15

\14	moveq	#0,D6
	bra	.36

\15	clr.l	($32,A5)
	bra	\28

\16	btst	#2,($17F,A3)
	beq.b	\26
	btst	#1,($17F,A3)
	beq.b	\18
	move.l	($32,A5),D0
	beq.b	\17
	cmp.l	A5,D0
	bne.b	\18
\17	move.l	A5,-(SP)
	move.l	(A2),-(SP)
	move.l	([$A8,SP]),-(SP)
	move.l	A3,-(SP)
	move.l	($A8,SP),-(SP)
	bsr	lbC003330
	lea	($14,SP),SP
\18	movea.l	($32,A5),A2
	move.l	A2,D0
	bne.b	\19
	movea.l	(A5),A3
	bra.b	\22

\19	cmpi.w	#$414,D4
	bne.b	\20
	movea.l	(A2),A3
	bra.b	\22

\20	cmpi.w	#$415,D4
	bne.b	\21
	movea.l	(4,A2),A3
	bra.b	\22

\21	movea.l	A2,A3
\22	move.l	A3,D0
	beq.b	\23
	tst.l	(4,A3)
	bne.b	\24
\23	movea.l	(8,A5),A3
	bra.b	\25

\24	tst.l	(A3)
	bne.b	\25
	movea.l	(A5),A3
\25	lea	($12,A3),A2
	move.l	(14,A3),D7
	move.l	A3,($32,A5)
	bra.b	\28

\26	btst	#1,($17F,A3)
	beq.b	\27
	move.l	A5,-(SP)
	move.l	(A2),-(SP)
	move.l	([$A8,SP]),-(SP)
	move.l	A3,-(SP)
	move.l	($A8,SP),-(SP)
	bsr	lbC003330
	lea	($14,SP),SP
\27	moveq	#0,D6
	bra	.36

\28	move.l	($44,A5),D0
	sub.l	($40,A5),D0
	movea.l	($9C,SP),A3
	addq.l	#1,D0
	move.l	([$A8-4,SP]),d1
	adda.w	#$9FC,A3
	movea.l	($40,A5),a1
	movea.l	A3,a0
	move.l	A3,($30-16,SP)
	bsr	lbC00325C
	move.l	D0,D6
	neg.l	D6
	movea.l	($A4,SP),A0
	add.l	D6,(A0)
	move.l	#$1FF,D2
	sub.l	(A0),D2
	cmp.l	D7,D2
	blt.b	\29
	move.l	D7,D2
\29	add.l	D2,D6
	add.l	D2,(A0)
	move.l	d2,d1
	move.l	(A0),d0
	movea.l	($40,A5),a1
	movea.l	A3,a0
	move.l	A2,-(SP)
	bsr	lbC003280
	movea.l	($40,A5),A0
	adda.l	D2,A0
	movea.l	A0,A2
	subq.l	#1,A2
	movea.l	($48,A5),A1
	move.l	A0,($1C,SP)
	moveq	#'"',d0
	cmp.b	(-1,a1),d0
	bne	.30
	moveq	#1,d4
	bra	\32

.30	moveq	#$20,D0	
	moveq	#0,d4
	bra.b	\31

\30	cmp.b	(A1)+,D0
	bne.b	\31
	moveq	#1,d4
	bra	\32

\31	cmpa.l	A2,A1
	bcs.b	\30
\32	movea.l	($40,A5),A2
	movea.l	($A4,SP),A3
	tst.b	d4
	beq	\34
	move.l	(A3),D0
	cmpi.l	#$1FE,D0
	bge	\34
	movea.l	($1C,SP),A0
	moveq	#$20,D0
	cmp.b	-(A0),D0
	bne.b	\33
	move.b	#$22,(A0)
	movea.l	a2,A1
	moveq	#1,d1
	adda.l	D2,A1
	move.l	(A3),d0
	move.l	($2C-12,SP),a0
	pea	($88-16,SP)
	bsr	lbC003280
	addq.l	#1,(A3)
	addq.l	#1,D6
	addq.l	#1,D2
\33	tst.l	($4C,A5)
	bne	\35
	movea.l	($A4,SP),A2
	moveq	#1,d1
	move.l	(A2),d0
	lea	($48,A5),A3
	movea.l	(A3),a1
	movea.l	($2C-12,SP),a0
	pea	($87-16,SP)
	bsr	lbC003280
	addq.l	#1,(A2)
	addq.l	#1,D6
	move.l	($40,A5),D1
	sub.l	(A3),D1
	movea.l	(A3)+,A2
	movea.l	A2,A0
	add.l	D1,D2
	addq.l	#1,D2
	move.l	A0,(A3)+
	addq.l	#1,($48,A5)
	addq.l	#1,($40,A5)
	moveq	#1,D0
	move.w	D0,(A3)+
	bra.b	\35

\34	tst.b	D4
	bne.b	\35
	tst.w	($50,A5)
	beq.b	\35
	moveq	#1,d0
	move.l	(A3),d1
	movea.l	($4C,A5),a1
	movea.l	($2C-12,SP),a0
	bsr	lbC00325C
	subq.l	#1,(A3)
	lea	($48,A5),A3
	movea.l	(A3),A0
	subq.l	#1,A0
	subq.l	#1,D6
	move.l	A0,(A3)+
	movea.l	($40,A5),A1
	subq.l	#1,A1
	move.l	A1,($40,A5)
	clr.w	($50,A5)
	clr.l	(A3)+
	move.l	A1,D0
	move.l	A0,D1
	sub.l	D1,D0
	movea.l	D1,A2
	add.l	D0,D2
\35	movea.l	([$A0,SP]),A0
	cmpa.l	A2,A0
	movea.l	($9C,SP),A3
	bls.b	\36
	move.l	A0,D1
	sub.l	A2,D1
	lea	($80-4,SP),a0
	bsr	lbC009998
	lea	($84-8,SP),a0
	move.l	A3,a1
	move.l	($A8-16,SP),d1
	bsr	_doio
\36	move.l	D2,d0
	movea.l	A2,a0
	move.l	A3,d1
	movea.l	($A4-12,SP),a1
	bsr	lbC0031DE
	movea.l	($A0,SP),A1
	adda.l	D2,A2
	move.l	A2,(A1)
	subq.l	#1,A2
	move.l	A2,($44,A5)
	tst.l	($32,A5)
	bne.b	\37
.36	movea.l	($22,A5),A0
	move.l	A5,-(SP)
	move.l	($9C,SP),-(SP)
	jsr	(A0)
	addq.l	#8,SP
	clr.l	($32,A5)
\37	move.l	D6,D0
\38	movem.l	(SP)+,D2/D4/D6/D7/A2/A3/A5
	adda.w	#$78,SP
	rts

lbC003BA8	suba.w	#$18,SP
	movem.l	D7/A2/A3/A5/A6,-(SP)
	move.l	($3C,SP),D0
	movea.l	($40,SP),A2
	movea.l	($38,SP),A3
	movea.l	($34,SP),A5
	move.l	D0,a0
	move.l	A3,a1
	move.l	A5,d0
	move.l	($3C-12,SP),d1
	bsr	lbC0032BC
	lea	($9FC,A5),A0
	move.l	A0,($14,SP)
	cmpa.l	A0,A3
	bls.b	lbC003C00
	move.l	A3,D1
	sub.l	A0,D1
	lea	($1C-4,SP),a0
	bsr	lbC009998
	lea	($20-8,SP),a0
	move.l	A5,a1
	move.l	($40-16,SP),d1
	bsr	_doio
lbC003C00	tst.l	(4,A2)
	beq.b	lbC003C0A
	tst.l	(A2)
	bne.b	lbC003C0E
lbC003C0A	moveq	#0,D7
	bra.b	lbC003C3C

lbC003C0E	move.l	(14,A2),d0
	lea	($12,A2),a0
	move.l	A5,d1
	movea.l	($3C-12,SP),a1
	bsr	lbC0031DE
	movea.l	($14,SP),A1
	lea	($12,A2),A0
	move.l	(14,A2),D0
	movea.l	(_exec,A4),A6
	sys	CopyMem
	move.l	(14,A2),D7
lbC003C3C	move.l	D7,D0
	movem.l	(SP)+,D7/A2/A3/A5/A6
	adda.w	#$18,SP
	rts

lbC003C84	movem.l	A2/A3,-(SP)
	movea.l	A0,A2
	movea.l	A2,A3
	adda.l	d0,A3
	subq.l	#1,A1
	subq.l	#1,A3
	cmpa.l	A3,A1
	bcc.b	\1
	movea.l	A1,A0
	bra.b	\2

\1	movea.l	A3,A0
\2	cmpa.l	A0,A2
	bls.b	\3
	movea.l	A2,A0
	bra.b	\5

\3	cmpa.l	A3,A1
	bcc.b	\4
	movea.l	A1,A0
	bra.b	\5

\4	movea.l	A3,A0
\5	movea.l	A0,A3
	bra.b	\7

\6	subq.l	#1,A3
\7	moveq	#$20,D0
	cmp.b	(A3),D0
	bne.b	\8
	cmpa.l	A2,A3
	bhi.b	\6
\8	cmpa.l	A2,A3
	bls.b	\12
	bra.b	\10

\9	subq.l	#1,A3
\10	cmp.b	(A3),D0
	beq.b	\11
	cmpa.l	A2,A3
	bhi.b	\9
\11	cmp.b	(A3),D0
	bne.b	\12
	addq.l	#1,A3
\12	move.l	A3,D0
	movem.l	(SP)+,A2/A3
	rts

lbC003CE6	move.l	A3,-(SP)
;	move.l	(12,SP),D1
;	movea.l	(8,SP),A1
	movea.l	a0,A3
	bra.b	\2

\1	addq.l	#1,A3
\2	moveq	#$20,D0
	cmp.b	(A3),D0
	beq.b	\4
	movea.l	A1,A0
	adda.l	D1,A0
	cmpa.l	A0,A3
	bcs.b	\1
	bra.b	\4

\3	addq.l	#1,A3
\4	moveq	#$20,D0
	cmp.b	(A3),D0
	bne.b	\5
	movea.l	A1,A0
	adda.l	D1,A0
	cmpa.l	A0,A3
	bcs.b	\3
\5	move.l	A3,D0
	movea.l	(SP)+,A3
	rts

lbC003D1E	movem.l	A2/A3,-(SP)
	movea.l	A0,A2
	movea.l	A2,A3
	adda.l	($1b4,sp),A3
	movea.l	($1b8,sp),A1
	subq.l	#1,A1
	subq.l	#1,A3
	cmpa.l	A3,A1
	bcc.b	\1
	movea.l	A1,A0
	bra.b	\2

\1	movea.l	A3,A0
\2	cmpa.l	A0,A2
	bls.b	\3
	movea.l	A2,A0
	bra.b	\5

\3	cmpa.l	A3,A1
	bcc.b	\4
	movea.l	A1,A0
	bra.b	\5

\4	movea.l	A3,A0
\5	movea.l	A0,A3
	bra.b	\7

\6	subq.l	#1,A3
\7	move.b	(A3),D0
	bsr	lbC00681A
	beq.b	\8
	cmpa.l	A2,A3
	bhi.b	\6
\8	cmpa.l	A2,A3
	bls.b	\12
	bra.b	\10

\9	subq.l	#1,A3
\10	move.b	(A3),D0
	bsr	lbC00681A
	bne.b	\11
	cmpa.l	A2,A3
	bhi.b	\9
\11	move.b	(A3),D0
	bsr	lbC00681A
	beq.b	\12
	addq.l	#1,A3
\12	move.l	A3,D0
	movem.l	(SP)+,A2/A3
	rts

lbC003D9E	movem.l	D7/A3/A5,-(SP)
	move.l	d1,D7
	movea.l	a1,A5
	movea.l	a0,A3
	bra.b	\2

\1	addq.l	#1,A3
\2	move.b	(A3),D0
	bsr	lbC00681A
	bne.b	\4
	movea.l	A5,A0
	adda.l	D7,A0
	cmpa.l	A0,A3
	bcs.b	\1
	bra.b	\4

\3	addq.l	#1,A3
\4	move.b	(A3),D0
	bsr	lbC00681A
	beq.b	\5
	movea.l	A5,A0
	adda.l	D7,A0
	cmpa.l	A0,A3
	bcs.b	\3
\5	move.l	A3,D0
	movem.l	(SP)+,D7/A3/A5
	rts

lbC003DEE	suba.w	#$10,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	move.w	($52,SP),D5
	move.w	($4E,SP),D6
	move.l	($48,SP),D7
	moveq	#0,D4
	tst.w	D5
	movea.l	($44,SP),A2
	movea.l	($40,SP),A3
	movea.l	($3C,SP),A5
	beq.b	lbC003E2A
	movea.l	A3,A0
	adda.l	D7,A0
	lea	($9FC,A0),a1
	move.l	D7,a0
	move.l	A3,d0
	move.l	A5,d1
	bsr	lbC0032BC
lbC003E2A	moveq	#$1C,D0
	cmp.w	D0,D6
	beq	lbC003ECE
	tst.l	D7
	ble.b	lbC003E7C
;	btst	#7,($162,A3)
;	bne.b	lbC003E7C
	tst.b	($162,A3)
	bmi	lbC003E7C
	move.l	($3C,A3),D0
	ble.b	lbC003E66
	movea.l	($36,A3),A0
	move.l	(14,A0),D0
	cmp.l	D7,D0
	bne.b	lbC003E66

lbC00DCEA	move.l	d7,D0
	beq.b	lbC003E7C
	lea	($12,a0),A0	;current entry
	lea	($9FC,A3),a1		;last entry
	subq.l	#1,D0
lbC00DCFC	move.b	(A0)+,D1
	cmp.b	(A1)+,D1
	bne.b	lbC00DD0E		;they don't match: add to history
	tst.b	D1
	beq.b	lbC00DD0A
	subq.l	#1,D0
	bpl.b	lbC00DCFC
lbC00DD0A	moveq	#0,D0
	bra.b	lbC003E7C

lbC00DD0E	bgt.b	lbC00DD14
	moveq	#-1,D0
	bra	lbC003E66

lbC00DD14	moveq	#1,D0

lbC003E66	movea.l	($4C,A3),A0
	move.l	D7,-(SP)
	pea	($9FC,A3)
	pea	($2E,A3)
	move.l	A5,-(SP)
	jsr	(A0)
	lea	($10,SP),SP
lbC003E7C	tst.w	D5
	beq.b	lbC003EC0
	movea.l	A3,A0
	adda.l	D7,A0
	lea	($9FC,A0),A1
	cmpa.l	A1,A2
	bcc.b	lbC003EA4
	lea	($9FC,A3),A0
	move.l	A0,D1
	sub.l	A2,D1
	add.l	D7,D1
	lea	($2D-4,SP),a0
	bsr	lbC0099D6
	move.l	D0,D4
lbC003EA4	move.l	D4,D0
	addq.l	#1,D0
	move.b	#10,($29,SP,D4.L)
;	move.l	D0,-(SP)
	lea	($2D-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
lbC003EC0	movea.l	A3,A0
	adda.l	D7,A0
	addq.l	#1,D7
	moveq	#10,D0
	move.b	D0,($9FC,A0)
	bra.b	lbC003EDE

lbC003ECE	movea.l	A3,A0
	adda.l	D7,A0
	addq.l	#1,D7
	clr.b	($9FC,A0)
	move.w	#1,($106,A3)
lbC003EDE	cmpi.w	#$40A,D6
	bne.b	lbC003EEC
	move.b	#10,($9FC,A3)
	moveq	#1,D7
lbC003EEC	move.l	($10A,A3),D0
	move.l	($10E,A3),D1
	cmp.l	D0,D1
	bge.b	lbC003F04
	moveq	#$40,D2
	lsl.l	#5,D2
	move.l	D2,D3
	sub.l	D0,D3
	add.l	D1,D3
	bra.b	lbC003F08

lbC003F04	sub.l	D0,D1
	move.l	D1,D3
lbC003F08	add.l	D7,D3
	cmpi.l	#$800,D3
	bls.b	lbC003F20
	suba.l	A0,A0
	movea.l	(8,A5),A6
	moveq	#0,D7
	jsr	(-$60,A6)
	bra.b	lbC003F32

lbC003F20	move.l	D7,d0
	lea	($9FC,A3),a0
	movea.l	A3,a1
	bsr	lbC0098BA
lbC003F32	tst.w	D5
	beq.b	lbC003F48
	move.l	D7,d0
	lea	($9FC,A3),a0
	move.l	A3,d1
	move.l	A5,a1
	bsr	lbC00B3A6
lbC003F48	move.l	D7,D0
	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$10,SP
	rts

lbC003F54	movem.l	D7/A3/A6,-(SP)	handle breaks (ctrl-c\d\e\f)
	movea.l	(_exec,A4),A6
	moveq	#9,d7
	add.b	d0,D7
	lea	($f0,a0),A3	read packet list
	sys	Forbid
	IFEMPTY	a3,\empty
	movea.l	(a3),a3
	movea.l	(14,a3),a3	packet
	movea.l	(dp_Port,a3),a3
	move.l	(MP_SIGTASK,a3),d0
	bsr	_ChkTask
	tst.l	d0
	beq	\empty
	movea.l	d0,a1
	moveq	#1,D0
	lsl.l	D7,D0
	sys	Signal
	bra	\1

\empty	movea.l	($64,A0),A3	openers list
	bra	.1

\loop	move.l	($22,A3),d0
	bsr	_ChkTask
	move.l	D0,($22,A3)
	bne	\ok
	move.l	($1e,a3),d0
	move.l	D0,($22,A3)
\ok	movea.l	D0,A1
	moveq	#1,D0
	lsl.l	D7,D0
	sys	Signal
	movea.l	(a3),a3
.1	tst.l	(a3)
	bne	\loop
\1	sys	Permit
	movem.l	(SP)+,D7/A3/A6
	rts

lbC003FD8	suba.w	#$4C,SP
	movem.l	D2/D4-D7/A2/A3/A5/A6,-(SP)
	lea	($78,SP),A6
	movea.l	(A6)+,A3
	movea.l	($84,SP),A2
	moveq	#0,D6
	moveq	#0,D4
	clr.w	($5A,SP)
	movea.l	(A6)+,A5
	movea.l	(A5),A0
	move.b	(A0)+,D0
	move.l	a0,(a5)
	movea.l	(A6)+,A0
	subq.l	#1,(A0)
	move.b	D0,($28,SP)
	subq.b	#2,D0
	beq	lbC004578
	subq.b	#2,D0
	beq	lbC0045B4
	subq.b	#5,D0
	beq	lbC00457E
	moveq	#9,D1
	sub.b	D1,D0
	beq	lbC00459C
	subq.b	#1,D0
	beq	lbC0045A2
	subq.b	#5,D0
	beq	lbC004578
	subi.b	#$83,D0
	bne	lbC0045C6
	movea.l	(A5),A0
	bsr	lbC00936C
	movea.l	($80,SP),A0
	move.l	(A5),D1
	move.l	(A0),D2
	sub.l	D0,D2
	add.l	D1,D2
	subq.l	#1,D2
	move.l	D2,(A0)
	movea.l	D0,A0
	move.b	(A0),D1
	move.l	D0,($2A,SP)
	moveq	#$40,D0
	sub.b	D0,D1
	beq	lbC004514
	subq.b	#1,D1
	beq	lbC004520
	subq.b	#1,D1
	beq	lbC004532
	subq.b	#1,D1
	beq	lbC004502
	subq.b	#1,D1
	beq	lbC0044FC
	moveq	#15,D0
	sub.b	D0,D1
	beq	lbC00450E
	subq.b	#1,D1
	beq	lbC004508
	subq.b	#6,D1
	beq	lbC004538
	moveq	#$1C,D0
	sub.b	D0,D1
	beq	lbC004556
	subq.b	#6,D1
	bne	lbC00456A
	suba.w	#24,SP
	movea.l	SP,a0
	movem.l	A2/A3/A5,-(SP)
	movea.l	(a5),a5
	moveq	#6,D7
	movea.l	a0,A3
	bra.b	\2

\1	cmpi.b	#$39,(A5)+
	bls.b	\1
	lea	(-2,A5),a0
	movea.l	A2,a1
	bsr	ascii2long
	move.l	D0,(A3)+
\2	movea.l	a5,a2
	dbf	d7,\1
	movem.l	(SP)+,A2/A3/A5

	movea.l	a2,A1		construct embedded InputEvent struct
	clr.l	(A1)+		ie_NextEvent
	movea.l	SP,A0
	move.l	(A0)+,D0	class
	move.b	D0,(A1)+	ie_Class
	move.l	(A0)+,D1
	move.b	D1,(A1)+	ie_SubClass
	move.l	(A0)+,D1
	move.w	D1,(A1)+	ie_Code
	move.l	(A0)+,D1
	move.w	D1,(A1)+	ie_Qualifier
	move.l	(A0)+,D1
	move.w	D1,(A1)+	ie_EventAddress (ie_X)
	move.l	(A0)+,D1
	move.w	D1,(A1)+	ie_Y
	adda.w	#24,SP
	subq.b	#1,d0
	beq	_rawkey		1
	subq.b	#1,d0
	beq	_rawmouse	2
	subq.b	#8,d0
	beq	_menu		10
	subq.b	#1,d0
	beq	_closeg		11
	subq.b	#1,d0
	bne	lbC00456A	12

_winr	move.w	#$3E8,D6	window resized
	bra	lbC00456A

_rawkey	movea.l	($9C,A3),A6

*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
	ifd	newmouse
	lea	(ie_Code,a2),a0
	move.w	(A0),D1
	subi.w	#NM_WHEEL_UP,d1
	bcs	\1
	bne	\dwn
	moveq	#$4c,d0		CURSOR_UP
.1	bset	#3,(ie_Qualifier+1,A2)	fake CTRL
.2	move.w	d0,(a0)		simulate cursor
	bra	\1

\dwn	moveq	#$4d,d0		CURSOR_DOWN
	subq.w	#NM_WHEEL_DOWN-NM_WHEEL_UP,d1
	beq	.1
	moveq	#$4c,d0		CURSOR_UP
	subq.w	#NM_WHEEL_LEFT-NM_WHEEL_DOWN,d1
	beq	.2
	moveq	#$4d,d0		CURSOR_DOWN	
	subq.w	#NM_WHEEL_RIGHT-NM_WHEEL_LEFT,d1
	beq	.2
	endc
*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

\1	move.l	A2,-(SP)
	movea.l	A2,A0
	movea.l	([$98,A3],io_Unit),A2	ConUnit
	adda.w	#cu_KeyMapStruct,A2
	movea.l	(io_Device,A6),A6	console.device lib ptr
	moveq	#$28,D1
	lea	($36,SP),A1
	sys	RawKeyConvert
	movea.l	(SP)+,A2
	move.l	D0,($5C,SP)
	move.w	(ie_Code,A2),D0
	moveq	#$41,D1
	move.w	(ie_Qualifier,A2),D5
	sub.w	D1,D0
	blt	lbC004282
	cmpi.w	#15,D0
	bge	lbC004282
	move.w	(lbW00412E,PC,D0.W*2),D0
	move.w	d5,d1
	andi.w	#$30,d1
	jmp	(lbW004130,PC,D0.W)

lbW00412E	dc.w	_bs-lbW004130		41 backspace
lbW004130	dc.w	_tab-lbW004130		42 tab
	dc.w	_enter-lbW004130		43 enter (pad)
	dc.w	_enter-lbW004130		44 return
	dc.w	lbC004282-lbW004130		45 esc
	dc.w	_del-lbW004130			46 del
	dc.w	lbC004282-lbW004130		47 n\u
	dc.w	lbC004282-lbW004130		48 n\u
	dc.w	lbC004282-lbW004130		49 n\u
	dc.w	lbC004282-lbW004130		4a - (pad)
	dc.w	lbC004282-lbW004130		4b n\u
	dc.w	_crsru-lbW004130		4c cursor up
	dc.w	_crsrd-lbW004130		4d cursor down
	dc.w	_crsrr-lbW004130		4e cursor right
	dc.w	_crsrl-lbW004130		4f cursor left

_crsrl	beq.b	\1
	move.w	#$3FB,D6
	bra	lbC0042B8

\1	btst	#3,D5		CTRL?
	beq	lbC0042B8
	move.w	#$40F,D6
	bra	lbC0042B8

_crsrr	beq.b	\1
	move.w	#$3FA,D6
	bra	lbC0042B8

\1	btst	#3,D5		CTRL?
	beq	lbC0042B8
	move.w	#$40E,D6
	bra	lbC0042B8

_crsru	beq.b	\1
	move.w	D5,D0
	move.w	#$402,D6
	andi.w	#3,D0		shifts?
	beq	lbC0042B8
	addq.w	#2,D6
	bra	lbC0042B8

\1	btst	#3,D5		CTRL?
	beq	lbC0042B8
	move.w	#$410,D6
	bra	lbC0042B8

_crsrd	beq.b	\1
	move.w	D5,D0
	move.w	#$403,D6
	andi.w	#3,D0		shifts?
	beq	lbC0042B8
	addq.w	#2,D6
	bra	lbC0042B8

\1	btst	#3,D5		CTRL?
	beq	lbC0042B8
	move.w	#$411,D6
	bra	lbC0042B8

_tab	beq	lbC0042B8
	moveq	#$48,D0
	lsl.l	#6,D0
	and.l	($160,A3),D0
	bne	lbC0042B8
	move.w	#$3F0,D6
	bra	lbC0042B8

_bs	btst	#3,D5		CTRL?
	beq.b	\1
	move.w	#$40D,D6
	bra	lbC0042B8

\1	tst.w	d1
	beq.b	\2
	moveq	#$17,D6
	bra	lbC0042B8

\2	move.w	D5,D0
	andi.w	#3,D0		shifts?
	beq.b	lbC0042B8
	moveq	#$15,D6
	bra.b	lbC0042B8

_del	btst	#3,D5		CTRL?
	beq.b	\1
	move.w	#$40C,D6
	bra.b	lbC0042B8

\1	tst.b	D5		RAMIGA?
	bpl	\2
	move.w	#$409,D6
	bra.b	lbC0042B8

\2	tst.w	d1
	beq.b	\3
	move.w	#$407,D6
	bra.b	lbC0042B8

\3	move.w	D5,D0
	andi.w	#3,D0		shifts?
	beq.b	lbC0042B8
	moveq	#11,D6
	bra.b	lbC0042B8

_enter	beq.b	lbC0042B8
	move.w	#$40A,D6
	bra.b	lbC0042B8

lbC004282	tst.b	d5	(btst	#7,D5)	RAMIGA?
	bpl.b	lbC0042B8
	tst.l	($5C,SP)
	ble.b	lbC0042B8
;	moveq	#0,D0
	move.b	($32,SP),D0	'cooked' key
	ori.b	#%00100000,d0	make lowercase
	moveq	#'c',D1
	cmp.b	D1,D0
	bne	\v
	move.w	#$401,D6	c
	bra.b	lbC0042B8

\v	moveq	#'v',D1
	cmp.b	D1,D0
	bne	lbC0042B8
	move.w	#$3FE,D6	v
lbC0042B8	tst.w	D6
	bne	lbC00456A
	lea	($32,SP),A0
	move.l	A0,($6C,SP)
	movea.l	($74,SP),A5
	bra.b	lbC0042F8

lbC0042CC	moveq	#0,D0
	move.b	($8F,SP),D0
	movea.l	($88,SP),A1
	move.l	D0,-(SP)
	pea	(A1,D4.W*2)
	move.l	A2,-(SP)
	pea	($68,SP)
	pea	($7C,SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC003FD8
	add.w	D0,D4
	move.w	D4,($5A,SP)
lbC0042F8	move.l	($5C,SP),D0
	ble	lbC00456A
	moveq	#$28,D0
	cmp.w	D0,D4
	blt.b	lbC0042CC
	bra	lbC00456A

_rawmouse	movea.l	($78,A3),A5	window
	tst.l	(wd_BorderRPort,A5)
	beq.b	\1
	movem.w	(wd_GZZMouseX,A5),D4/D5
;	move.w	(wd_GZZMouseX,A5),D4
;	move.w	(wd_GZZMouseY,A5),D5
	bra.b	\2

\1	move.w	(wd_MouseX,A5),D4
	move.w	(wd_MouseY,A5),D5
\2	ext.l	d4
	ext.l	d5
	move.w	($120,A3),D0
	ext.l	D0
	move.l	D4,D1
	sub.l	D0,D1
	bpl.b	\3
	move.l	d0,d1
	sub.l	D4,D1
\3	moveq	#2,D0
	cmp.l	D0,D1
	bge.b	\5
	move.w	($122,A3),D1
	ext.l	D1
	move.l	D5,D2
	sub.l	D1,D2
	bpl.b	\4
	move.l	d1,d2
	sub.l	D5,D2
\4	cmp.l	D0,D2
	bge.b	\5
;	btst	#7,(7,A2)
;	beq.b	\5
	tst.b	(7,A2)
	bpl	\5
	move.w	#$406,D6
\5	movem.w	d4/d5,($120,a3)
	bra	lbC00456A


_closeg	move.w	#$416,D6	close gadget
	bra	lbC00456A

_menu	movea.l	($78,A3),A0	our window
	movea.l	($A4,A3),A1	menu strip
	cmpa.l	(wd_MenuStrip,A0),A1
	bne	lbC00456A
	move.w	(6,A2),D5
	movea.l	($74,SP),A2
	bra	lbC0044DA

lbC0043B4	move.w	D5,D0
	subi.w	#$F800,D0
	beq	lbC004452	'Console/Clear window' ($f800)
	subq.w	#1,D0
	beq	lbC004488	'Complete/Filename' ($f801)
	subq.w	#1,D0
	beq	lbC00449A	'Review/Enabled' ($f802)
	subq.w	#1,D0
	beq	lbC00449A	'History/Enabled' ($f803)
	moveq	#$1D,D1
	sub.w	D1,D0
	beq	lbC004454	'Console/Reset ($f820)
	subq.w	#1,D0
	beq	lbC00448E	'Complete/Command' ($f821)
	subq.w	#1,D0
	beq	lbC0044AC	'Review/Clear buffer' ($f822)
	subq.w	#1,D0
	beq	lbC0044BE	'History/Clear buffer' ($f823)
	sub.w	d1,d0
	beq	lbC00449A	'Console/Jump scroll' ($f840)
	subq.w	#1,d0
	beq	lbC004494	'Complete/Device' ($f841)
	moveq	#$21,D1
	sub.w	D1,D0
	beq	lbC0044B2	'Review/Save plain text as' ($f862)
	moveq	#$1E,D1
	sub.w	D1,D0
	beq.b	lbC004458	'Console/Iconify' ($f880)
	subq.w	#1,D0
	beq	lbC00449A	'Complete/Enable cache' ($f881)
	subq.w	#1,D0
	beq	lbC0044B8	'Review/Save with styles as' ($f882)
	sub.w	D1,D0
	beq	lbC00445E	'Console/Normalize' ($f8a0)
	subq.w	#1,D0
	beq	lbC0044A0	'Complete/Reset cache' ($f8a1)
	moveq	#$1F,D1
	sub.w	D1,D0
	beq.b	lbC004464	'Console/Maximize' ($f8c0)
	subq.w	#1,d0
	beq	lbC0044A6	'Complete/Purge cache' ($f8c1)
	moveq	#$3F,D1
	sub.w	D1,D0
	beq.b	lbC00446A	'Console/Next screen' ($f900)
	subq.w	#1,D0
	beq.b	lbC00449A	'Complete/Show .info' ($f901)
	moveq	#$1F,D1
	sub.w	D1,D0
	beq.b	lbC004470	'Console/Goto screen' ($f920)
	moveq	#$40,D1
	sub.w	D1,D0
	beq.b	lbC004476	'Console/Halt' ($f960)
	moveq	#$20,D1
	sub.w	D1,D0
	beq.b	lbC00447A	'Console/Resume' ($f980)
	sub.w	D1,D0
	beq	lbC00449A	'Console/Keep closed' ($f9a0)
	moveq	#$40,D1
	sub.w	D1,D0
	beq.b	lbC00447E	'Console/About' ($f9e0)
	sub.w	D1,D0
	beq	lbC004484	'Console/Quit' ($fa20)
	moveq	#0,D5
	bra	lbC0044C6

lbC004452	move.w	#$3E9,D5	'Console/Clear window'
	bra.b	lbC0044C6

lbC004454	move.w	#$3E7,d5	'Console/Reset'
	bra.b	lbC0044C6

lbC004458	move.w	#$3EA,D5	'Console/Iconify'
	bra.b	lbC0044C6

lbC00445E	move.w	#$3EB,D5	'Console/Normalize'
	bra.b	lbC0044C6

lbC004464	move.w	#$3EC,D5	'Console/Maximize'
	bra.b	lbC0044C6

lbC00446A	move.w	#$417,D5	'Console/Next screen'
	bra.b	lbC0044C6

lbC004470	move.w	#$418,D5	'Console/Goto screen'
	bra.b	lbC0044C6

lbC004476	moveq	#$13,D5		'Console/Halt'
	bra.b	lbC0044C6

lbC00447A	moveq	#$11,D5		'Console/Resume'
	bra.b	lbC0044C6

lbC00447E	move.w	#$3ED,D5	'Console/About'
	bra.b	lbC0044C6

lbC004484	moveq	#$1C,D5		'Console/Quit'
	bra.b	lbC0044C6

lbC004488	move.w	#$3EE,D5	'Complete/Filename'
	bra.b	lbC0044C6

lbC00448E	move.w	#$3F0,D5	'Complete/Command'
	bra.b	lbC0044C6

lbC004494	move.w	#$3EF,D5	'Complete/Device'
	bra.b	lbC0044C6

lbC00449A	move.w	#$3F1,D5	...
	bra.b	lbC0044C6

lbC0044A0	move.w	#$419,D5	'Complete/Reset cache'
	bra.b	lbC0044C6

lbC0044A6	move.w	#$41A,D5	'Complete/Purge cache'
	bra.b	lbC0044C6

lbC0044AC	move.w	#$3F2,D5	'Review/Clear buffer'
	bra.b	lbC0044C6

lbC0044B2	move.w	#$3F3,D5	'Review/Save plain text as'
	bra.b	lbC0044C6

lbC0044B8	move.w	#$3F4,D5	'Review/Save with styles as'
	bra.b	lbC0044C6

lbC0044BE	move.w	#$3F5,D5	'History/Clear buffer'

lbC0044C6	move.l	D4,D0
	addq.w	#1,D4
	move.w	D4,($5A,SP)
	movea.l	($88,SP),A1
	move.w	D5,(A1,D0.W*2)
	move.w	($20,A5),D5
lbC0044DA	moveq	#40,D0
	cmp.w	D0,D4
	bge.b	lbC0044F8
	movea.l	($A4,A3),A0
	movea.l	(8,A2),A6
	moveq	#0,D0
	move.w	D5,D0
	sys	ItemAddress
	movea.l	D0,A5
	tst.l	D0
	bne	lbC0043B4
lbC0044F8	moveq	#0,D6
	bra.b	lbC00456A

lbC0044FC	move.w	#$3F6,D6
	bra.b	lbC00456A

lbC004502	move.w	#$3F7,D6
	bra.b	lbC00456A

lbC004508	move.w	#$3F8,D6
	bra.b	lbC00456A

lbC00450E	move.w	#$3F9,D6
	bra.b	lbC00456A

lbC004514	movea.l	(A5),A0
	moveq	#$20,D0
	cmp.b	(A0),D0
	bne.b	lbC00456A
	moveq	#$1A,D6
	bra.b	lbC00456A

lbC004520	movea.l	(A5),A0
	moveq	#$20,D0
	cmp.b	(A0),D0
	bne.b	lbC00452C
	moveq	#1,D6
	bra.b	lbC00456A

lbC00452C	move.w	#$3FD,D6
	bra.b	lbC00456A

lbC004532	move.w	#$3FC,D6
	bra.b	lbC00456A

lbC004538	moveq	#4,D0
	cmp.b	($8F,SP),D0
	bne.b	lbC004546
	move.w	#$415,D6
	bra.b	lbC00456A

lbC004546	moveq	#$48,D0
	lsl.l	#6,D0
	and.l	($160,A3),D0
	bne.b	lbC00456A
	move.w	#$3EF,D6
	bra.b	lbC00456A

lbC004556	movea.l	(A5),A0
	moveq	#$30,D0
	cmp.b	(A0),D0
	bne.b	lbC00456A
	moveq	#$20,D0
	cmp.b	(1,A0),D0
	bne.b	lbC00456A
	move.w	#$3FE,D6
lbC00456A	movea.l	($2A,SP),A0
	addq.l	#1,A0
	movea.l	($7C,SP),A5
	move.l	A0,(A5)
	bra.b	lbC0045CC

lbC004578	move.w	#$3FF,D6
	bra.b	lbC0045CC

lbC00457E	moveq	#4,D0
	cmp.b	($8F,SP),D0
	bne.b	lbC00458C
	move.w	#$414,D6
	bra.b	lbC0045CC

lbC00458C	moveq	#$48,D0
	lsl.l	#6,D0
	and.l	($160,A3),D0
	bne.b	lbC0045C6
	move.w	#$3EE,D6
	bra.b	lbC0045CC

lbC00459C	move.w	#$3F8,D6
	bra.b	lbC0045CC

lbC0045A2	moveq	#4,D0
	cmp.b	($8F,SP),D0
	bne.b	lbC0045B0
	move.w	#$413,D6
	bra.b	lbC0045CC

lbC0045B0	moveq	#$13,D6
	bra.b	lbC0045CC

lbC0045B4	moveq	#1,D0
	cmp.b	($8F,SP),D0
	bne.b	lbC0045C0
	moveq	#4,D6
	bra.b	lbC0045CC

lbC0045C0	move.w	#$412,D6
	bra.b	lbC0045CC

lbC0045C6	moveq	#0,D6
	move.b	($28,SP),D6
lbC0045CC	movea.l	($80,SP),A3
	move.l	(A3),D0
	bpl.b	lbC0045DA
	sub.l	D0,(A5)
	clr.l	(A3)
	moveq	#0,D6
lbC0045DA	move.w	($5A,SP),D4
	tst.w	D6
	beq.b	lbC0045F4
	ext.l	D4
	move.l	D4,D0
	add.l	D0,D0
	movea.l	($88,SP),A0
	adda.l	D0,A0
	addq.w	#1,D4
	move.w	D6,(A0)
lbC0045F4	move.w	D4,D0
	movem.l	(SP)+,D2/D4-D7/A2/A3/A5/A6
	adda.w	#$4C,SP
	rtd	#28

lbC004600	movem.l	d0/a0,-(SP)
	move.l	a0,d1
	suba.l	a2,a1
	add.l	a1,D1
	subq.l	#1,a1
	move.l	a1,d0
	movea.l	a2,a1
	move.l	a2,a0
	bsr	lbC00325C
	movem.l	(SP)+,d0/a0
	move.b	d0,(a2)
	move.l	a0,D0
	addq.l	#1,D0
	rts

lbC004638	suba.w	#$140,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	movea.l	($170,SP),A5
	lea	($1FC,A5),A0
	moveq	#0,D7
	moveq	#0,D6
	move.l	A0,($164,SP)
	lea	($9FC,A5),A3
	bra	lbC004BFC

lbC004658	movea.l	($164,SP),A3
	movea.l	A5,a1
	bsr	lbC00C90E
	movea.l	($16C,SP),A2
	beq.b	lbC004676
	moveq	#1,D4
	move.w	#$40B,($110,SP)
	bra.b	lbC004696

lbC004676	clr.l	-(SP)
	pea	($114,SP)
	pea	($DA,SP)
	pea	($180,SP)
	pea	($174,SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003FD8
	move.w	D0,D4
lbC004696	tst.w	D4
	beq	lbC004B9E
	moveq	#0,D0
	move.w	($110,SP),D0
	subq.l	#3,D0
	beq	lbC004B7A
	subq.l	#1,D0
	beq	lbC004B7A
	subq.l	#1,D0
	beq	lbC004B7A
	subq.l	#1,D0
	beq	lbC004B7A
	moveq	#11,D1
	sub.l	D1,D0
	beq	lbC004906
	subq.l	#2,D0
	beq	lbC0048D2
	moveq	#9,D1
	sub.l	D1,D0
	beq	lbC004B2E
	subi.l	#$3CC,D0
	beq	lbC0047A2
	subq.l	#1,D0
	beq	lbC0047E2
	subq.l	#1,D0
	beq	lbC004868
	subq.l	#1,D0
	beq	lbC00487E
	subq.l	#1,D0
	beq	lbC004890
	subq.l	#1,D0
	beq	lbC004944
	subq.l	#1,D0
	beq	lbC004956
	subq.l	#1,D0
	beq	lbC004956
	subq.l	#1,D0
	beq	lbC004956
	subq.l	#1,D0
	beq	lbC004A60
	subq.l	#1,D0
	beq	lbC004A72
	subq.l	#1,D0
	beq	lbC004A8E
	subq.l	#1,D0
	beq	lbC004ADC
	subq.l	#1,D0
	beq	lbC004B28
	moveq	#9,D1
	sub.l	D1,D0
	beq.b	lbC004752
	moveq	#13,D1
	sub.l	D1,D0
	beq.b	lbC00477A
	moveq	#12,D1
	sub.l	D1,D0
	beq	lbC0048A2
	subq.l	#1,D0
	beq	lbC0048A2
	subq.l	#1,D0
	beq	lbC004B5E
	subq.l	#1,D0
	beq	lbC004B6C
	bra	lbC004B9E

lbC004752	lea	($1FC,A5),A2
	move.l	($174,SP),d0
	move.l	($164,SP),a1
	movea.l	A2,A3
	move.l	A2,d1
	move.l	($16c,SP),a0
	bsr	lbC007226
	add.l	D0,($174,SP)
	move.l	A3,($164,SP)
	bra	lbC004B9E

lbC00477A	movea.l	A2,A1
	movea.l	($160,SP),A2
	clr.l	-(SP)
	move.l	A2,-(SP)
	pea	($9FC,A5)
	move.l	A5,-(SP)
	move.l	A1,-(SP)
	bsr	lbC00C92E
	lea	(20,SP),SP
	movea.l	A2,A0
	adda.l	D0,A0
	movea.l	($164,SP),A3
	move.l	D0,D7
	bra	lbC004B9A

lbC0047A2	clr.l	-(sp)
	movea.l	sp,a0
	moveq	#1,d0
	movea.l	a5,a1
	move.l	a2,d1
	bsr	_doio
	addq.l	#4,sp
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00AF9E
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00AE7A
	btst	#4,($160,A5)
	bne	lbC004B9E
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC009C58	bclr	#0,(_clrw,a4)
	bne	\clrw
	move.l	#$1B<<24|'c'<<16|10<<8|0,(a0)	ESCc,0
	movea.l	($90,a5),a1
	bsr	lbC00A4F4
	moveq	#3,d0
	rts

\clrw	move.l	#$9b<<24|'H'<<16|$9b<<8|'J',(a0)	CSIH CSIJ
	movea.l	($90,a5),a1
	bsr	lbC00A4F4
	moveq	#4,d0
	rts

lbC0047E2	move.l	($8C,A5),D0
	beq.b	lbC004802
	movea.l	($78,A5),A0
	move.l	A0,($28,SP)
	movea.l	($28,SP),A1
	movea.l	($32,A1),A1
	movea.l	($10,A2),A6
	movea.l	D0,A0
	jsr	(-$42,A6)
lbC004802	;move.l	A5,-(SP)
	move.l	A2,a0
	bsr	lbC00B96A
	lea	($F0-8,SP),a0
	bsr	lbC009C58
	move.l	D0,D5
;	move.l	D0,-(SP)
	lea	($F4-12,SP),a0
	move.l	A5,d1
	move.l	A2,a1
	bsr	lbC00B3A6
	lea	($100-16-8,SP),A0
	moveq	#1,d1
	move.l	($160,A5),d0
	adda.l	D5,A0
	bsr	lbC009BA2
	add.l	D0,D5
	lea	($100-16-8,SP),A0
	moveq	#0,d1
	move.l	#$82400,d0
	adda.l	D5,A0
	bsr	lbC009BA2
	add.l	D0,D5
	move.l	D5,d0
	lea	($100-16-8,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004868	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00D9B6
	move.w	D0,($104,A5)
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC00487E	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00181E
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004890	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC0017B4
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC0048A2	cmpi.w	#$418,($110,SP)
	seq	D0
	neg.b	D0
	extb.l	D0
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00D882
	tst.w	D0
	beq.b	lbC0048CA
	move.l	($78,A5),D0
	beq	lbC004C18
lbC0048CA	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC0048D2	lea	(1,A3),A0
	movea.l	($164,SP),A1
	cmpa.l	A0,A1
	bls	lbC004B9E
	lea	($1FC,A5),A2
	moveq	#$13,d0
	movea.l	($174,SP),a0
	bsr	lbC004600
	movea.l	A2,A3
	move.l	D0,($174,SP)
	move.l	A3,($164,SP)
	bra	lbC004B9E

lbC004906	;move.l	A5,-(SP)
	move.l	A2,a0
	bsr	lbC00B96A
	movea.l	($164,SP),A1
	lea	(1,A3),A0
	cmpa.l	A0,A1
	bls	lbC004B9E
	lea	($1FC,A5),A2
	moveq	#$11,d0
	movea.l	($174,SP),a0
	bsr	lbC004600
	movea.l	A2,A3
	move.l	D0,($174,SP)
	move.l	A3,($164,SP)
	bra	lbC004B9E

lbC004944	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00C830
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004956	moveq	#1,d0
	suba.l	a1,a1
	lea	($88-8,SP),a0
	bsr	lbC009826
	clr.l	($C2-16,SP)
	move.w	($120-16,SP),D0
	pea	($90-16,SP)
	ext.l	D0
	move.l	D0,-(SP)
	clr.l	-(SP)
	pea	($9FC,A5)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC0035FA
	lea	($28-16,SP),SP
	tst.w	D0
	beq	lbC004A48
	move.w	($110,SP),D0
	cmpi.w	#$3EE,D0
	bne.b	lbC00499E
	lea	(Selectfilenam.MSG,pc),A1
	bra.b	lbC0049AE

lbC00499E	cmpi.w	#$3EF,D0
	bne.b	lbC0049AA
	lea	(Selectdevice.MSG,pc),A1
	bra.b	lbC0049AE

lbC0049AA	lea	(Selectcommand.MSG),A1
lbC0049AE	bsr	_getstr
	moveq	#1,D0
	cmp.l	($8E,SP),D0
	bne.b	lbC0049BC
	movea.l	($80,SP),A2
	bra.b	lbC0049D4

lbC0049BC	move.l	A1,-(SP)
	clr.l	-(SP)
	pea	($88,SP)
	move.l	A5,-(SP)
	move.l	($17C,SP),-(SP)
	bsr	lbC007932
	movea.l	D0,A2
lbC0049D4	move.l	A2,D0
	beq.b	lbC004A48
	lea	($1FC,A5),A3
	movea.l	($164,SP),A0
	cmpa.l	A3,A0
	bls.b	lbC004A00
	move.l	A0,D0
	move.l	A3,D1
	sub.l	D1,D0
	move.l	($174,SP),D2
	add.l	D0,D2
	move.l	D1,a1
	move.l	D1,a0
	move.l	D2,d1
	bsr	lbC00325C
lbC004A00	moveq	#$40,D0
	lsl.l	#5,D0
	move.l	($174,SP),D1
	sub.l	D1,D0
	move.l	(14,A2),D2
	cmp.l	D2,D0
	blt.b	lbC004A14
	move.l	D2,D0
lbC004A14	exg	D0,d1
	movea.l	A3,a1
	movea.l	A3,a0
	pea	($12,A2)
	bsr	lbC003280
	move.l	($174,SP),D5
	add.l	(14,A2),D5
	cmpi.l	#$800,D5
	ble.b	lbC004A3E
	moveq	#$40,D0
	lsl.l	#5,D0
	bra.b	lbC004A40

lbC004A3E	move.l	D5,D0
lbC004A40	move.l	D0,($174,SP)
	move.l	A3,($164,SP)
lbC004A48	movea.l	($AE,SP),A0
	pea	($80,SP)
	move.l	($170,SP),-(SP)
	jsr	(A0)
	addq.l	#8,SP
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004A60	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00C692
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004A72	;move.l	A5,-(SP)
	move.l	A2,a0
	bsr	lbC00B96A
	bsr	lbC00AF60
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004ADC	moveq	#$15,d2	with styles
	bra	lbC004A90

lbC004A8E	moveq	#11,d2	plain
lbC004A90	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00170E
	pea	(47,SP)
	lea	(Savereview.MSG,pc),a1
	bsr	_getstr
	moveq	#$2A,d0
	move.l	A5,d1
	move.l	A2,a0
	bsr	_filereq
	tst.l	D0
	beq.b	lbC004ACA
;	move.l	d2,-(sp)
	lea	($33-4,SP),a1
;	move.l	A5,-(SP)
;	move.l	A2,-(SP)
	bsr	lbC00B06A
lbC004ACA	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC001746
	movea.l	($164,SP),A3
	bra	lbC004B9E

lbC004B28	movea.l	($164,SP),A3
	bra.b	lbC004B9E

lbC004B2E	lea	(1,A3),A0
	movea.l	($164,SP),A1
	cmpa.l	A0,A1
	bls.b	lbC004B9E
	lea	($1FC,A5),A2
	moveq	#$1C,d0
	movea.l	($174,SP),a0
	bsr	lbC004600
	movea.l	A2,A3
	move.l	D0,($174,SP)
	move.l	A3,($164,SP)
	bra.b	lbC004B9E

lbC004B5E	movea.l	A2,a0
	bsr	lbC008BA2
	movea.l	($164,SP),A3
	bra.b	lbC004B9E

lbC004B6C	movea.l	A2,a0
	bsr	lbC008A98
	movea.l	($164,SP),A3
	bra.b	lbC004B9E

lbC004B7A	move.w	($110,SP),D0
	movea.l	A5,a0
	bsr	lbC003F54
	bra.b	lbC004B9E

lbC004B92	movea.l	($160,SP),A0
	move.b	(A3)+,(A0)+
	addq.l	#1,D7
lbC004B9A	move.l	A0,($160,SP)
lbC004B9E	cmpa.l	($164,SP),A3
	bcc.b	lbC004BAC
	cmpi.l	#$200,D7
	blt.b	lbC004B92
lbC004BAC	move.l	($10A,A5),D0
	move.l	($10E,A5),D1
	cmp.l	D0,D1
	bge.b	lbC004BC4
	moveq	#$40,D2
	lsl.l	#5,D2
	move.l	D2,D3
	sub.l	D0,D3
	add.l	D1,D3
	bra.b	lbC004BC8

lbC004BC4	sub.l	D0,D1
	move.l	D1,D3
lbC004BC8	add.l	D7,D3
	cmpi.l	#$800,D3
	movea.l	($16C,SP),A3
	bls.b	lbC004BE2
	suba.l	A0,A0
	movea.l	(8,A3),A6
	moveq	#0,D7
	jsr	(-$60,A6)
lbC004BE2	lea	($9FC,A5),A2
	move.l	D7,d0
	movea.l	A2,a0
	movea.l	A5,a1
	bsr	lbC0098BA
	add.l	D7,D6
	moveq	#0,D7
	movea.l	A2,A3
lbC004BFC	move.l	A3,($160,SP)
	tst.l	($174,SP)
	bne	lbC004658
	movea.l	A5,a1
	bsr	lbC00C90E
	bne	lbC004658
	move.l	D6,D0
lbC004C18	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$140,SP
	rtd	#12

lbC004C22	suba.w	#$190,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	lea	($1A8,SP),A3
	moveq	#0,D7
	clr.l	(A3)
	movea.l	($1C0,SP),A5
;	movea.l	($98,A5),A0
;	move.l	(io_Unit,A0),($2C,SP)
	moveq	#0,D4
	move.w	#1,($11E,A5)
	lea	($1FC,A5),A0
	move.l	A0,($1B0,SP)
	move.l	($116,A5),D0
	movea.l	A5,A0
	adda.l	D0,A0
	move.l	D0,(A3)+
	lea	($9FC,A0),A6
	move.l	A6,(A3)+
	movea.l	($1BC,SP),A2
	tst.l	D0
	ble.b	\2
	lea	($9FC,A5),a0
	move.l	A5,d1
	movea.l	A2,a1
	bsr	lbC0031DE
	bra.b	\2

\1	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC0014D4
	move.l	D0,($1C4,SP)
\2	tst.l	($1C4,SP)
	bne.b	\3
	movea.l	A5,a1
	bsr	lbC00C90E
	beq.b	\1
\3	movea.l	($60,A5),A3
	move.l	A3,($38,SP)
	moveq	#1,d0
	suba.l	a1,a1
	lea	($44-8,SP),a0
	bsr	lbC009826
	clr.l	($6E,SP)
	btst	#5,($160,A5)
	beq	\141
	btst	#3,($162,A5)
	bne	\141
	tst.l	($90,A5)
	beq	\141
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00AE7A
	bra	\141

\4	tst.l	($6E,SP)
	beq.b	\5
	move.b	#4,($108,SP)
	bra.b	\9

\5	move.l	($1A8,SP),D0
	bne.b	\6
	move.b	#1,($108,SP)
	bra.b	\9

\6	lea	($9FC,A5),A0
	movea.l	($1AC,SP),A1
	cmpa.l	A1,A0
	bne.b	\7
	move.b	#2,($108,SP)
	bra.b	\9

\7	movea.l	A5,A0
	adda.l	D0,A0
	lea	($9FC,A0),A6
	cmpa.l	A6,A1
	bne.b	\8
	move.b	#3,($108,SP)
	bra.b	\9

\8	clr.b	($108,SP)
\9	movea.l	A5,a1
	bsr	lbC00C90E
	beq.b	\10
	move.w	#1,($1A4,SP)
	move.w	#$40B,($B6,SP)
	bra.b	\11

\10	moveq	#0,D0
	move.b	($108,SP),D0
	move.l	D0,-(SP)
	pea	($BA,SP)
	pea	($114,SP)
	pea	($1D0,SP)
	pea	($1C0,SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003FD8
	move.w	D0,($1A4,SP)
\11	clr.w	($106,SP)
	bra	\139

\12	move.w	($B6,SP,D0.W*2),D0
	move.w	D0,($1B4,SP)
	moveq	#0,D6
	clr.b	($B4,SP)
	subq.w	#1,D0		1 CTRL-A
	beq	\101
	subq.w	#2,D0		3 CTRL-C
	beq	\104
	subq.w	#1,D0		4 CTRL-D
	beq	\104
	subq.w	#1,D0		5 CTRL-E
	beq	\104
	subq.w	#1,D0		6 CTRL-F
	beq	\104
	subq.w	#2,D0		8 BS
	beq	\59
	subq.w	#2,D0		A CTRL-J
	beq	\107
	subq.w	#1,D0		B CTRL-K
	beq	\96
	subq.w	#1,D0		C CTRL-L
	beq	\100
	subq.w	#1,D0		D CTRL-M (ENTER)
	beq	\107
	subq.w	#3,D0		10 CTRL-P
	beq	\80
	subq.w	#1,D0		11 'Console/Resume' (CTRL-Q)
	beq	\22
	subq.w	#2,D0		13 'Console/Halt' (CTRL-S)
	beq	\21
	subq.w	#1,D0		14 CTRL-T
	beq	\88
	subq.w	#1,D0		15 CTRL-U (SHIFT-BS)
	beq	\61
	subq.w	#2,D0		17 CTRL-W (ALT-BS)
	beq	\62
	subq.w	#2,D0		19 CTRL-Y
	beq	\93
	subq.w	#1,D0		1A
	beq	\102
	subq.w	#2,D0		1C 'Console/Quit' (CTRL-\)
	beq	\25
	moveq	#$63,D1
	sub.w	D1,D0		7F DEL
	beq	\60
	bmi	\126
	subi.w	#$368,D0	3E7 'Console/Reset'
	beq	\15
	subq.w	#1,d0		3E8 window resize
	beq	\13
	subq.w	#1,D0		3E9 'Console/Clear window'
	beq	\14
	subq.w	#1,D0		3EA 'Console/Iconify'
	beq	\17
	subq.w	#1,D0		3EB 'Console/Normalize'
	beq	\18
	subq.w	#1,D0		3EC 'Console/Maximize'
	beq	\19
	subq.w	#1,D0		3ED 'Console/About'
	beq	\23
	subq.w	#1,D0		3EE 'Complete/Filename'
	beq	\26
	subq.w	#1,D0		3EF 'Complete/Device'
	beq	\26
	subq.w	#1,D0		3F0 'Complete/Command'
	beq	\26
	subq.w	#1,D0		3F1 'Console/Keep closed'
	beq	\27
	subq.w	#1,D0		3F2 'Review/Clear buffer'
	beq	\28
	subq.w	#1,D0		3F3 'Review/Save plain text as'
	beq	\30
	subq.w	#1,D0		3F4 'Review/Save with styles as'
	beq	\29
	subq.w	#1,D0		3F5 'History/Clear buffer'
	beq	\33
	subq.w	#1,D0		3F6 CRSR-RIGHT
	beq	\34
	subq.w	#1,D0		3F7 CRSR-LEFT
	beq	\35
	subq.w	#1,D0		3F8 CTRL-R
	beq	\36
	subq.w	#1,D0		3F9
	beq	\44
	subq.w	#1,D0		3FA ALT-CRSR-RIGHT
	beq	\45
	subq.w	#1,D0		3FB ALT-CRSR-LEFT
	beq	\48
	subq.w	#1,D0		3FC CRSR-DOWN
	beq	\53
	subq.w	#1,D0		3FD CRSR-UP
	beq	\51
	subq.w	#1,D0		3FE RAMIGA-V
	beq	\58
	subq.w	#1,D0		3FF CTRL-B
	beq	\98
	subq.w	#2,D0		401 RAMIGA-C
	beq	\108
	subq.w	#1,D0		402 ALT-CRSR-UP
	beq	\109
	subq.w	#1,D0		403 ALT-CRSR-DOWN
	beq	\111
	subq.w	#1,D0		404
	beq	\112
	subq.w	#1,D0		405
	beq	\113
	subq.w	#1,D0		406 RAWMOUSE
	beq	\116
	subq.w	#1,D0		407 ALT-DEL
	beq	\66
	subq.w	#1,D0		408
	beq	\65
	subq.w	#1,D0		409
	beq	\70
	subq.w	#1,D0		40A ALT-RET
	beq	\107
	subq.w	#1,D0		40B AppWindow icon drop
	beq	\123
	subq.w	#1,D0		40C CTRL-DEL
	beq	\66
	subq.w	#1,D0		40D CTRL-BS
	beq	\62
	subq.w	#1,D0		40E CTRL-CRSR-RIGHT
	beq	\45
	subq.w	#1,D0		40F CTRL-CRSR-LEFT
	beq	\48
	subq.w	#1,D0		410 CTRL-CRSR-UP
	beq	\114
	subq.w	#1,D0		411 CTRL-CRSR-DOWN
	beq	\115
	subq.w	#1,D0		412 CTRL-D
	beq	\103
	subq.w	#1,D0		413
	beq	\26
	subq.w	#1,D0		414
	beq	\26
	subq.w	#1,D0		415
	beq	\26
	subq.w	#1,D0		416 Close button
	beq	\25
	subq.w	#1,D0		417 'Console/Next screen'
	beq	\20
	subq.w	#1,D0		418 'Console/Goto screen'
	beq	\20
	subq.w	#1,D0		419 'Complete/Reset cache'
	beq	\124
	subq.w	#1,D0		41A 'Complete/Purge cache'
	beq	\125
	bra	\126

\13	clr.l	-(sp)
	movea.l	sp,a0
	moveq	#1,d0
	movea.l	a5,a1
	move.l	a2,d1
	bsr	_doio
	addq.l	#4,sp
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00AF9E	reprint if in buffer display mode
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00AE7A
	bra	\131

\14	st	(_clrw,a4)
\15	bsr	\24
	clr.l	($6E,SP)
	move.l	($8C,A5),D0
	beq.b	\16
	movea.l	($78,A5),A0
	move.l	A0,($28,SP)
	movea.l	($28,SP),A1
	movea.l	($32,A1),A1
	movea.l	($10,A2),A6
	movea.l	D0,A0
	jsr	(-$42,A6)
\16	;move.l	A5,-(SP)
	move.l	A2,a0
	bsr	lbC00B96A
	lea	($184-8,SP),a0
	bsr	lbC009C58
	move.l	D0,D6
	lea	($188-4-8,SP),a0
	move.l	A5,d1
	move.l	A2,a1
	bsr	lbC00B3A6
	lea	($194-16-8,SP),A0
	moveq	#1,d1
	move.l	($160,A5),d0
	adda.l	D6,A0
	bsr	lbC009BA2
	add.l	D0,D6
	clr.l	($1C0-16-8,SP)
	lea	($9FC,A5),A0
	move.l	A0,($1c4-16-8,SP)
	moveq	#0,D4
	clr.l	-(SP)
	pea	(13).W
	clr.l	-(SP)
	move.l	A0,-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003DEE
	lea	($1c-4,SP),SP
	bra	\131

\17	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00D9B6
	move.w	D0,($104,A5)
	bra	\131

\18	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00181E
	bra	\131

\19	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC0017B4
	bra	\131

\20	cmpi.w	#$418,($1B4,SP)	Goto Screen?
	seq	D0
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00D882
	tst.w	D0
	beq	\131
;	movea.l	($98,A5),A0
;	move.l	(io_Unit,A0),($2C,SP)
	move.l	($78,A5),D0
	beq	\143
	movea.l	A5,a0
	bsr	lbC00BD7E
	tst.l	D0
	bne	\131
	move.l	($1A8,SP),d0
	lea	($9FC,A5),a0
	move.l	A5,d1
	movea.l	A2,a1
	bsr	lbC0031DE
	move.l	($1A8,SP),D0
	movea.l	A5,A0
	adda.l	D0,A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\131
	lea	($9FC,A5),A1
	move.l	A1,D1
	sub.l	($1AC,SP),D1
	add.l	D1,D0
	move.l	D0,d1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	bra	\131

\21	bsr	\24
	clr.l	($6E,SP)
	moveq	#1,D4
	bra	\131

\22	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	bra	\130

\23	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00C830
	bra	\131

\24:	movea.l	($5E+4,SP),A0
	pea	($3C+4,SP)
	move.l	A2,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	rts

\25	bsr	\24
	clr.l	($76-8,SP)
	pea	(1).W
	pea	($1C).W
	move.l	($1B8-8,SP),-(SP)
	move.l	($1C0-8,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003DEE
	lea	($20-8,SP),SP
	add.l	D0,D7
	clr.l	($1A8,SP)
	lea	($9FC,A5),A0
	move.l	A0,($1AC,SP)
	bra	\130

\26	move.w	($1B4,SP),D0
	pea	($3C,SP)
	ext.l	D0
	move.l	D0,-(SP)
	pea	($1B0,SP)
	pea	($1B8,SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003720
	lea	($18,SP),SP
	tst.l	D0
	beq	\130
	move.b	#2,($B4,SP)
	bra	\130

\27	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC00C692
	bra	\131

\28	move.l	($1A8,SP),a0
	move.l	($1B0-4,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	bsr	lbC00AF60
	bra	\130


\29	moveq	#$15,d2	;review+styles
	bra	\31

\30	moveq	#11,d2
\31	move.l	A5,a0	;save review (plain)
	move.l	A2,a1
	bsr	lbC00170E
	pea	(295,SP)
	lea	(Savereview.MSG,pc),a1
	bsr	_getstr
	moveq	#$2A,d0
	move.l	A5,d1
	move.l	A2,a0
	bsr	_filereq
	tst.l	D0
	beq.b	\32
;	move.l	d2,-(sp)
	lea	($12B-4,SP),a1
;	move.l	A5,-(SP)
;	move.l	A2,-(SP)
	bsr	lbC00B06A
\32	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC001746
	bra	\131

\33	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	ObtainSemaphore
	movea.l	($50,A5),A0
	pea	($2E,A5)
	move.l	A2,-(SP)
	jsr	(A0)
	addq.l	#8,SP
	lea	($32,A5),A0
	move.l	A0,($38,SP)
	movea.l	A5,A0
	sys	ReleaseSemaphore
	bra	\131

\34	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A1
	lea	($9FC,A5),A0
	cmpa.l	A0,A1
	bls	\130
	moveq	#1,d1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	subq.l	#1,($1AC,SP)
	bra	\130

\35	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	adda.l	($1A8,SP),A5
	lea	($9FC,A5),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\130
	moveq	#1,d1
	lea	($180-4,SP),a0
	bsr	lbC0099D6
	move.l	D0,D6
	addq.l	#1,($1AC,SP)
	bra	\130

\36	bsr	\24
	clr.l	($6E,SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	sys	ObtainSemaphore
	lea	($9FC,A5),A3
	move.l	($1AC,SP),D0
	move.l	A3,D1
	sub.l	D1,D0
	movea.l	($38,SP),a0
\37	movem.l	D7/A3/A5/A6,-(SP)
	move.l	d0,D7
	movea.l	d1,A3
	movea.l	(4,A0),A5
	move.l	A5,D0
	bne.b	\39
	bra.b	\40
\38	movea.l	A5,A0
	movea.l	(4,A0),A5
\39	move.l	(4,A5),d0
	beq.b	\40
	lea	($12,A5),A0
	movea.l	A3,A1
	move.l	D7,D0
	movea.l	($1C,A2),A6
	jsr	(-$A8,A6)
	tst.l	D0
	bne.b	\38
	move.l	A5,D0
\40	movem.l	(SP)+,D7/A3/A5/A6
	beq.b	\42
	move.l	D0,-(SP)
	move.l	($1AC,SP),-(SP)
	move.l	($1B4,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	move.l	D0,($4C,SP)
	bsr	lbC003BA8
	lea	($14,SP),SP
	move.l	D0,($1A8,SP)
	move.l	($1AC,SP),D1
	move.l	A3,D2
	sub.l	D2,D1
	cmp.l	D1,D0
	ble.b	\41
	sub.l	D1,D0
	move.l	D0,d1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
\41	move.b	#2,($B4,SP)
	bra.b	\43

\42	suba.l	A0,A0
	movea.l	(8,A2),A6
	jsr	(-$60,A6)
\43	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	ReleaseSemaphore
	bra	\130

\44	movea.l	($5E,SP),A0
	pea	($3C,SP)
	move.l	A2,-(SP)
	jsr	(A0)
	clr.l	($76,SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	sys	ObtainSemaphore
	lea	($32,A5),A0
	move.l	A0,(SP)
	move.l	($1B0,SP),-(SP)
	move.l	($1B8,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	move.l	A0,($50,SP)
	bsr	lbC003BA8
	lea	($18,SP),SP
	move.l	D0,($1A8,SP)
	movea.l	A5,A0
	sys	ReleaseSemaphore
	lea	($9FC,A5),A0
	move.l	A0,($1AC,SP)
	move.b	#2,($B4,SP)
	bra	\130

\45	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1a8,SP),a0
	move.l	($1ac,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	cmpi.w	#$3FA,($1B4,SP)
	lea	($9FC,A5),A3
	bne.b	\46
	move.l	($1AC,SP),a0
	move.l	($1A8,SP),d1
	move.l	A3,a1
	bsr	lbC003CE6
	bra.b	\47

\46	move.l	($1AC,SP),a0
	move.l	($1A8,SP),d1
	move.l	A3,a1
	bsr	lbC003D9E
\47	movea.l	($1AC,SP),A1
	movea.l	D0,A0
	movea.l	A0,A3
	cmpa.l	A1,A0
	bls	\130
	move.l	A3,D1
	sub.l	($1AC,SP),D1
	lea	($180-4,SP),a0
	bsr	lbC0099D6
	lea	($184-8,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
	moveq	#0,D6
	move.l	A3,($1AC,SP)
	bra	\130

\48	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	cmpi.w	#$3FB,($1B4,SP)
	lea	($9FC,A5),A3
	bne.b	\49
	movea.l	($1AC,SP),a1
	move.l	($1A8,SP),d0
	movea.l	A3,a0
	bsr	lbC003C84
	bra.b	\50

\49	move.l	A3,a0
	bsr	lbC003D1E
\50	movea.l	($1AC,SP),A1
	movea.l	D0,A0
	movea.l	A0,A3
	cmpa.l	A1,A0
	bcc	\130
	move.l	A1,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
;	move.l	D0,(SP)
	lea	($184-8,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
	moveq	#0,D6
	move.l	A3,($1AC,SP)
	bra	\130

\51	bsr	\24
	clr.l	($6E,SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	sys	ObtainSemaphore
	movea.l	(4,A3),A0
	move.l	A0,D0
	beq.b	\52
	move.l	A0,-(SP)
	move.l	($1AC,SP),-(SP)
	move.l	($1B4,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	move.l	A0,($4C,SP)
	bsr	lbC003BA8
	lea	($14,SP),SP
	movea.l	A5,A0
	adda.l	D0,A0
	move.l	D0,($1A8,SP)
	lea	($9FC,A0),A1
	move.l	A1,($1AC,SP)
	move.b	#2,($B4,SP)
\52	movea.l	A5,A0
	sys	ReleaseSemaphore
	bra	\130

\53	bsr	\24
	clr.l	($6E,SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	sys	ObtainSemaphore
	tst.l	(A3)
	beq.b	\54
	movea.l	(A3),A0
	move.l	A0,-(SP)
	move.l	($1AC,SP),-(SP)
	move.l	($1B4,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	move.l	A0,($4C,SP)
	bsr	lbC003BA8
	lea	($14,SP),SP
	movea.l	A5,A0
	adda.l	D0,A0
	move.l	D0,($1A8,SP)
	lea	($9FC,A0),A1
	move.l	A1,($1AC,SP)
	bra.b	\56

\54	move.l	($1A8,SP),D0
	ble.b	\57
	move.l	D0,a0
	move.l	($1B0-4,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A0
	lea	($9FC,A5),A3
	cmpa.l	A3,A0
	bls.b	\55
	move.l	A0,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
\55	move.l	A3,($1AC,SP)
	clr.l	($1A8,SP)
\56	move.b	#2,($B4,SP)
\57	movea.l	A5,A0
	sys	ReleaseSemaphore
	bra	\130

\58	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	lea	($1FC,A5),A3
	move.l	($1D8-20,SP),d0
	move.l	($1C4-20,SP),a1
	move.l	A3,d1
	move.l	A2,a0
	bsr	lbC007226
	add.l	D0,($1C4,SP)
	move.l	A3,($1B0,SP)
	bra	\130

\59	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A0
	lea	($9FC,A5),A3
	cmpa.l	A3,A0
	bls	\130
	moveq	#1,d1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	subq.l	#1,($1B4-8,SP)
	move.l	($1B0-8,SP),D1
	subq.l	#1,($1B0-8,SP)
	moveq	#1,d0
	movea.l	($1BC-16,SP),a1
	movea.l	A3,a0
	bsr	lbC00325C
	move.b	#1,($B4,SP)
	bra	\130

\60	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	($1A8,SP),D1
	movea.l	A5,A0
	adda.l	D1,A0
	lea	($9FC,A0),A0
	movea.l	($1AC,SP),A1
	cmpa.l	A0,A1
	bcc	\130
	subq.l	#1,($1A8,SP)
	moveq	#1,d0
	lea	($9FC,A5),a0
	bsr	lbC00325C
	move.b	#1,($B4,SP)
	bra	\130

\61	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A0
	lea	($9FC,A5),A3
	cmpa.l	A3,A0
	bls	\130
	move.l	A0,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	move.l	($1B4-8,SP),D0
	move.l	A3,D1
	sub.l	D1,D0
	move.l	D1,a1
	move.l	D1,a0
	move.l	($1B0-8,SP),d1
	bsr	lbC00325C
	move.l	($1AC,SP),D0
	sub.l	A3,D0
	sub.l	D0,($1A8,SP)
	move.l	A3,($1AC,SP)
	move.b	#2,($B4,SP)
	bra	\130

\62	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	moveq	#$17,D0
	cmp.w	($1B4,SP),D0
	lea	($9FC,A5),A2
	bne.b	\63
	movea.l	($1AC,SP),a1
	move.l	($1A8,SP),d0
	movea.l	A2,a0
	bsr	lbC003C84
	bra.b	\64

\63	move.l	A2,a0
	bsr	lbC003D1E
\64	movea.l	($1AC,SP),A1
	movea.l	D0,A0
	movea.l	A0,A3
	cmpa.l	A1,A0
	bcc	\130
	move.l	A1,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	move.l	($1B4-8,SP),D0
	move.l	A3,D1
	sub.l	D1,D0
	movea.l	D1,a1
	move.l	($1B0-8,SP),d1
	movea.l	A2,a0
	bsr	lbC00325C
	move.l	($1AC,SP),D0
	sub.l	A3,D0
	sub.l	D0,($1A8,SP)
	move.l	A3,($1AC,SP)
	move.b	#2,($B4,SP)
	bra	\130

\65	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	A5,A0
	adda.l	($1A8,SP),A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\130
	lea	($9FC,A5),A1
	move.l	A0,D0
	sub.l	A1,D0
	move.l	D0,($1A8,SP)
	move.b	#2,($B4,SP)
	bra	\130

\66	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	cmpi.w	#$407,($1B4,SP)
	lea	($9FC,A5),A2
	bne.b	\67
	move.l	($1AC,SP),a0
	move.l	($1AC-4,SP),d1
	move.l	A2,a1
	bsr	lbC003CE6
	bra.b	\68

\67	move.l	($1AC,SP),a0
	move.l	($1AC-4,SP),d1
	move.l	A2,a1
	bsr	lbC003D9E
\68	movea.l	D0,A0
	moveq	#$20,D0
	cmp.b	(-1,A0),D0
	movea.l	A0,A3
	bne.b	\69
	movea.l	($1AC,SP),A1
	addq.l	#1,A1
	cmpa.l	A1,A3
	bls.b	\69
	subq.l	#1,A3
\69	movea.l	($1AC,SP),A1
	cmpa.l	A1,A3
	bls	\130
	move.l	A3,D0
	sub.l	($1AC,SP),D0
	move.l	($1AC-4,SP),d1
	movea.l	A2,a0
	bsr	lbC00325C
	move.l	A3,D0
	sub.l	($1AC,SP),D0
	sub.l	D0,($1A8,SP)
	move.b	#2,($B4,SP)
	bra	\130

\70	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1c0-20,SP),a3
	movea.l	($1BC-20,SP),a0
	adda.w	#$9FC,A5

\71	moveq	#$20,D0
	cmp.b	(A3),D0
	beq.b	\72
	adda.l	A5,A0
	cmpa.l	A0,A3
	bne.b	\74
\72	moveq	#0,D0
	move.l	d0,a3
	bra.b	\77

\73	subq.l	#1,A3
\74	cmpa.l	A5,A3
	bls.b	\75
	moveq	#$20,D0
	cmp.b	(A3),D0
	bne.b	\73
\75	moveq	#$20,D0
	cmp.b	(A3),D0
	bne.b	\76
	addq.l	#1,A3
\76	move.l	A3,D0
\77	move.l	($1C8-28,SP),a0
	move.l	($1C4-24-4,SP),d1
	move.l	A5,a1
	bsr	lbC003CE6
	movea.l	D0,A2
	cmpa.l	A3,A5
	bne.b	\78
	movea.l	($1C0,SP),A0
	adda.l	($1A8,SP),A0
	lea	($9FC,A0),A1
	cmpa.l	A1,A2
	bcc.b	\78
	subq.l	#1,A2
\78	cmpa.l	A3,A2
	bls	\130
	move.l	A3,D0
	beq	\130
	movea.l	($1AC,SP),A0
	cmpa.l	A3,A0
	bls.b	\79
	move.l	A0,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
\79	move.l	A2,D5
	move.l	A3,D1
	sub.l	D1,D5
	move.l	D5,d0
	move.l	D1,a1
	move.l	($1AC-4,SP),d1
	move.l	A5,a0
	bsr	lbC00325C
	sub.l	D5,($1A8,SP)
	move.l	A3,($1AC,SP)
	move.b	#2,($B4,SP)
	bra	\130

\80	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A1
	lea	($9FC,A5),A2
	cmpa.l	A2,A1
	bls	\130
	move.b	#$20,($B5,SP)
	move.l	($1A8,SP),d0
	movea.l	A2,a0
	bsr	lbC003C84
	moveq	#0,D6
	move.l	($1AC,SP),D5
	sub.l	D0,D5
	movea.l	D0,A3
	bra.b	\82

\81	addq.l	#1,D6
\82	cmp.l	D5,D6
	bge.b	\83
	moveq	#$20,D0
	cmp.b	(A3,D6.L),D0
	bne.b	\81
\83	move.l	($1AC,SP),D0
	move.l	D0,D1
	move.l	A2,D2
	sub.l	D2,D1
	move.l	#$1FF,D5
	sub.l	D1,D5
	cmp.l	D5,D6
	bge.b	\84
	move.l	D6,D1
	bra.b	\85

\84	move.l	D5,D1
\85	move.l	D1,D6
	movea.l	D0,a1
	move.l	($1AC-4,SP),d0
	movea.l	D2,a0
	move.l	A3,-(SP)
	bsr	lbC003280
	add.l	D6,($1A8,SP)
	moveq	#$20,D0
	movea.l	($1AC,SP),A0
	cmp.b	(-1,A0),D0
	beq.b	\86
	move.l	($1A8,SP),D0
	cmpi.l	#$1FF,D0
	bge.b	\86
	addq.l	#1,($1A8,SP)
	moveq	#1,d1
	movea.l	A0,a1
	movea.l	D2,a0
	pea	($C5-16,SP)
	bsr	lbC003280
	addq.l	#1,D6
\86	tst.l	D6
	ble.b	\87
	move.l	D6,d0
	movea.l	($1B0-4,SP),a0
	move.l	A5,d1
	movea.l	($1C8-12,SP),a1
	bsr	lbC0031DE
	move.b	#2,($B4,SP)
\87	add.l	D6,($1AC,SP)
	moveq	#0,D6
	bra	\130

\88	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	ObtainSemaphore
	movea.l	A3,A0
	movea.l	(4,A0),A3
	move.l	A3,D0
	beq	\92
	move.b	#$20,($B5,SP)
	lea	($12,A3),A0
;	move.l	A0,-(SP)
	move.l	(14,A3),d1
	move.l	A0,a1
	bsr	lbC003CE6
	lea	($12,A3),A0
	movea.l	D0,A2
	sub.l	A0,D0
	move.l	(14,A3),D1
	lea	($9FC,A5),A3
	sub.l	D0,D1
	move.l	($1AC,SP),D2
	move.l	A3,D3
	sub.l	D3,D2
	move.l	#$1FF,D6
	sub.l	D2,D6
	cmp.l	D6,D1
	blt.b	\89
	move.l	D6,D1
\89	move.l	D1,D6
	move.l	($1AC-4,SP),d0
	movea.l	($1B4-8,SP),a1
	movea.l	D3,a0
	move.l	A2,-(SP)
	bsr	lbC003280
	add.l	D6,($1A8,SP)
	movea.l	($1AC,SP),A0
	cmpa.l	D3,A0
	bls.b	\90
	moveq	#$20,D0
	cmp.b	(-1,A0),D0
	beq.b	\90
	move.l	($1A8,SP),D0
	cmpi.l	#$1FF,D0
	bge.b	\90
	addq.l	#1,($1A8,SP)
	moveq	#1,d1
	movea.l	A0,a1
	movea.l	D3,a0
	pea	($C5-16,SP)
	bsr	lbC003280
	addq.l	#1,D6
\90	tst.l	D6
	ble.b	\91
	move.l	D6,d0
	movea.l	($1B0-4,SP),a0
	move.l	A5,d1
	movea.l	($1C8-12,SP),a1
	bsr	lbC0031DE
	move.b	#2,($B4,SP)
\91	add.l	D6,($1AC,SP)
	moveq	#0,D6
\92	movea.l	A5,A0
	sys	ReleaseSemaphore
	bra	\130

\93	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	#$1FF,D0
	move.l	($1A8,SP),D1
	sub.l	D1,D0
	move.l	($112,A5),D2
	cmp.l	D2,D0
	blt.b	\94
	move.l	D2,D0
\94	move.l	D0,D6
	ble.b	\95
	move.l	D1,d0
	move.l	D6,d1
	movea.l	($1B4-8,SP),a1
	lea	($9FC,A5),a0
	pea	($BFC,A5)
	bsr	lbC003280
	move.l	D6,d0
	lea	($BFC,A5),a0
	move.l	A5,d1
	move.l	A2,a1
	bsr	lbC0031DE
	add.l	D6,($1AC,SP)
	add.l	D6,($1A8,SP)
	move.b	#2,($B4,SP)
\95	moveq	#0,D6
	bra	\130

\96	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	($1A8,SP),D0
	movea.l	A5,A0
	adda.l	D0,A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc.b	\97
	lea	($9FC,A5),A1
	sub.l	A0,D0
	move.l	A1,D2
	add.l	D2,D0
	lea	($BFC,A5),A1
	movea.l	(_exec,A4),A6
	move.l	D0,D6
	jsr	(-$270,A6)
	move.l	D6,($112,A5)
	sub.l	D6,($1A8,SP)
	move.b	#2,($B4,SP)
\97	moveq	#0,D6
	bra	\130

\98	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A0
	lea	($9FC,A5),A3
	cmpa.l	A3,A0
	bls.b	\99
	move.l	A0,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
\99	clr.l	($1A8,SP)
	move.l	A3,($1AC,SP)
	move.b	#2,($B4,SP)
	bra	\130

\100	movea.l	($5E,SP),A0
	pea	($3C,SP)
	move.l	A2,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	clr.l	($76-8,SP)
;	move.l	A5,-(SP)
	move.l	A2,a0
	bsr	lbC00B96A
	lea	($1C0-11,SP),A0
	moveq	#1,d0
	move.l	A5,d1
	move.l	A2,a1
	bsr	lbC00B3A6
	move.b	#12,($17C,SP)
	moveq	#1,D6
	clr.l	($1A8,SP)
	lea	($9FC,A5),A0
	move.l	A0,($1AC,SP)
	bra	\130

\101	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	movea.l	($1AC,SP),A0
	lea	($9FC,A5),A3
	cmpa.l	A3,A0
	bls	\130
	move.l	A0,D1
	sub.l	A3,D1
	lea	($180-4,SP),a0
	bsr	lbC009998
	move.l	D0,D6
	move.l	A3,($1AC,SP)
	bra	\130

\102	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	($1A8,SP),D0
	movea.l	A5,A0
	adda.l	D0,A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\130
	sub.l	A0,D0
	lea	($9FC,A5),A1
	add.l	A1,D0
	move.l	D0,d1
	lea	($180-4,SP),a0
	bsr	lbC0099D6
	movea.l	A5,A0
	adda.l	($1A8,SP),A0
	lea	($9FC,A0),A1
	move.l	D0,D6
	move.l	A1,($1AC,SP)
	bra	\130

\103	moveq	#4,d0
\104	move.l	d0,d4
	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	tst.l	d4
	bne	\105
	move.w	($1C8-20,SP),D0
	movea.l	A5,a0
	bsr	lbC003F54
	bra	\130

\105	move.l	($1A8,SP),D0
	ble	\130
	moveq	#0,d0
	move.l	($1B0-4,SP),a0
	lea	($9FC,A5),a1
	bsr	lbC003596
	move.l	($1B8-12,SP),D1
	sub.l	D0,D1
	pea	($48-12,SP)
	move.l	D1,-(SP)
	move.l	D0,-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC0085E2
	lea	($20-12,SP),SP
	tst.l	D0
	beq.b	\106
	pea	($3C,SP)
	move.l	($1AC,SP),-(SP)
	move.l	($1B4,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003330
	lea	($14,SP),SP
\106	bsr	\24
	clr.l	($6E,SP)
	bra	\130

\107	movea.l	($5E,SP),A0
	pea	($3C,SP)
	move.l	A2,-(SP)
	jsr	(A0)
	clr.l	($76,SP)
	movea.l	(_exec,A4),A6
	movea.l	A5,A0
	sys	ObtainSemaphore
	move.w	($1BC,SP),D0
	pea	(1).W
	ext.l	D0
	move.l	D0,-(SP)
	move.l	($1B8,SP),-(SP)
	move.l	($1C0,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC003DEE
	lea	($20,SP),SP
	add.l	D0,D7
	clr.l	($1A8,SP)
	lea	($9FC,A5),A0
	move.l	A0,($1AC,SP)
	lea	($32,A5),A0
	move.l	A0,($38,SP)
	movea.l	A5,A0
	sys	ReleaseSemaphore
	moveq	#10,D1
	cmp.w	($1B4,SP),D1
	seq	D0
	neg.b	D0
	extb.l	D0
	move.l	D0,D4
	bra	\131

\108	bsr	\24
	clr.l	($6E,SP)
	movea.l	($98,A5),A1
	move.l	(io_Unit,A1),a3
	lea	(cu_RawEvents,A3),a2
	move.b	(A2),D2
	clr.b	(A2)
	movea.l	($9C,A5),A6
	movea.l	(io_Device,A6),A6
	movea.l	(io_Device,A1),A1
	lea	($10C,SP),A0
	sys	CDInputHandler
	move.b	d2,(A2)
	bra	\131

\109	moveq	#$64,d0
\110	bsr	lbC00BC02
	bra	\131

\111	moveq	#$2A,d0
	bra	\110

\112	moveq	#$12,d0
	bra	\110

\113	moveq	#$17,d0
	bra	\110

\114	moveq	#$63,d0
	bra	\110

\115	moveq	#$39,d0
	bra	\110

\116	bsr	\24
	clr.l	($6e,SP)
	movea.l	($98,A5),A3
	move.l	(io_Unit,A3),a3
	move.w	(cu_XRSize,A3),D0
	ext.l	D0
	move.w	(cu_XROrigin,A3),D1
	ext.l	D1
	move.w	($120,A5),D4
	ext.l	D4
	sub.l	D1,D4
	divs.l	D0,D4
	move.w	(cu_YRSize,A3),D0
	ext.l	D0
	move.w	(cu_YROrigin,A3),D1
	ext.l	D1
	move.w	($122,A5),D2
	ext.l	D2
	sub.l	D1,D2
	divs.l	D0,D2
	movea.l	A5,a0
	bsr	lbC00BD7E
	tst.l	D0
	beq.b	\117
	move.l	D2,-(SP)
	move.l	D4,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00BD98
	move.l	($1AC,SP),D2
	lea	($9FC,A5),A0
	sub.l	A0,D2
	sub.l	D2,D0
	move.l	D0,D4
	bra.b	\118

\117	move.w	($2A,A3),D0
	addq.w	#1,D0
	sub.w	($40,A3),D2
	muls.w	D0,D2
	add.w	D4,D2
	sub.w	($3E,A3),D2
	move.l	D2,D4
\118	tst.w	D4
	bpl.b	\119
	move.w	D4,D0
	lea	($9FC,A5),A0
	ext.l	D0
	move.l	A0,D1
	sub.l	($1AC,SP),D1
	cmp.l	D0,D1
	bgt.b	\119
	move.l	($1A8,SP),a0
	move.l	($1B0-4,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.w	D4,D1
	ext.l	D1
	neg.l	D1
	move.l	D1,-(SP)
	lea	($18C-12,SP),a0
	bsr	lbC009998
	lea	(4,SP),SP
	bra.b	\122

\119	tst.w	D4
	ble	\130
	movea.l	A5,A0
	move.l	($1A8,SP),D0
	adda.l	D0,A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\130
	move.l	A0,a1
	move.l	D0,a0
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	($1AC,SP),D0
	lea	($9FC,A5),A0
	sub.l	A0,D0
	move.l	($1A8,SP),D6
	sub.l	D0,D6
	move.w	D4,D0
	ext.l	D0
	cmp.l	D6,D0
	bge.b	\120
	move.w	D4,D0
	ext.l	D0
	bra.b	\121

\120	move.l	D6,D0
\121	move.l	D0,D4
	move.l	D0,d1
	lea	($180-4,SP),a0
	bsr	lbC0099D6
\122	movea.l	($1AC,SP),A0
	adda.w	D4,A0
	move.l	D0,D6
	move.l	A0,($1AC,SP)
	bra	\130

\123	bsr	\24	handle icon drop
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	move.l	($1BC-20,SP),-(SP)
	move.l	($1C0-16,SP),-(SP)
	pea	($9FC,A5)
	move.l	A5,-(SP)
	move.l	A2,-(SP)
	bsr	lbC00C92E
	lea	(20,SP),SP
	move.l	D0,D6
	ble	\130
	move.l	D6,d0
	movea.l	($1B0-4,SP),a0
	move.l	A5,d1
	movea.l	A2,a1
	bsr	lbC0031DE
	add.l	D6,($1A8,SP)
	add.l	D6,($1AC,SP)
	moveq	#0,D6
	move.b	#2,($B4,SP)
	bra	\130

\124	movea.l	A2,a0
	bsr	lbC008BA2
	bra	\131

\125	movea.l	A2,a0
	bsr	lbC008A98
	bra	\131

\126	bsr	\24
	clr.l	($76-8,SP)
	move.l	($1B0-8,SP),a0
	move.l	($1B4-8,SP),a1
	move.l	A5,d0
	move.l	A2,d1
	bsr	lbC0032BC
	cmpi.l	#$1FF,($1A8,SP)
	bge	\130
	moveq	#0,d0
	move.b	($1B5,SP),D0
	bsr	lbC009434
	beq.b	\127
	move.b	D0,D1
;	move.l	D1,-(SP)
	lea	($180-4,SP),a0
	bsr	lbC009A5C
	move.l	D0,D6
	bra.b	\128

\127	move.b	($1B5,SP),($17C,SP)
	moveq	#1,D6
\128	move.l	($1A8,SP),D0
	lea	($9FC,A5,d0.l),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc.b	\129
	lea	($1B4,SP),A1
	move.l	A1,D1
	addq.l	#1,D1
	addq.l	#1,($1AC,SP)
	addq.l	#1,($1A8,SP)
	move.l	D1,-(SP)
	moveq	#1,d1
	movea.l	A0,a1
	lea	($9FC,A5),a0
	bsr	lbC003280
	move.b	#1,($B4,SP)
	bra.b	\130

\129	addq.l	#1,($1AC,SP)
	move.w	($1B4,SP),D0
	move.b	D0,(A0)
	addq.l	#1,($1A8,SP)
\130	moveq	#0,D4
\131	movea.l	($1C0,SP),A5
	movea.l	($1BC,SP),A2
	move.l	D6,d0
	beq.b	\132
	lea	($180-4,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
\132	tst.b	($B4,SP)
	beq	\135
	movea.l	A5,A0
	adda.l	($1A8,SP),A0
	lea	($9FC,A0),A1
	movea.l	($1AC,SP),A0
	cmpa.l	A1,A0
	bcc	\135
	moveq	#0,d1
	lea	($17c,SP),a0
	bsr	lbC009A14
;	move.l	D0,-(SP)
	lea	($180-4,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
	lea	($9FC,A5),A0
	move.l	A0,D5
	move.l	($1bc-16,SP),D0
	move.l	($1B8-16,SP),D1
	sub.l	D0,D1
	add.l	D5,D1
	movea.l	d0,a0
	move.l	D1,d0
;	move.l	D0,-(SP)	a0
	move.l	A5,d1
	movea.l	A2,a1
	bsr	lbC0031DE
	moveq	#1,D0
	cmp.b	($B4,SP),D0
	bne.b	\133
	lea	($17C,SP),a0
	bsr	lbC009A84
	bra.b	\134

\133	lea	($17C,SP),a0
	bsr	lbC009A4A
\134	move.l	D0,D6
	move.l	($1AC,SP),D0
	move.l	($1A8,SP),D1
	sub.l	D0,D1
	lea	($17C,SP),A0
	add.l	D5,D1
	adda.l	D6,A0
	bsr	lbC009998
	add.l	D0,D6
	lea	($184-8,SP),A0
	moveq	#1,d1
	adda.l	D6,A0
	bsr	lbC009A14
	add.l	D0,D6
	move.l	D6,d0
	lea	($188-12,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
	bra.b	\138

\135	moveq	#1,D0
	cmp.b	($B4,SP),D0
	bne.b	\136
	lea	($17C,SP),a0
	bsr	lbC009A84
	bra.b	\137

\136	moveq	#2,D0
	cmp.b	($B4,SP),D0
	bne.b	\138
	lea	($17C,SP),a0
	bsr	lbC009A4A
\137	;move.l	D0,-(SP)
	lea	($180-4,SP),a0
	move.l	A5,a1
	move.l	A2,d1
	bsr	_doio
\138	addq.w	#1,($106,SP)
\139	move.w	($106,SP),D0
	cmp.w	($1A4,SP),D0
	movea.l	($38,SP),A3
	blt	\12
	tst.l	($1C4,SP)
	bne.b	\141
	move.l	($1A8,SP),D0
	bgt.b	\140
	tst.w	D4
	beq.b	\141
\140	tst.w	($104,A5)
	bne.b	\141
	move.l	D0,($116,A5)
	move.l	($1AC,SP),($11A,A5)
	move.l	A5,a0
	move.l	A2,a1
	bsr	lbC0014D4
	move.l	D0,($1C4,SP)
	lea	($1FC,A5),A0
	move.l	A0,($1B0,SP)
\141	tst.l	($1C4,SP)
	bne	\4
	movea.l	A5,a1
	bsr	lbC00C90E
	bne	\4
	move.l	A3,($60,A5)
	lea	($116,A5),A6
	clr.l	(A6)+
	lea	($9FC,A5),A0
	move.l	A0,(A6)+
	clr.w	(A6)+
	movea.l	($6A,SP),A0
	pea	($3C,SP)
	move.l	A2,-(SP)
	jsr	(A0)
	addq.l	#8,SP
	tst.l	D7
	bne.b	\142
	clr.w	($108,A5)
\142	move.l	D7,D0
\143	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$190,SP
	rtd	#12


lbC00D9B6	suba.w	#$14,SP
	movem.l	A2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	move.l	($168,A3),d0
	bne.b	\1
	btst	d0,($160,A3)
	beq.b	\2
	btst	#6,($160,A3)
	bne.b	\2
\1	;move.l	A3,-(SP)
;	move.l	A5,-(SP)
	bsr	lbC0018FA
	moveq	#0,D0
	bra	\8

\2	tst.b	($100,A3)
	beq	\7
	moveq	#EasyStruct_SIZEOF,D0
	lea	(16,SP),A2
	move.l	D0,(A2)+
	clr.l	(A2)+
	lea	(mess,pc),a1
	bsr	_getstr
	move.l	a1,(A2)+
	lea	(iwarn,pc),a1
	bsr	_getstr
	move.l	a1,(A2)+
	lea	(igadg,pc),a1
	move.l	a1,(A2)
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00170E
	move.l	A3,-(SP)
	lea	(Cancel.MSG,pc),a1
	bsr	_getstr
	pea	(a1)
	lea	(gMinimize,pc),a1
	bsr	_getstr
	pea	(a1)
	lea	(gIconify,pc),a1
	bsr	_getstr
	pea	(a1)
	movea.l	($78,A3),A0
	lea	(32,SP),A1
	movea.l	(8,A5),A6
	suba.l	A2,A2
	move.l	sp,A3
	sys	EasyRequestArgs
	lea	(12,sp),sp
	movea.l	(SP)+,A3
	subq.l	#1,D0
	beq.b	\3
	subq.l	#1,D0
	beq.b	\4
	bra.b	\5

\3	clr.b	($100,A3)
	bra.b	\5

\4	bsr	lbC0018FA
\5	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC001746
	tst.b	($100,A3)
	beq.b	\7
	moveq	#0,D0
	bra.b	\8

\7	moveq	#1,D0
\8	movem.l	(SP)+,A2/A3/A5/A6
	adda.w	#$14,SP
	rts


lbC0063B0	suba.w	#$8C,SP
	movem.l	D4-D7/A2/A3/A5,-(SP)
	movea.l	($B0,SP),A3
	lea	($1FC,A3),A0
	movea.l	($AC,SP),A5
	move.l	A0,($A4,SP)
	move.l	($116,A3),D7
	move.l	A0,($1C,SP)
	bra	\30

\1	tst.l	D7
	bne.b	\2
	move.b	#1,($A0,SP)
	bra.b	\3

\2	moveq	#3,D0
	move.b	D0,($A0,SP)
\3	moveq	#0,D0
	move.b	($A0,SP),D0
	movea.l	($A4,SP),A0
	move.l	D0,-(SP)
	pea	($2A,SP)
	pea	($7E,SP)
	pea	($C0,SP)
	pea	($B4,SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	move.l	A0,($3C,SP)
	bsr	lbC003FD8
	move.w	D0,D6
	moveq	#0,D5
	bra	\29

\4	move.w	($26,SP,D5.W*2),D4
	ext.l	d4
	movea.l	($1C,SP),A2
	move.l	D4,D0
	subq.l	#1,D0
	beq	\5
	subq.l	#2,D0
	beq	\22
	subq.l	#1,D0
	beq	\22
	subq.l	#1,D0
	beq	\22
	subq.l	#1,D0
	beq	\22
	subq.l	#2,D0
	beq	\11
	subq.l	#2,D0
	beq	\9
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\9
	subq.l	#3,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#2,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\10
	subq.l	#2,D0
	beq	\12
	subq.l	#2,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#2,D0
	beq	\5
	subq.l	#4,D0
	beq	\23
	moveq	#$5F,D1
	sub.l	D1,D0
	beq	\5
	subi.l	#$369,D0
	beq	\8
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\13
	subq.l	#1,D0
	beq	\14
	subq.l	#1,D0
	beq	\15
	subq.l	#1,D0
	beq	\17
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\19
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq	\18
	subq.l	#1,D0
	beq	\10
	subq.l	#2,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq	\28
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq	\9
	subq.l	#2,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#1,D0
	beq.b	\5
	subq.l	#4,D0
	beq.b	\5
	subq.l	#1,D0
	beq	\16
	subq.l	#1,D0
	beq	\16
	subq.l	#1,D0
	beq	\20
	subq.l	#1,D0
	beq	\21
	bra	\27

\5	move.l	($A4,SP),D0
	sub.l	($20,SP),D0
	add.l	D0,($B4,SP)
	bra.b	\7

\6	move.l	A2,D1
	lea	($1c,sp),a0
	sub.l	(a0)+,D1
	movea.l	(a0),A0
	move.b	(A0,D1.L),(A2)+
\7	movea.l	A3,A0
	adda.l	($B4,SP),A0
	lea	($1FC,A0),A1
	cmpa.l	A1,A2
	bcs.b	\6
	movea.l	A3,A0
	adda.l	D7,A0
	move.l	D7,($116,A3)
	lea	($9FC,A0),A1
	move.l	A1,($11A,A3)
	move.l	($B4,SP),-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC004C22
	moveq	#1,D0
	bra	\32

\8	clr.l	-(sp)
	movea.l	sp,a0
	moveq	#1,d0
	movea.l	a3,a1
	move.l	a5,d1
	bsr	_doio
	addq.l	#4,sp
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AF9E
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
	bra	\28

\9	clr.l	-(SP)
	move.l	D4,-(SP)
	movea.l	A3,A0
	adda.l	D7,A0
	move.l	D7,-(SP)
	pea	($9FC,A0)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC003DEE
	lea	($18,SP),SP
	moveq	#0,D7
	bra	\28

\10	moveq	#0,D7
	bra	\28

\11	tst.l	D7
	ble	\28
	subq.l	#1,D7
	bra	\28

\12	movea.l	A3,A1
	adda.l	D7,A1
	lea	($9FC,A1),A2
	movea.l	A2,a1
	move.l	D7,d0
	lea	($9FC,A3),a0
	bsr	lbC003C84
	move.l	A2,D1
	sub.l	D0,D1
	sub.l	D1,D7
	bra	\28

\13	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00D9B6
	tst.w	D0
	beq	\28
	clr.l	($116,A3)
	move.w	#1,($104,A3)
	bra	\31

\14	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00181E
	bra	\28

\15	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC0017B4
	bra	\28

\16	cmpi.w	#$418,D4
	seq	D0
	neg.b	D0
	extb.l	D0
	movea.l	a3,a0
	movea.l	a5,a1
	bsr	lbC00D882
	tst.w	D0
	beq	\28
	move.l	($78,A3),D0
	bne	\28
	bra	\32

\17	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00C830
	bra	\28

\18	move.l	($B4,SP),d0
	move.l	($A4,SP),a1
	move.l	A2,d1
	move.l	A5,a0
	bsr	lbC007226
	add.l	D0,($B4,SP)
	move.l	A2,($A4,SP)
	bra	\28

\19	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00C692
	bra	\28

\20	movea.l	A5,a0
	bsr	lbC008BA2
	bra	\28

\21	movea.l	A5,a0
	bsr	lbC008A98
	bra	\28

\22	move.l	D4,D0
	movea.l	A3,a0
	bsr	lbC003F54
	bra.b	\28

\24	move.l	A2,D1
	sub.l	($1C,SP),D1
	movea.l	($20,SP),A0
	move.b	(A0,D1.L),(A2)+
\25	movea.l	A3,A0
	adda.l	($B4,SP),A0
	lea	($1FC,A0),A1
	cmpa.l	A1,A2
	bcs.b	\24
	addq.l	#1,($B4,SP)
	movea.l	A3,A0
	adda.l	D7,A0
	move.b	#$20,(A2)
	move.l	D7,($116,A3)
	lea	($9FC,A0),A1
	move.l	A1,($11A,A3)
	move.l	($B4,SP),-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC004C22
	moveq	#1,D0
	bra.b	\32

\23	tst.l	D7
	beq.b	\25
\26	cmpi.l	#$1FF,D7
	bge.b	\28
	movea.l	A3,A0
	adda.l	D7,A0
	addq.l	#1,D7
	move.b	#$20,($9FC,A0)
	bra.b	\28

\27	tst.w	D4
	beq.b	\28
	cmpi.l	#$1FF,D7
	bge.b	\28
	movea.l	A3,A0
	adda.l	D7,A0
	addq.l	#1,D7
	move.b	D4,($9FC,A0)
\28	addq.w	#1,D5
\29	cmp.w	D6,D5
	blt	\4
\30	move.l	($B4,SP),D0
	bgt	\1
	movea.l	A3,A0
	adda.l	D7,A0
	move.l	D7,($116,A3)
	lea	($9FC,A0),A1
	move.l	A1,($11A,A3)
\31	moveq	#0,D0
\32	movem.l	(SP)+,D4-D7/A2/A3/A5
	adda.w	#$8C,SP
	rtd	#12

lbC00681A	moveq	#'/',D1
	cmp.b	D0,D1
	beq.b	\1
	moveq	#':',D1
	cmp.b	D0,D1
	beq.b	\1
	moveq	#' ',D1
	cmp.b	D0,D1
	beq.b	\1
	moveq	#0,D0
	bra.b	\2

\1	moveq	#1,D0
\2	rts

lbC00D882	suba.w	#$8C,SP
	movem.l	D7/A2/A3/A5/A6,-(SP)
	tst.b	d0
	movea.l	a0,A3
	movea.l	a1,A5
	bne.b	\5
	lea	($14,SP),a2
	movea.l	($88,A3),A0
	movea.l	A2,A1
	movea.l	(8,A5),A6
	sys	NextPubScreen
	tst.l	D0
	beq.b	\2
	movea.l	A2,A0
	sys	LockPubScreen
	movea.l	D0,A2
	tst.l	D0
	beq.b	\2
	cmpa.l	($88,A3),A2
	beq.b	\2
	bsr	lbC00D6B4
	tst.w	D0
	beq.b	\2
	move.l	A2,D0
	bra.b	\4

\2	move.l	A2,D0
	beq.b	\3
	movea.l	A2,A1
	suba.l	A0,A0
	sys	UnlockPubScreen
\3	moveq	#0,D0
\4	movea.l	D0,A2
	tst.l	D0
	bne.b	\6
	bra	\9

\5	pea	($14,SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00D784
	movea.l	D0,A2
	tst.l	D0
	beq.b	\9
\6	lea	($15FC,A3),A0
	movea.l	A0,A1
	lea	($14,SP),A0
	moveq	#$46,D0
	add.l	D0,D0
	movea.l	(a5),A6
	sys	CopyMem
	move.l	A3,a0
	bsr	lbC001596
	movea.l	A3,a0
	bsr	_CloseWindow
	moveq	#0,d0
	move.l	A3,a0
	move.l	A5,a1
	bsr	_OpenWindow
	tst.w	D0
	bne.b	\7
	clr.b	($15FC,A3)
	moveq	#0,d0
	move.l	A3,a0
	move.l	A5,a1
	bsr	_OpenWindow
\7	move.l	($88,A3),D0
	beq.b	\8
	movea.l	D0,A0
	movea.l	(8,A5),A6
	sys	ScreenToFront
\8	movea.l	A2,A1
	movea.l	(8,A5),A6
	suba.l	A0,A0
	sys	UnlockPubScreen
	moveq	#1,D0
	bra.b	\10

\9	moveq	#0,D0
\10	movem.l	(SP)+,D7/A2/A3/A5/A6
	adda.w	#$8C,SP
	rts

lbC00686A	suba.w	#$10,SP
	movem.l	D2-D4/D6/D7/A2/A3/A5/A6,-(SP)
	moveq	#-1,d4
	movea.l	($3C,SP),A3
	movea.l	($38,SP),A5
	clr.l	($30,SP)
	lea	($24,A5),a2
	move.l	(a2),d0
	bne	.ok
	lea	(asllibrary.MSG,pc),A1
	moveq	#$25,D0
	movea.l	(a5),a6
	sys	OpenLibrary
	move.l	D0,(A2)		(36,a5)
	beq	\14
.ok	movea.l	d0,A6
	suba.l	A0,A0
	moveq	#0,D0
	sys	AllocAslRequest
	move.l	D0,($2C,SP)
	beq	\13
	movea.l	sp,a2
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00170E
	move.w	($138,A3),D0
	cmp.w	d4,D0
	bne.b	\1
	moveq	#$50,D0
	lsl.l	#2,D0
\1	ext.l	D0
	move.l	($88,A3),a0
	bsr	lbC0073D8
	move.w	D0,D7
	ext.l	D7
	move.w	($13A,A3),D0
	cmp.w	d4,D0
	bne.b	\2
	moveq	#$64,D0
	add.l	D0,D0
\2	ext.l	D0
	move.l	($88,A3),a0
	bsr	lbC00744C
	ext.l	D0
	moveq	#0,d2
	move.w	($138,A3),D2
	cmp.w	d4,D2
	bne.b	\5
	moveq	#1,D6
	bra.b	\6

\5	move.l	#ASLFR_InitialWidth,D6
\6	move.w	($13A,A3),D1
	cmp.w	d4,D1
	bne.b	\7
	moveq	#1,D3
	bra.b	\8

\7	move.l	#ASLFR_InitialHeight,D3
\8	moveq	#0,D4
	move.w	D1,D4
	moveq	#$2A,D1
	cmp.b	($43,SP),D1
	bne.b	\9
	moveq	#$20,D1
	bra.b	\10

\9	moveq	#0,D1
\10	ori.w	#$10,D1
	clr.l	-(SP)
	move.l	D1,-(SP)
	move.l	#ASLFR_Flags1,-(SP)
	move.l	($5C-8,SP),-(SP)
	move.l	#ASLFR_InitialFile,-(SP)
	move.l	D4,-(SP)
	move.l	D3,-(SP)
	move.l	D2,-(SP)
	move.l	D6,-(SP)
	move.l	D0,-(SP)
	move.l	#ASLFR_InitialTopEdge,-(SP)
	move.l	D7,-(SP)
	move.l	#ASLFR_InitialLeftEdge,-(SP)
	move.l	($78,A3),-(SP)
	move.l	#ASLFO_Window,-(SP)
	move.l	($88-8,SP),-(SP)
	move.l	#ASLFR_TitleText,-(SP)
	movea.l	($78-8,SP),A0
	btst	#6,($162,A3)
	beq.b	\11
	moveq	#FRF_REJECTICONS,d1
	move.l	d1,-(sp)
	move.l	#ASLFR_Flags2,-(SP)
\11	movea.l	SP,A1
	sys	AslRequest
	movea.l	a2,sp
	movea.l	($2C,SP),A2
	tst.w	D0
	beq.b	\12
	movea.l	($4C,SP),A0
	move.l	(a0),d3
	clr.b	(A0)
	move.l	A0,D1
	move.l	(8,A2),D2
	movea.l	(4,A5),A6
	sys	AddPart
	move.l	($4C,SP),D1
	move.l	(4,A2),D2
	sys	AddPart
	movea.l	($4C,SP),a0
	bsr	_strlen
	lea	($16,A2),A6
	lea	($134,A3),A1
	move.l	(A6)+,(A1)+
	move.w	(A6)+,(A1)+
	move.w	($1C,A2),(A1)+
	move.l	D0,($30,SP)
\12	movea.l	A2,A0
	movea.l	($24,A5),A6
	sys	FreeAslRequest
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC001746
\13	move.l	($30,SP),D0
\14	movem.l	(SP)+,D2-D4/D6/D7/A2/A3/A5/A6
	adda.w	#$10,SP
	rts

lbC006A28	subq.l	#8,SP
	movem.l	D4-D7/A2/A3/A5/A6,-(SP)
	move.l	D1,D7
	movea.l	A1,A5
	move.l	D0,($20,SP)
	move.l	A0,($24,SP)
	movea.l	A5,a0
	bsr	_strlen
	move.l	D0,D6
	cmp.l	D0,D7
	slt	D1
	neg.b	D1
	move.l	($30,SP),D5
	extb.l	D1
	bne.b	lbC006A60
	cmp.l	D5,D6
	ble.b	lbC006A60
	moveq	#$3A,D0
	cmp.b	(-1,A5,D6.L),D0
	bne.b	lbC006A64
lbC006A60	move.l	D5,D0
	bra	lbC006ADE

lbC006A64	move.l	($20,SP),D0
	asl.l	#2,D0
	movea.l	D0,A3
	moveq	#0,D5
	move.b	(A3),D5
	movea.l	($24,SP),A0
	movea.l	A0,A2
	adda.l	D6,A2
	move.b	(A2),D0
	beq.b	lbC006A82
	moveq	#$2F,D1
	cmp.b	D1,D0
	bne.b	lbC006ADA
lbC006A82	movea.l	A0,A1
	movea.l	A5,A0

lbC006A0E	addq.l	#1,A0
	addq.l	#1,A1
lbC006A12	move.b	(A0),D0
	move.b	(A1),D1
	cmp.b	D0,D1
	bne.b	lbC006A1E
	tst.b	D0
	bne.b	lbC006A0E
lbC006A1E	tst.b	(A0)
	seq	D0
	neg.b	D0
	ext.w	D0
	beq.b	lbC006ADA
	move.l	D7,D4
	add.l	D5,D4
	sub.l	D6,D4
	move.l	D4,D0
	addq.l	#1,D0
	cmp.l	($34,SP),D0
	bge.b	lbC006ADA
	movea.l	A3,a0
	addq.l	#1,a0
	movea.l	($2C,SP),A3
	movea.l	A3,A1
	move.l	D5,D0
	movea.l	(_exec,A4),A6
	jsr	(-$270,A6)
	lea	(a3,d5.l),a1
	move.b	#':',(A1)+
	move.l	D7,D0
	sub.l	D6,D0
	lea	(1,A2),A0
	jsr	(-$270,A6)
	clr.b	(1,A3,D4.L)
	move.l	D6,D0
	bra.b	lbC006ADE

lbC006ADA	move.l	($30,SP),D0
lbC006ADE	movem.l	(SP)+,D4-D7/A2/A3/A5/A6
	addq.l	#8,SP
	rtd	#12

lbC006AE6	suba.w	#$A4,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	lea	($D0,SP),A6
	movea.l	(A6)+,A5
	move.l	($DC,SP),D1
	move.l	(A6)+,D5
	moveq	#0,D7
	movea.l	(A6)+,A2
	clr.b	(A2)
	moveq	#$50,D0
	cmp.l	D0,D1
	bgt.b	\1
	move.l	D1,D0
\1	move.l	D0,D6
	move.l	D0,D3
	move.l	D5,D1
	lea	($7B,SP),A0
	move.l	A0,D2
	movea.l	(4,A5),A6
	sys	NameFromLock
	tst.l	D0
	beq	\9
	asl.l	#2,D5
	movea.l	D5,A0
	move.l	(fl_Volume,A0),D5
	movea.l	D2,a0
	bsr	_strlen
	move.l	D0,D4
	moveq	#$3A,D1
	cmp.b	($7A,SP,D0.L),D1
	bne.b	\2
	move.l	D4,D0
	addq.l	#1,D0
	movea.l	D2,A0
	movea.l	A2,A1
	movea.l	(A5),A6
	sys	CopyMem
	move.l	D4,D0
	bra	\9

\2	movea.l	(4,A5),A6
	moveq	#$11,D1
	jsr	(-$28E,A6)
	movea.l	D0,A2
	bra	\7

\3	moveq	#1,D0
	cmp.l	(4,A2),D0
	bne	\7
	move.l	(12,A2),D0
	move.l	D0,D1
	asl.l	#2,D1
	movea.l	D1,A0
	move.l	($10,A0),D1
	cmp.l	D5,D1
	bne.b	\4
	move.l	D0,D1
	lea	($29,SP),A0
	move.l	A0,D2
	move.l	D6,D3
	movea.l	(4,A5),A6
	sys	NameFromLock
	tst.l	D0
	beq.b	\4
	move.l	D3,-(SP)
	move.l	D7,-(SP)
	move.l	A3,-(SP)
	movea.l	D2,A1
	move.l	($28,A2),D0
	move.l	D4,D1
	lea	($87,SP),A0
	bsr	lbC006A28
	move.l	D0,D7
\4	movea.l	($14,A2),A3
	move.l	A3,D0
	beq.b	\7
\5	move.l	(4,A3),D0
	move.l	D0,D1
	asl.l	#2,D1
	movea.l	D1,A0
	move.l	($10,A0),D1
	cmp.l	D5,D1
	bne.b	\6
	move.l	D0,D1
	lea	($29,SP),A0
	move.l	A0,D2
	move.l	D6,D3
	movea.l	(4,A5),A6
	sys	NameFromLock
	tst.l	D0
	beq.b	\6
	move.l	D3,-(SP)
	move.l	D7,-(SP)
	move.l	($E0,SP),-(SP)
	movea.l	D2,A1
	move.l	($28,A2),D0
	move.l	D4,D1
	lea	($87,SP),A0
	bsr	lbC006A28
	move.l	D0,D7
\6	movea.l	(A3),A3
	move.l	A3,D0
	bne.b	\5
\7	move.l	A2,D1
	movea.l	(4,A5),A6
	moveq	#$11,D2
	sys	NextDosEntry
	movea.l	D0,A2
	movea.l	($D8,SP),A3
	tst.l	D0
	bne	\3
	moveq	#$11,D1
	sys	UnLockDosList
	tst.l	D7
	bne.b	\8
	move.l	D4,D0
	addq.l	#1,D0
	lea	($7B,SP),A0
	movea.l	A3,A1
	movea.l	(A5),A6
	sys	CopyMem
	move.l	D4,D0
	bra.b	\9

\8	movea.l	A3,a0
	bsr	_strlen
\9	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$A4,SP
	rts

Task:	suba.w	#$8C,SP
	movem.l	D2-D7/A2-A6,-(SP)
	movea.l	(4).w,A6
	move.l	#SIGBREAKF_CTRL_F,d0
	sys	Wait
	suba.l	a1,a1
	sys	FindTask
	movea.l	d0,a0
	movea.l	(TC_Userdata,a0),a4
	suba.l	A5,A5
	clr.l	(TC_Userdata,a0)
	moveq	#1,D6
	lea	($74,SP),a2
	moveq	#-1,d2
	bsr	_OpenLibs
	beq	lbC007008
	sys	CreateMsgPort
	move.l	D0,($58,SP)
	beq	lbC007008
	movea.l	d0,a5
	lea	($3E,SP),A1
	movea.l	(_mport1,A4),A0
	sys	PutMsg

lbC006CA8	movea.l	A5,A0
	sys	WaitPort
	bra	lbC006FE6

_20	suba.l	A1,A1
	sys	FindTask
	movea.l	D0,A0
	move.l	(pr_WindowPtr,A0),D0
	move.l	(A2),(pr_WindowPtr,A0)
lbC006FD4	move.l	d0,d7
lbC006FD8	move.l	D7,($16,A3)	result
	movea.l	A3,A1
	movea.l	(_exec,A4),A6
	sys	ReplyMsg
lbC006FE6	movea.l	A5,A0
	sys	GetMsg
	movea.l	D0,A3
	tst.l	D0
	bne	lbC006CB6
	tst.w	D6
	bne	lbC006CA8
	movea.l	A5,A0
	sys	DeleteMsgPort
lbC007008	lea	($74,SP),a2
	bsr	_CloseLibs
	movea.l	(_locb,a4),a1
	sys	CloseLibrary
	move.l	A5,D0
	bne.b	lbC00702E
	st	(_dtf,a4)
	movea.l	(_mport1,A4),A0
	lea	($3E,SP),A1
	sys	PutMsg
lbC00702E	movem.l	(SP)+,D2-D7/A2-A6
	adda.w	#$8C,SP
	rts


lbC009E10	suba.w	#$200,SP
	movem.l	D2/D5-D7/A2/A3/A5/A6,-(SP)
	move.l	($230,SP),D7
	moveq	#0,D6
	movea.l	($228,SP),A5
	move.l	($22C,SP),D0
	move.l	A5,D1
	sub.l	D1,D0
	move.l	D7,D2
	add.l	D0,D2
	move.l	D1,a1
	move.l	D1,a0
	move.l	D2,d1
	bsr	lbC00325C
	movea.l	($224,SP),A3
	move.l	(32,A3),D0
	bne	\lib
	lea	(iffparselibra.MSG,pc),A1
	moveq	#37,D0
	sys	OpenLibrary
	move.l	D0,(32,A3)
	beq	\16
\lib	movea.l	D0,A6
	sys	AllocIFF
	movea.l	D0,A2
	tst.l	D0
	beq	\15
	moveq	#0,D0
	sys	OpenClipboard
	move.l	D0,(A2)
	beq	\14
	movea.l	A2,A0
	sys	InitIFFasClip
	movea.l	A2,A0
	moveq	#0,D0
	sys	OpenIFF
	tst.l	D0
	bne	\13
	movea.l	A2,A0
	move.l	#'FTXT',D0
	move.l	#'CHRS',D1
	sys	StopChunk
	tst.l	D0
	bne	\12
\1	movea.l	($224,SP),A3
	movea.l	A2,A0
	moveq	#0,D0
	sys	ParseIFF
	move.l	D0,D5
	addq.l	#2,D0
	beq.b	\1
	tst.l	D5
	bne	\12
	movea.l	A2,A0
	sys	CurrentChunk
	tst.l	D0
	beq.b	\1
	movea.l	D0,A0
	cmpi.l	#'FTXT',(12,A0)
	bne.b	\1
	cmpi.l	#'CHRS',(8,A0)
	bne.b	\1
	movea.l	($224,SP),A3
	bra	\11

\2	lea	($20,SP),a0
	lea	($120,SP),a1
\3	movem.l	D6/A2/A3/A5,-(SP)
	movea.l	a0,A5
	moveq	#0,D6
	movea.l	a1,A3
	bra.b	\9

\4	cmpi.b	#$9B,(A3)
	bne.b	\5
	lea	(1,A3),A0
	bsr	lbC00936C
	movea.l	D0,A3
	bra.b	\8

\5	move.b	(A3),D0
	moveq	#$1B,D1
	cmp.b	D1,D0
	beq.b	\8
	moveq	#9,D1
	cmp.b	D1,D0
	beq.b	\8
	moveq	#$1C,D1
	cmp.b	D1,D0
	beq.b	\8
	tst.b	D0
	beq.b	\8
	movea.l	A5,A2
	addq.l	#1,A5
	moveq	#10,D1
	cmp.b	D1,D0
	bne.b	\6
	moveq	#13,D1
	bra.b	\7

\6	moveq	#0,D1
	move.b	D0,D1
\7	move.b	D1,(A2)
	addq.l	#1,D6
\8	addq.l	#1,A3
\9	move.l	A3,D0
	sub.l	a1,D0
	cmp.l	D5,D0
	blt.b	\4
	move.l	D6,D0
	movem.l	(SP)+,D6/A2/A3/A5

	moveq	#$40,D1
	lsl.l	#5,D1
	sub.l	D7,D1
	sub.l	D6,D1
	cmp.l	D1,D0
	blt.b	\10
	move.l	D1,D0
\10	move.l	D0,D5
	move.l	D7,D1
	add.l	D6,D1
	exg	d0,d1
	movea.l	A5,a1
	movea.l	($234-12,SP),a0
	pea	($30-16,SP)
	bsr	lbC003280
	add.l	D5,D6
	adda.l	D5,A5
\11	move.l	D7,D0
	add.l	D6,D0
	cmpi.l	#$800,D0
	bge	\1
	movea.l	A2,A0
	lea	($120,SP),A1
	moveq	#$40,D0
	lsl.l	#2,D0
	sys	ReadChunkBytes
	move.l	D0,D5
	bgt	\2
	bra	\1

\12	movea.l	A2,A0
	sys	CloseIFF
\13	movea.l	(A2),A0
	sys	CloseClipboard
\14	movea.l	A2,A0
	sys	FreeIFF
\15	move.l	D6,D0
\16	movem.l	(SP)+,D2/D5-D7/A2/A3/A5/A6
	adda.w	#$200,SP
	rts


lbC006CB6	moveq	#0,D7
	lea	($1A,A3),A2
	moveq	#0,D0
	move.w	(20,A3),D0
	subq.l	#1,D0		1 OpenDiskFont()
	beq	_1
	subq.l	#1,D0		2 ExAll()
	beq	_2
	subq.l	#1,D0		3 DupLock() of pr_CurrentDir of a process
	beq	_3
	subq.l	#1,D0		4 CurrentDir()
	beq	_4
	subq.l	#1,D0		5 UnLock()
	beq	_5
	subq.l	#1,D0		6 Lock()
	beq	_6
	subq.l	#1,D0		7 DupLock() general function
	beq	_7
	subq.l	#1,D0		8
	beq	_8
	subq.l	#1,D0		9 filereq
	beq	_9
	subq.l	#1,D0		10 Write()
	beq	_10
	subq.l	#1,D0		11 Open()
	beq	_11
	subq.l	#1,D0		12 Close()
	beq	_12
	subq.l	#1,D0		13 SetProtection()
	beq	_13
	subq.l	#1,D0		14 NameFromLock()
	beq	_14
	subq.l	#1,D0		15 GetVar()
	beq	_15
	subq.l	#1,D0		16 GetDiskObject()
	beq	_16
	subq.l	#1,D0		17 FreeDiskObject()
	beq	_17
	subq.l	#1,D0		18
	beq.b	_18
	subq.l	#1,D0		19 scan dir and add to completer
	beq	_19
	subq.l	#1,D0		20 change pr_WindowPtr
	beq	_20
	subq.l	#1,d0		21 SameLock()
	beq	_sl
	subq.l	#1,d0		22 GetDeviceProc()
	beq	_gdp
	subq.l	#1,d0		23 OpenCatalog()
	beq	_ocat
	moveq	#77,D1		100 quit
	sub.l	D1,D0
	beq	_100
	bra	lbC006FD8

_18	move.l	(a2)+,d0
	movea.l	(A2)+,A0
	move.l	(A2),-(SP)
	move.l	A0,-(SP)
	move.l	d0,-(SP)
	pea	($80,SP)
	bsr	lbC006AE6
	lea	(16,SP),SP
	bra	lbC006FD4

_1	;lea	(xData.MSG,pc),A1
;	sys	Forbid
;	sys	FindTask
;	sys	Permit
;	tst.l	D0
;	bne.b	lbC006D86
	movea.l	(A2),A0
	movea.l	($84,SP),A6	gfx lib
	sys	OpenFont
	tst.l	d0
	bne	\1
	move.l	($8C,SP),d0
	bne	.ok
	bsr	_chk_env
	move.l	d7,d0
	beq	\1
	lea	(diskfontlibra.MSG,pc),A1
	moveq	#36,D0
	movea.l	(_exec,A4),A6
	sys	OpenLibrary
	move.l	D0,($8c,sp)
	beq	\1
.ok	movea.l	d0,a6
	movea.l	(A2),A0
	sys	OpenDiskFont
\1	bra	lbC006FD4

_2	movem.l	(4,a2),d1-d5
	movea.l	($78,SP),A6
	sys	ExAll
	move.l	D0,D7
	sys	IoErr
	move.l	D0,(A2)
	bra	lbC006FD8

_19	lea	(12,a2),a2
	move.l	(A2),-(SP)
	move.l	-(A2),-(SP)
	move.l	-(A2),-(SP)
	move.l	-(A2),-(SP)
	pea	($84,SP)
	bsr	lbC008044
	lea	(20,SP),SP
	bra	lbC006FD8

_3	movea.l	(A2),a0
	sys	Forbid
	bsr	lbC0094C2
	tst.l	D0
	beq.b	\1
	movea.l	d0,A0
	move.l	(pr_CurrentDir,A0),D0
	beq	\1
	move.l	d0,d1
	movea.l	($78,SP),A6
	sys	DupLock
	movea.l	(_exec,A4),A6
\1	sys	Permit
	bra	lbC006FD4

_7	move.l	(A2),D1
	movea.l	($78,SP),A6
	sys	DupLock
	bra	lbC006FD4

_4	move.l	(A2),D1
	movea.l	($78,SP),A6
	sys	CurrentDir
	bra	lbC006FD4

_5	move.l	(A2),D1
	movea.l	($78,SP),A6
	sys	UnLock
	bra	lbC006FD8

_6	movem.l	(A2),D1/D2
	movea.l	($78,SP),A6
	sys	Lock
	bra	lbC006FD4

_gdp	movem.l	(A2),D1/D2
	movea.l	($78,SP),A6
	sys	GetDeviceProc
	bra	lbC006FD4

_ocat	lea	(localelib,pc),a1
	moveq	#38,d0
	sys	OpenLibrary
	move.l	d0,(_locb,a4)		locale base
	beq	lbC006FD4
	suba.l	a0,a0
	lea	(_catname,pc),a1
	suba.l	a2,a2
	movea.l	d0,a6
	sys	OpenCatalogA
	move.l	d0,(_cat,a4)
	bra	lbC006FD4

_sl	movem.l	(A2),D1/D2
	movea.l	($78,SP),A6
	sys	SameLock
	bra	lbC006FD4

_8	movea.l	(A2)+,A0
	movea.l	(A2)+,A1
	move.l	(A2)+,-(SP)
	move.l	A1,-(SP)
	move.l	A0,-(SP)
	pea	($80,SP)
	bsr	lbC009E10
	lea	(16,SP),SP
	bra	lbC006FD4

_100	moveq	#0,D6
	suba.l	A1,A1
	sys	FindTask
	movea.l	D0,A1
	moveq	#10,D0
	sys	SetTaskPri
	bra	lbC006FD8

_9	movea.l	(A2)+,A0
	move.l	(A2)+,D0
	movea.l	(A2)+,A1
	move.l	(A2)+,($38,SP)
	move.l	(A2),-(SP)
	move.l	($3C,SP),-(SP)
	move.l	A1,-(SP)
	move.l	D0,-(SP)
	move.l	A0,-(SP)
	pea	($88,SP)
	bsr	lbC00686A
	lea	($18,SP),SP
	bra	lbC006FD4

_10	movem.l	(A2),D1-d3
	movea.l	($78,SP),A6
	sys	Write
	bra	lbC006FD4

_11	movem.l	(A2),d1/D2
	movea.l	($78,SP),A6
	sys	Open
	bra	lbC006FD4

_12	move.l	(A2),D1
	movea.l	($78,SP),A6
	sys	Close
	bra	lbC006FD8

_13	movem.l	(A2),D1/D2
	movea.l	($78,SP),A6
	sys	SetProtection
	bra	lbC006FD8

_14	movem.l	(A2),D1-d3	lock/buffer/length
	movea.l	($78,SP),A6
	sys	NameFromLock
	bra	lbC006FD4

_15	bsr.b	_chk_env
	beq	lbC006FD4
	movem.l	(A2),d1-d4
	sys	GetVar
	bra	lbC006FD4

_chk_env	movea.l	($7C,SP),A6
	moveq	#LDF_ALL|LDF_READ,d1
	sys	AttemptLockDosList
	move.l	D0,D7
	andi.l	#~1,d7
	beq.b	.fail
	move.l	D0,D1
	lea	(ENV.MSG,PC),A0
	move.l	A0,D2
	moveq	#LDF_ALL,d3
	sys	FindDosEntry
	move.l	D0,D7
	moveq	#LDF_ALL|LDF_READ,d1
	sys	UnLockDosList
.fail	tst.l	D7
	rts

_16	move.l	(A2),d0		if zero: default icon image
	beq	.defi		so we can skip env: checkin'
	bsr	_chk_env
	beq	lbC006FD8
.defi	move.l	($9C,SP),d0
	beq	lbC006FD4
	movea.l	d0,a6
	movea.l	(A2),a0
	sys	GetDiskObject
	bra	lbC006FD4

_17	move.l	(A2),d0
	beq	lbC006FD8
	movea.l	d0,a0
	movea.l	($9C,SP),a6
	sys	FreeDiskObject
	bra	lbC006FD8


_DoSyncDos	suba.w	#$38,SP
	movem.l	A5/A6,-(SP)
	lea	($44,SP),A5
	move.w	d0,($1E,SP)
	lea	($24,SP),A1
	moveq	#6,d0
.loop	move.l	(a5)+,(a1)+
	dbf	d0,.loop
	movea.l	($3c,a0),a0
	movea.l	(_mport1,a4),A5
	move.l	a5,($18,SP)
	lea	(10,SP),A1
	movea.l	(_exec,A4),A6
	sys	PutMsg
	movea.l	A5,A0
	sys	WaitPort
	movea.l	A5,A0
	sys	GetMsg
	move.l	($20,SP),D0	CCR must not change from here!
	movem.l	(SP)+,A5/A6
	adda.w	#$38,SP
	rts

_FindTask	moveq	#20,d0
_l1	move.l	a1,-(SP)
	bsr	_DoSyncDos
	addq.l	#4,SP
	rts

_OpenDiskFont	moveq	#1,d0
	bra	_l1

lbC0071B4	moveq	#3,d0
	bra	_l1

_CurrentDir	moveq	#4,d0
	bra	_l1

_DupLock	moveq	#7,d0
	bra	_l1

_UnLock	moveq	#5,d0
	bra	_l1

_GetDiskObject	moveq	#16,d0
	bra	_l1

_Lock	moveq	#6,d0
_l2	move.l	d1,-(SP)
	move.l	a1,-(SP)
	bsr	_DoSyncDos
	addq.l	#8,sp
	rts

_filereq	move.l	(4,SP),-(SP)
	pea	(nullbyte,pc)
	move.l	a1,-(SP)
	move.l	D0,-(SP)
	move.l	d1,-(SP)
	moveq	#9,d0		filereq
	bsr	_DoSyncDos
	lea	(20,SP),SP
	rtd	#4

lbC007226	move.l	d0,-(SP)
	move.l	a1,-(SP)
	move.l	d1,-(SP)
	moveq	#8,d0
	bra	__d

_Write	move.l	d0,-(SP)
	move.l	a1,-(SP)
	move.l	d1,-(SP)
	movea.l	a5,a0
	moveq	#10,d0
__d	bsr	_DoSyncDos
	lea	(12,SP),SP
	rts

_NameFromLock	move.l	d0,-(SP)
	move.l	a1,-(SP)
	move.l	d1,-(SP)
	moveq	#14,d0
	bra __d

lbC007360	moveq	#19,d0
	move.l	(16,SP),-(SP)
	move.l	(16,SP),-(SP)
	move.l	(16,SP),-(SP)
	move.l	(16,SP),-(SP)
	move.l	A5,a0
	bsr	_DoSyncDos
	lea	(16,SP),SP
	rtd	#16

lbC0073D8	movem.l	D2/D5/D7,-(SP)
	movea.l	($30,a0),A1
	movea.l	(8,A1),A1
	move.w	($48,A0),D5
	neg.w	D5
	move.l	d0,D1
	move.w	($20,A1),D0
	sub.w	($1C,A1),D0
	addq.w	#1,D0
	ext.l	D0
	sub.l	D1,D0
	move.l	D0,D7
	move.w	D5,D0
	ext.l	D0
	move.w	($16,SP),D1
	ext.l	D1
	bpl.b	\1
	addq.l	#1,D1
\1	asr.l	#1,D1
	move.w	($12,A0),D2
	ext.l	D2
	sub.l	D1,D2
	sub.l	D0,D2
	cmp.l	D2,D7
	bge.b	\2
	move.l	D7,D0
	bra.b	\3

\2	move.l	D2,D0
\3	tst.l	D0
	bpl.b	\4
	moveq	#0,D0
	bra.b	\6

\4	cmp.l	D2,D7
	bge.b	\5
	move.l	D7,D0
	bra.b	\6

\5	move.l	D2,D0
\6	move.w	D5,D1
	ext.l	D1
	add.l	D0,D1
	move.w	D1,D0
	movem.l	(SP)+,D2/D5/D7
	rts

lbC00744C	movem.l	D2/D5/D7,-(SP)
	movea.l	(a0,$30),A1
	movea.l	(8,A1),A1
	move.w	($4A,A0),D5
	neg.w	D5
	move.l	d0,D1
	move.w	($22,A1),D0
	sub.w	($1E,A1),D0
	addq.w	#1,D0
	ext.l	D0
	sub.l	D1,D0
	move.l	D0,D7
	move.w	D5,D0
	ext.l	D0
	tst.l	D1
	bpl.b	\1
	addq.l	#1,D1
\1	asr.l	#1,D1
	move.w	($10,A0),D2
	ext.l	D2
	sub.l	D1,D2
	sub.l	D0,D2
	cmp.l	D2,D7
	bge.b	\2
	move.l	D7,D0
	bra.b	\3

\2	move.l	D2,D0
\3	tst.l	D0
	bpl.b	\4
	moveq	#0,D0
	bra.b	\6

\4	cmp.l	D2,D7
	bge.b	\5
	move.l	D7,D0
	bra.b	\6

\5	move.l	D2,D0
\6	move.w	D5,D1
	ext.l	D1
	add.l	D0,D1
	move.w	D1,D0
	movem.l	(SP)+,D2/D5/D7
	rts

* open completer window

lbC0074C0	suba.w	#$40,SP
	movem.l	D2-D5/D7/A2/A3/A5/A6,-(SP)
	movea.l	($78,SP),A3
	movea.l	($68,SP),A5
	movea.l	($6C,SP),A1
	movea.l	([$78,A1],$2E),A2
	move.l	A2,($2E,SP)
	movea.l	($88,A1),A0
	movea.l	(8,A5),A6
	sys	GetScreenDrawInfo
	move.l	D0,(20,A3)
	beq	lbC007894
	movea.l	d0,A0
	moveq	#0,D0
	move.w	($10,A0),D0
	moveq	#0,D1
	move.w	(14,A0),D1
	move.l	D1,D2
	asl.l	#2,D2
	add.l	D1,D2
	add.l	D2,D2
	divs.l	D0,D2
	subq.l	#1,D2
	movea.l	($28,A2),A0
	move.w	(4,A0),D4
	move.w	D4,($32,SP)
	movea.l	($10,A5),A6
	sys	OpenFont
	tst.l	D0
	beq.b	lbC00753E
	movea.l	D0,A1
	move.w	(tf_XSize,A1),D5
	move.b	(tf_Flags,A1),($63,SP)
	sys	CloseFont
	bra.b	lbC00754A

lbC00753E	move.b	([$28,A2],7),($63,SP)
	move.w	D4,D5
lbC00754A	move.w	D5,($24,SP)
	movea.l	(A5),A6
	sys	Forbid
	movea.l	([$10,A5],$9A),A2
lbC007382	movem.l	D7/A3/A6,-(SP)
	move.l	(10,A2),a0
	bsr	_strlen
	move.l	D0,D7
	addq.l	#1,D7
	moveq	#9,D1
	add.l	D1,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(_exec,A4),A6
	sys	AllocVec
	tst.l	D0
	beq.b	lbC0073D0
	movea.l	D0,A3
	addq.l	#8,D0
	move.l	D0,(A3)
	movea.l	D0,A1
	movea.l	(10,A2),A0
	move.l	D7,D0
	sys	CopyMem
	lea	($14,A2),A6
	lea	(4,A3),A0
	move.l	(a6)+,(a0)+
;	move.w	(A6)+,(A0)+
;	move.b	(A6)+,(A0)+
;	move.b	(A6)+,(A0)+
lbC0073D0	move.l	A3,D0
	movem.l	(SP)+,D7/A3/A6
	move.l	D0,(8,A3)
	btst	#5,($63,SP)
	beq.b	lbC007584
	move.w	($18,A2),D5
	move.w	($14,A2),D1
	movea.l	D0,A2
	move.w	D1,($62,SP)
	bra.b	lbC007590

lbC007584	move.w	D4,($62,SP)
	movea.l	([$2E,SP],$28),A2
lbC007590	sys	Permit
	tst.l	(8,A3)
	bne.b	lbC0075B2
	movea.l	($6C,SP),A0
	movea.l	($88,A0),A0
	movea.l	(20,A3),A1
	movea.l	(8,A5),A6
	sys	FreeScreenDrawInfo
	bra	lbC007894

lbC0075B2	move.w	D4,D0
	ext.l	D0
	movea.l	($2E,SP),A0
	move.b	($23,A0),D4
	extb.l	D4
	add.l	D0,D4
	addq.l	#1,D4
	lea	(4,A3),A0
	clr.l	(a0)
	movea.l	(12,A5),A6
	sys	CreateContext
	move.l	([$6C,SP],$80),($4C,SP)
	move.l	A2,($42,SP)
	lea	($36,SP),A1
	move.w	#10,(A1)+
	move.w	D4,D1
	add.w	D2,D1
	move.w	D1,(A1)+
	ext.l	D5
	move.l	D5,D7
	asl.l	#5,D7
	sub.l	D5,D7
	move.l	D7,D1
	moveq	#$1C,D3
	add.l	D3,D1
	move.w	D1,(A1)+
	movea.l	($70,SP),A1
	move.l	D0,($2A,SP)
	move.l	(14,A1),D1
	moveq	#5,D3
	cmp.l	D3,D1
	blt.b	lbC007612
	move.l	D1,D3
lbC007612	moveq	#15,D0
	cmp.l	D0,D3
	bgt.b	lbC007620
	moveq	#5,D0
	cmp.l	D0,D1
	blt.b	lbC007620
	move.l	D1,D0
lbC007620	move.w	($62,SP),D4
	move.w	D4,D1
	ext.l	D1
	addq.l	#1,D1
	muls.l	D1,D0
	addq.l	#4,D0
	move.w	D0,($3C,SP)
	lea	(nullbyte,pc),A0
	move.l	A0,($3E,SP)
	clr.w	($46,SP)
	clr.l	($48,SP)
	moveq	#0,D1
	move.w	($7E,SP),D1
	move.l	#GTLV_ShowSelected,D0
	IFND	rom
	cmpi.w	#$27,($14,A6)
	bcc	\1
	moveq	#1,D0
	ENDC
\1	clr.l	-(SP)
	pea	(1).W
	move.l	#LAYOUTA_Spacing,-(SP)
	clr.l	-(SP)
	move.l	D0,-(SP)
	move.l	D1,-(SP)
	move.l	#GTLV_Selected,-(SP)
	move.l	A1,-(SP)
	move.l	#GTLV_Labels,-(SP)
	movea.l	($52-4,SP),A0
	lea	($5E-4,SP),A1
	moveq	#LISTVIEW_KIND,D0
	movea.l	SP,A2
	sys	CreateGadgetA
	lea	(36,SP),SP
	move.l	D0,(12,A3)
	beq	lbC007868
	movea.l	D0,A0
	move.w	(gg_Height,A0),d1
	add.w	d1,($38,SP)
	move.w	D4,D1
	addq.w	#5,D1
	move.w	D1,($3C,SP)
	move.w	#$FFFF,($46,SP)
	clr.l	-(SP)
	pea	(1).W
	move.l	#GTTX_Border,-(SP)
	lea	($46-4,SP),A1
	moveq	#TEXT_KIND,D0
	movea.l	SP,A2
	sys	CreateGadgetA
	lea	(12,SP),SP
	move.l	D0,(16,A3)
	beq	lbC007868
	move.l	([$2E,SP],$28),($42,SP)
	move.w	D2,D5
	lea	($38,SP),A2
	move.w	(A2),D1
	add.w	($3C,SP),D1
	add.w	D5,D1
	move.w	D1,(A2)+
	lea	(Cancel.MSG,pc),a1
	bsr	_getstr
	move.l	a1,d3
\t	tst.b	(a1)+
	bne	\t
	suba.l	d3,a1
	move.l	a1,d2
	lea	(OK.MSG,pc),a1
	bsr	_getstr
	movea.l	a1,a0
\t2	tst.b	(a0)+
	bne	\t2
	suba.l	a1,a0
	cmpa.l	d2,a0
	bcs	1$
	move.l	a0,d2
1$	addq.l	#1,d2
	move.w	($24,SP),D1
	move.w	D1,D0
	mulu	d2,d0
	sub.w	D1,D0
	add.w	d2,d0
	move.w	D0,(A2)+
	move.w	($32,SP),D1
	addq.w	#5,D1
	move.w	D1,(A2)+
	move.l	a1,(A2)+
	move.w	#1,($4a-4,SP)
	movea.l	(16,a3),A0
	lea	($36,SP),A1
	moveq	#BUTTON_KIND,D0
	suba.l	a2,a2
	sys	CreateGadgetA
	move.l	d0,d2
	beq	lbC007868
	move.w	($3A,SP),D1
	ext.l	D1
	move.l	D7,D0
	sub.l	D1,D0
	moveq	#$26,D1
	add.l	D1,D0
	move.w	D0,($36,SP)
	move.l	d3,($42-4,SP)
	move.w	#2,($4a-4,SP)
	movea.l	D2,A0
	lea	($3a-4,SP),A1
	moveq	#BUTTON_KIND,D0
	sys	CreateGadgetA
	tst.l	D0
	beq	lbC007868
	movea.l	D0,A0
	move.w	(4,A0),D1
	add.w	(8,A0),D1
	move.w	D1,D4
	addi.w	#10,D4
	move.w	(6,A0),D1
	add.w	(10,A0),D1
	add.w	D1,D5
	suba.l	A2,A2
	move.w	D4,D0
	ext.l	D0
	move.l	([$70-4,SP],$88),a0
	bsr	lbC0073D8
	move.w	D0,D7
	ext.l	D7
	move.w	D5,D0
	ext.l	D0
	move.l	([$74-8,SP],$88),a0
	bsr	lbC00744C
	ext.l	D0
	move.w	D4,D1
	ext.l	D1
	move.w	D5,D2
	ext.l	D2
	clr.l	-(SP)
	move.l	($32,SP),-(SP)
	move.l	#WA_PubScreen,-(SP)
	move.l	#IDCMP_INTUITICKS!IDCMP_CLOSEWINDOW!IDCMP_RAWKEY!IDCMP_GADGETUP!IDCMP_GADGETDOWN!IDCMP_MOUSEMOVE!IDCMP_MOUSEBUTTONS!IDCMP_REFRESHWINDOW,-(SP)
	move.l	#WA_IDCMP,-(SP)
	pea	(1).W
	move.l	#WA_SimpleRefresh,-(SP)
;	pea	(1).W
;	move.l	#WA_AutoAdjust,-(SP)
	pea	(1).W
	move.l	#WA_CloseGadget,-(SP)
	pea	(1).W
	move.l	#WA_Activate,-(SP)
	pea	(1).W
	move.l	#WA_DepthGadget,-(SP)
	pea	(1).W
	move.l	#WA_DragBar,-(SP)
	move.l	D2,-(SP)
	move.l	#WA_Height,-(SP)
	move.l	D1,-(SP)
	move.l	#WA_Width,-(SP)
	move.l	D0,-(SP)
	move.l	#WA_Top,-(SP)
	move.l	D7,-(SP)
	move.l	#WA_Left,-(SP)
	move.l	(4,A3),-(SP)
	move.l	#WA_Gadgets,-(SP)
	move.l	($E0-8,SP),-(SP)
	move.l	#WA_Title,-(SP)
	movea.l	A2,A0
	movea.l	(8,A5),A6
	movea.l	SP,A1
	sys	OpenWindowTagList
	lea	($74-8,SP),SP
	move.l	D0,(A3)
	beq.b	lbC007868
	movea.l	D0,A0
	movea.l	(12,A5),A6
	suba.l	A1,A1
	sys	GT_RefreshWindow
	move.l	(A3),D0
	bra.b	lbC007896

lbC007868	movea.l	(4,A3),A0
	movea.l	(12,A5),A6
	sys	FreeGadgets
	movea.l	($6C,SP),A0
	movea.l	($88,A0),A0
	movea.l	(20,A3),A1
	movea.l	(8,A5),A6
	sys	FreeScreenDrawInfo
	clr.l	(20,A3)
	clr.l	(A3)+
	clr.l	(A3)+
	clr.l	(A3)+
lbC007894	moveq	#0,D0
lbC007896	movem.l	(SP)+,D2-D5/D7/A2/A3/A5/A6
	adda.w	#$40,SP
	rts

lbC007932	suba.w	#$34,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	movea.l	($60,SP),A5
	move.w	#1,($54,SP)
	moveq	#-1,D0
	moveq	#0,D7
	clr.l	($4C,SP)
	movea.l	($64,SP),A3
	move.w	D0,($50,SP)
	cmp.w	($134,A3),d0
	bne.b	lbC00797E
	movea.l	($78,A3),A0
	move.w	(8,A0),D0
	bpl.b	lbC00796A
	addq.w	#1,D0
lbC00796A	asr.w	#1,D0
	move.w	(4,A0),D1
	add.w	D0,D1
	move.w	D1,($134,A3)
	move.w	(6,A0),D0
	move.w	D0,($136,A3)
lbC00797E	movea.l	(12,A5),A0
	moveq	#0,D0
	IFND	rom
	cmpi.w	#$27,($14,A0)
	bcs	\2
	ENDC
	not.w	D0
\2	move.l	A3,a0
	move.l	A5,a1
	move.w	D0,($5A-8,SP)
	bsr	lbC00170E
	movea.l	($70-8,SP),A2
	move.l	($74-8,SP),a1
;	move.l	A2,-(SP)
lbC007908	moveq	#0,D1
	move.l	A1,D0
	beq.b	lbC00792E
	movea.l	(a2),A0
	bra.b	lbC007928

lbC00791C	cmpa.l	A1,A0
	bne.b	lbC007924
	move.w	D1,D0
	bra.b	lbC00792E

lbC007924	movea.l	(A0),A0
	addq.w	#1,D1
lbC007928	tst.l	(A0)
	bne.b	lbC00791C
	moveq	#0,D0
lbC00792E	moveq	#0,D1
	move.w	D0,D1
	move.l	D1,-(SP)
	pea	($3C-8,SP)
	move.l	($80-8,SP),-(SP)
	move.l	A2,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	move.w	D0,($78-8,SP)
	bsr	lbC0074C0
	lea	(6*4,SP),SP
	tst.l	D0
	beq	lbC007DD4
	move.l	(14,A2),D0
	moveq	#5,D1
	cmp.l	D1,D0
	blt.b	lbC0079E0
	move.l	D0,D1
lbC0079E0	moveq	#15,D2
	cmp.l	D2,D1
	bgt.b	lbC0079F0
	moveq	#5,D1
	cmp.l	D1,D0
	blt.b	lbC0079EE
	move.l	D0,D1
lbC0079EE	move.l	D1,D2
lbC0079F0	move.w	D2,($2E,SP)
	bra	lbC007D92

lbC0079F8	movea.l	([$30,SP],$56),A1
	move.b	(15,A1),D1
	moveq	#1,D0
	lsl.l	d1,d0
	movea.l	(A5),A6
	sys	Wait
	bra	lbC007D7A

lbC007A18	lea	(20,A1),A0
	move.l	(A0)+,D6	Class
	move.w	(A0)+,D0	Code
	moveq	#3,D1
	and.w	(A0),D1		Qualifier
	moveq	#$30,D2
	and.w	(A0)+,D2
	movea.l	(A0),A2		IAddress
	move.l	($24,A1),D5	Seconds
	move.l	($28,A1),D4	Micros
	move.w	D0,($2C,SP)
	move.w	D1,($2A,SP)
*	movea.l	A3,A1
	movea.l	(12,A5),A6
	sys	GT_ReplyIMsg
	move.w	D2,($28,SP)
	move.l	D6,D0
	subq.l	#4,D0
	beq	lbC007C64
	moveq	#$1C,D1
	sub.l	D1,D0
	beq.b	lbC007A76
	moveq	#$20,D1
	sub.l	D1,D0
	beq.b	lbC007A76
	subi.l	#$1C0,D0
	beq	lbC007C4C
	subi.l	#$200,D0
	beq.b	lbC007AD6
	bra	lbC007C84

lbC007A76	moveq	#0,D0
	move.w	($26,A2),D0
	beq.b	lbC007A8C
	subq.l	#1,D0
	beq.b	lbC007AC0
	subq.l	#1,D0
	beq.b	lbC007AC8
	bra	lbC007C84

lbC007A8C	move.w	($2C,SP),D0
	move.w	D0,($58,SP)
	move.w	($50,SP),D1
	cmp.w	D0,D1
	bne.b	lbC007AB6
	move.l	D7,D0
	move.l	($4C,SP),D1
	move.l	D5,D2
	move.l	D4,D3
	movea.l	(8,A5),A6
	sys	DoubleClick
	tst.w	D0
	beq.b	lbC007AB6
	clr.w	($54,SP)
lbC007AB6	move.l	D5,D7
	move.l	D4,($4C,SP)
	bra	lbC007C84

lbC007AC0	clr.w	($54,SP)
	bra	lbC007C84

lbC007AC8	move.w	#$FFFF,($58,SP)
	clr.w	($54,SP)
	bra	lbC007C84

lbC007AD6	movea.l	($68,SP),A3
	move.w	($2C,SP),D0
	extb.l	D0
	moveq	#$42,D1
	sub.l	D1,D0
	blt	lbC007C84
	cmpi.l	#12,D0
	bge	lbC007C84
	move.w	(lbW007AFA,PC,D0.W*2),D0
	jmp	(lbW007AFC,PC,D0.W)

lbW007AFA	dc.w	lbC007B12-lbW007AFC
lbW007AFC	dc.w	lbC007C46-lbW007AFC
	dc.w	lbC007C46-lbW007AFC
	dc.w	lbC007C4C-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007C84-lbW007AFC
	dc.w	lbC007B5A-lbW007AFC
	dc.w	lbC007BCA-lbW007AFC

lbC007B12	tst.w	($2A,SP)
	beq.b	lbC007B38
	move.w	($58,SP),D0
	bne.b	lbC007B28
	move.l	(14,A3),D1
	move.l	D1,D2
	subq.l	#1,D2
	bra.b	lbC007B30

lbC007B28	moveq	#0,D1
	move.w	D0,D1
	subq.l	#1,D1
	move.l	D1,D2
lbC007B30	move.w	D2,($58,SP)
	bra	lbC007C84

lbC007B38	move.l	(14,A3),D0
	subq.l	#1,D0
	moveq	#0,D1
	move.w	($58,SP),D1
	cmp.l	D0,D1
	bne.b	lbC007B4C
	moveq	#0,D0
	bra.b	lbC007B52

lbC007B4C	moveq	#0,D0
	move.w	D1,D0
	addq.l	#1,D0
lbC007B52	move.w	D0,($58,SP)
	bra	lbC007C84

lbC007B5A	move.w	($2A,SP),D0
	beq.b	lbC007B70
	move.w	($28,SP),D1
	bne.b	lbC007B70
	moveq	#0,D2
	move.w	D2,($58,SP)
	bra	lbC007C84

lbC007B70	move.w	($28,SP),D1
	beq.b	lbC007BA4
	tst.w	D0
	bne.b	lbC007BA4
	move.w	($58,SP),D2
	bne.b	lbC007B88
	move.l	(14,A3),D3
	subq.l	#1,D3
	bra.b	lbC007B9C

lbC007B88	moveq	#0,D3
	move.w	($2E,SP),D3
	moveq	#0,D0
	move.w	D2,D0
	sub.l	D3,D0
	addq.l	#1,D0
	bge.b	lbC007B9A
	moveq	#0,D0
lbC007B9A	move.l	D0,D3
lbC007B9C	move.w	D3,($58,SP)
	bra	lbC007C84

lbC007BA4	tst.w	D1
	bne	lbC007C84
	tst.w	D0
	bne	lbC007C84
	move.w	($58,SP),D2
	bne.b	lbC007BBC
	move.l	(14,A3),D0
	bra.b	lbC007BC0

lbC007BBC	moveq	#0,D0
	move.w	D2,D0
lbC007BC0	subq.l	#1,D0
	move.w	D0,($58,SP)
	bra	lbC007C84

lbC007BCA	move.w	($2A,SP),D0
	beq.b	lbC007BE4
	move.w	($28,SP),D1
	bne.b	lbC007BE4
	move.l	(14,A3),D3
	subq.l	#1,D3
	move.w	D3,($58,SP)
	bra	lbC007C84

lbC007BE4	move.w	($28,SP),D1
	beq.b	lbC007C1E
	tst.w	D0
	bne.b	lbC007C1E
	move.l	(14,A3),D6
	subq.l	#1,D6
	moveq	#0,D2
	move.w	($58,SP),D2
	cmp.l	D6,D2
	bne.b	lbC007C02
	moveq	#0,D3
	bra.b	lbC007C18

lbC007C02	moveq	#0,D3
	move.w	($2E,SP),D3
	moveq	#0,D0
	move.w	D2,D0
	add.l	D3,D0
	subq.l	#1,D0
	cmp.l	D0,D6
	bge.b	lbC007C16
	move.l	D6,D0
lbC007C16	move.l	D0,D3
lbC007C18	move.w	D3,($58,SP)
	bra.b	lbC007C84

lbC007C1E	tst.w	D1
	bne.b	lbC007C84
	tst.w	D0
	bne.b	lbC007C84
	move.l	(14,A3),D0
	subq.l	#1,D0
	moveq	#0,D1
	move.w	($58,SP),D1
	cmp.l	D0,D1
	bne.b	lbC007C3A
	moveq	#0,D0
	bra.b	lbC007C40

lbC007C3A	moveq	#0,D0
	move.w	D1,D0
	addq.l	#1,D0
lbC007C40	move.w	D0,($58,SP)
	bra.b	lbC007C84

lbC007C4C	move.w	#$FFFF,($58,SP)
lbC007C46	clr.w	($54,SP)
	bra.b	lbC007C84

lbC007C64	movea.l	($30,SP),A0
	movea.l	(12,A5),A6
	jsr	(-$5A,A6)
	movea.l	($30,SP),A0
	suba.l	A1,A1
	jsr	(-$54,A6)
	movea.l	($30,SP),A0
	moveq	#1,D0
	jsr	(-$60,A6)
lbC007C84	move.w	($58,SP),D0
	move.w	($50,SP),D1
	cmp.w	D0,D1
	beq	lbC007D7A
	movea.l	(12,A5),A2
	IFND	rom
	cmpi.w	#$27,($14,A2)
	bcc.b	lbC007CCC
	moveq	#0,D1
	move.w	($52,SP),D1
	moveq	#0,D2
	move.w	($2E,SP),D2
	move.l	D2,D3
	add.l	D1,D3
	moveq	#0,D1
	move.w	D0,D1
	cmp.l	D3,D1
	blt.b	lbC007CC2
	move.w	D0,D1
	sub.w	D2,D1
	addq.w	#1,D1
	move.w	D1,($52,SP)
	bra.b	lbC007CCC

lbC007CC2	cmp.w	($52,SP),D0
	bcc.b	lbC007CCC
	move.w	D0,($52,SP)
	ENDC
lbC007CCC	suba.l	A3,A3
	moveq	#0,D6
	move.w	D0,D6
	move.w	($52,SP),D1
	cmpi.w	#$FFFF,D1
	bne.b	lbC007CE0
	moveq	#1,D2
	bra.b	lbC007CE6

lbC007CE0	move.l	#$80080005,D2
lbC007CE6	moveq	#0,D3
	move.w	D1,D3
	moveq	#0,D1
	move.w	D0,D1
	movem.l	A2/A3,-(SP)
	clr.l	-(SP)
	move.l	D1,-(SP)
	move.l	#$8008004E,-(SP)
	move.l	D3,-(SP)
	move.l	D2,-(SP)
	move.l	D6,-(SP)
	move.l	#$80080036,-(SP)
	exg	A2,A3
	movea.l	($60,SP),A0
	movea.l	($54,SP),A1
	movea.l	A3,A6
	movea.l	SP,A3
	jsr	(-$2A,A6)
	lea	($1C,SP),SP
	movem.l	(SP)+,A2/A3
	suba.l	A3,A3
	move.w	($58,SP),D0
	cmpi.w	#$FFFF,D0
	bne.b	lbC007D34
	lea	(nullbyte,pc),A0
	bra.b	lbC007D4A

lbC007D34	move.w	D0,D1
	movea.l	($68,SP),a0
	bsr	lbC0078E6
	movea.l	D0,A1
	movea.l	(10,A1),A0
lbC007D4A	movem.l	A2/A3,-(SP)
	clr.l	-(SP)
	move.l	A0,-(SP)
	move.l	#GTTX_Text,-(SP)
	movea.l	A3,A2
	movea.l	($54,SP),A0
	movea.l	($44,SP),A1
	movea.l	(12,A5),A6
	movea.l	SP,A3
	sys	GT_SetGadgetAttrsA
	lea	(12,SP),SP
	movem.l	(SP)+,A2/A3
	move.w	($58,SP),($50,SP)
lbC007D7A	movea.l	($30,SP),A0
	movea.l	($56,A0),A0
	movea.l	(12,A5),A6
	sys	GT_GetIMsg
	movea.l	D0,A1
	tst.l	D0
	bne	lbC007A18
lbC007D92	tst.w	($54,SP)
	bne	lbC0079F8
	movea.l	($30,SP),A0
	movea.l	($64,SP),A3
	move.w	(4,A0),D0
	move.w	D0,($134,A3)
	move.w	(6,A0),D0
	move.w	D0,($136,A3)
	movea.l	($78,A3),A0
	move.l	A0,D0
	beq.b	lbC007DC8
	moveq	#0,d0
	bsr	lbC00D666
lbC007DC8	lea	($30,SP),a0
lbC0078A0	movem.l	A2/A3/A6,-(SP)
	movea.l	a0,A3
	movea.l	(A3),A0
	movea.l	($2E,A0),A2
	movea.l	(8,A5),A6
	sys	CloseWindow
	movea.l	(4,A3),A0
	movea.l	(12,A5),A6
	sys	FreeGadgets
	movea.l	(8,A3),A1
	movea.l	(a5),A6
	sys	FreeVec
	movea.l	A2,A0
	movea.l	(20,A3),A1
	movea.l	(8,A5),A6
	sys	FreeScreenDrawInfo
	movem.l	(SP)+,A2/A3/A6

lbC007DD4	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC001746
	move.w	($58,SP),D0
	cmpi.w	#$FFFF,D0
	bne.b	lbC007DEC
	moveq	#0,D0
	bra.b	lbC007DFE

lbC007DEC	move.w	($58,SP),D1
	movea.l	($68,SP),a0
	bsr	lbC0078E6
lbC007DFE	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$34,SP
	rtd	#20

lbC0078E6	movea.l	(a0),a0
	tst.w	d1
	bra.b	\2

\1	movea.l	(A0),A0
	subq.w	#1,D1
\2	bhi.b	\1
	move.l	A0,D0
	rts

lbC007EA2	move.l	(8,A1),D0
	move.l	($10,A1),D1
	movea.l	($10,A0),A0
	movea.l	(4,A1),A1
lbC007E08	;subq.l	#4,SP
	movem.l	D2/D7/A2/A3/A5/A6,-(SP)
	extb.l	D0
	move.l	D0,D7
	movea.l	A1,A3
	movea.l	A0,A5
	movea.l	($10,A5),A2
;	move.l	D1,($18,SP)
	move.w	(12,A5),D0
	moveq	#1,D2
	cmp.w	D2,D0
	bne.b	\1
	tst.b	D7
	bpl.b	\6
	moveq	#$42,D2
	and.l	D2,D1
	subq.l	#2,D1
	beq.b	\6
\1	subq.w	#2,D0
	bne.b	\2
	tst.b	D7
	bmi.b	\6
\2	tst.w	(14,A5)
	beq.b	\3
	movea.l	A3,a0
	bsr	_strlen
	moveq	#5,D1
	cmp.l	D1,D0
	blt.b	\3
	movea.l	A3,A0
	adda.l	D0,A0
	subq.l	#5,A0
	move.l	D1,D0
	lea	(info.MSG0,pc),A1
	movea.l	($1C,A2),A6
	sys	Strnicmp
	tst.l	D0
	beq.b	\7
\3	tst.l	(A5)
	bne.b	\4
	movea.l	A3,A0
	movea.l	(4,A5),A1
	move.l	(8,A5),D0
	movea.l	($1C,A2),A6
	sys	Strnicmp
	tst.l	D0
	beq.b	\5
\4	move.l	(A5),D0
	beq.b	\7
	move.l	D0,D1
	move.l	A3,D2
	movea.l	(4,A2),A6
	sys	MatchPatternNoCase
	tst.l	D0
	beq.b	\7
\5	moveq	#1,D0
	bra.b	\7

\6	moveq	#0,D0
\7	movem.l	(SP)+,D2/D7/A2/A3/A5/A6
;	addq.l	#4,SP
	rts

_sort	movem.l	d2/A2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	movea.l	d0,A2
	move.l	d0,d2
	cmpi.l	#2,(14,A2)
	blt.b	\5
	movea.l	A3,A1
	movea.l	(A5),A6
	sys	Remove
	movea.l	A2,A0
	movea.l	(8,A0),A2
	bra.b	\3

\1	move.b	(9,A2),D0
	move.b	(9,A3),D1
	cmp.b	D0,D1
	blt.b	\4
;	cmp.b	D0,D1
	bne.b	\2
	lea	($12,A3),A0
	lea	($12,A2),A1
	movea.l	($1C,A5),A6
	sys	Stricmp
	tst.l	D0
	bgt.b	\4
\2	movea.l	A2,A0
	movea.l	(4,A0),A2
\3	tst.l	(4,A2)
	bne.b	\1
\4	movea.l	d2,A0
	movea.l	A3,A1
	movea.l	(A5),A6
	sys	Insert
\5	movem.l	(SP)+,d2/A2/A3/A5/A6
	rts

lbC008044	suba.w	#$70,SP
	movem.l	D2/D3/D6/D7/A2/A3/A5/A6,-(SP)
	lea	($94,sp),a0
	movea.l	(a0)+,a5
	movea.l	(a0)+,a3
	move.l	(a0)+,d7
	movea.l	(a0)+,a2
;	move.l	($9C,SP),D7
;	movea.l	($A0,SP),A2
;	movea.l	($94,SP),A5
;	movea.l	($98,SP),A3
	movea.l	(4,A2),A1
	move.l	a1,d1
	adda.l	(8,A2),A1
	move.b	(A1),D6
	clr.b	(A1)
	lea	($22,SP),A0
	move.l	A0,D2
	movea.l	(4,A5),A6
	moveq	#$5A,D3
	sys	ParsePatternNoCase
	subq.l	#1,D0
	bne.b	\1
	move.l	D2,(A2)
	bra.b	\2

\1	clr.l	(A2)
\2	movea.l	(4,A2),A0
	adda.l	(8,A2),A0
	move.b	D6,(A0)
	move.l	A2,($8C,SP)
	lea	(lbC007EA2,PC),A0
	move.l	A0,($84,SP)
	btst	#1,($167,A3)
	beq.b	\3
	move.l	($A4,SP),-(SP)
	move.l	A2,-(SP)
	move.l	D7,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC0091DC
	lea	(20,SP),SP
	tst.w	D0
	bne.b	\4
\3	pea	($7C,SP)
	move.l	($A8,SP),-(SP)
	move.l	A2,-(SP)
	move.l	D7,-(SP)
	move.l	A5,-(SP)
	bsr	lbC007F40
	lea	(20,SP),SP
\4	movem.l	(SP)+,D2/D3/D6/D7/A2/A3/A5/A6
	adda.w	#$70,SP
	rts

lbC007F40	subq.l	#8,SP
	movem.l	D2-D7/A3/A5/A6,-(SP)
	move.l	($34,SP),D7
	movea.l	($30,SP),A5
	movea.l	(4,A5),A6
	moveq	#1,D1
	moveq	#0,D2
	sys	AllocDosObject
	movea.l	D0,A3
	move.l	D0,($28,SP)
	beq	\10
	move.l	($40,SP),(12,A3)
	clr.l	(4,A3)
	moveq	#$40,D0
	lsl.l	#5,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(A5),A6
	sys	AllocVec
	move.l	D0,($24,SP)
	beq	\9
\1	movea.l	($24,SP),A0
	move.l	D5,-(SP)
	move.l	D7,D1
	move.l	A0,D2
	move.l	A3,D5
	movea.l	(4,A5),A6
	moveq	#$40,D3
	lsl.l	#5,D3
	moveq	#4,D4
	sys	ExAll
	move.l	(SP)+,D1
	move.l	D0,D5
	bne.b	\2
	sys	IoErr
	moveq	#$74,D1
	add.l	D1,D1
	cmp.l	D1,D0
	bne.b	\8
\2	tst.l	(A3)
	bls.b	\7
	movea.l	D2,A3
\3	movea.l	(4,A3),a0
	bsr	_strlen
	move.l	D0,D6
	addq.l	#1,D6
	movea.l	($3c,SP),A1
	movea.l	($1E,A1),A0
	move.l	D6,-(SP)
	move.l	(4,A3),-(SP)
	move.l	A1,-(SP)
	move.l	A5,-(SP)
	jsr	(A0)
	lea	($10,SP),SP
	tst.l	D0
	beq.b	\6
	movea.l	d0,a0
	move.l	(8,A3),D0
	bmi.b	\4
	moveq	#'/',D1
	bra.b	\5

\4	moveq	#' ',D1
\5	move.b	D1,($11,A0,D6.L)
	tst.l	d0
	smi	D0
	moveq	#1,D1
	sub.b	D0,D1
	move.b	D1,(9,A0)
	move.l	($3c,SP),d0
	movea.l	A5,a1
	bsr	_sort
\6	movea.l	(A3),A3
	move.l	A3,D0
	bne.b	\3
\7	movea.l	($28,SP),A3
	tst.w	D5
	bne	\1
\8	movea.l	($24,SP),A1
	movea.l	(A5),A6
	sys	FreeVec
\9	move.l	A3,D2
	movea.l	(4,A5),A6
	moveq	#1,D1
	sys	FreeDosObject
\10	movem.l	(SP)+,D2-D7/A3/A5/A6
	addq.l	#8,SP
	rts

lbC0080DC	suba.w	#276,SP
	movem.l	D2/D4/D5/D7/A2/A3/A5/A6,-(SP)
	movea.l	(140+176,SP),A3
	movea.l	(136+176,SP),A5
	move.l	(148+176,SP),D2
	move.l	A3,a1
	move.l	A5,a0
	bsr	lbC0071B4
	move.l	D0,D7
	beq.b	lbC008108
	move.l	D7,a1
	move.l	A5,a0
	bsr	_CurrentDir	;CurrentDir
lbC008108	tst.l	D2
	bgt.b	lbC008120
	btst	#0,($17F,A3)
	bne	lbC008202
	btst	#1,($162,A3)
	bne	lbC008202
lbC008120	move.l	(144+176,SP),D1
	movea.l	(4,A5),A6
	sys	PathPart
	movea.l	D0,A2
	movea.l	D0,A0
	move.b	(A0),D4
	clr.b	(A0)
	moveq	#-2,d1
	move.l	(144+176,SP),a1
	move.l	A5,a0
	bsr	_Lock
	move.b	D4,(A2)
	move.l	D0,D5
	bne.b	lbC00816C
	tst.l	D7
	beq.b	lbC008166
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
lbC008166	moveq	#0,D0
	bra	lbC008282

lbC00816C	movea.l	(144+176,SP),A2
	move.l	A2,D1
	movea.l	(4,A5),A6
	jsr	(-$366,A6)
	lea	(116+176,SP),A6
	move.l	D0,(A6)+
	sub.l	A2,D0
	move.l	D2,D1
	sub.l	D0,D1
	move.l	D1,(A6)+
	clr.w	(A6)+
	moveq	#$40,D0
	lsl.l	#8,D0
	and.l	($160,A3),D0
	move.w	D0,(126+176,SP)
	move.l	A5,(128+176,SP)
	move.l	($78,A3),D0
	beq.b	lbC0081B0
	movea.l	D0,a0
	moveq	#1,d0
	bsr	lbC00D666
lbC0081B0	move.l	(152+176,SP),-(SP)
	pea	(116+176,SP)
	move.l	D5,-(SP)
	move.l	A3,-(SP)
	bsr	lbC007360
	move.l	($78,A3),D0
	beq.b	lbC0081DA
	movea.l	d0,a0
	moveq	#0,d0
	bsr	lbC00D666
lbC0081DA	move.l	D5,a1
	move.l	A5,a0
	bsr	_UnLock
	movea.l	(152+176,SP),A0
	tst.l	(14,A0)
	bne	lbC008262
	move.l	A0,-(SP)
	move.l	D2,-(SP)
	move.l	A2,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00828C
	bra	lbC008262

lbC008202	moveq	#64,d2
	lsl.l	#2,d2	def max path length = 256
	lea	(Selectfilenam.MSG,pc),a1
	bsr	_getstr
	moveq	#$7B,d0
	move.l	a3,d1
	move.l	A5,a0
	lea	(32,sp),a3
	move.l	d2,(a3)		;max path length
	move.l	a3,-(sp)
	bsr	_filereq
	movea.l	a3,a1
	movea.l	d2,a0
	adda.l	D0,A1
	move.b	(-1,A1),D2
	moveq	#$2F,D1
	cmp.b	D1,D2
	beq.b	lbC00824C
	moveq	#$3A,D1
	cmp.b	D1,D2
	beq.b	lbC00824C
	subq.l	#1,a0
	cmp.l	a0,D0
	bge.b	lbC00824C
	tst.l	D0
	ble.b	lbC00824C
	addq.l	#1,D0
	move.b	#$20,(A1)+
	clr.b	(a1)
lbC00824C	movea.l	(152+176,SP),A6
	movea.l	($1E,A6),A1
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A6,-(SP)
	move.l	A5,-(SP)
	jsr	(A1)
	lea	($10,SP),SP

lbC008262	tst.l	D7
	beq.b	lbC00827A
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
lbC00827A	move.l	([152+176,SP],14),D0
lbC008282	movem.l	(SP)+,D2/D4/D5/D7/A2/A3/A5/A6
	adda.w	#276,SP
	rts

lbC00828C	suba.w	#$78,SP
	movem.l	D2-D5/D7/A2/A3/A5/A6,-(SP)
	move.l	($AC,SP),D7
	movea.l	($A0,SP),A5
	movea.l	($A8,SP),A2
	moveq	#0,d4
	move.l	A2,D1
	adda.l	d7,a2
	move.b	(A2),D4
	clr.b	(A2)
	lea	($24,SP),A0
	move.l	A0,D2
	movea.l	(4,A5),A6
	moveq	#$5A,D3
	sys	ParsePatternNoCase
	move.l	D0,D3
	move.b	D4,(A2)
	moveq	#$1D,D1
	sys	LockDosList
	movea.l	D0,A2
	bra	\11

\1	move.l	($28,A2),D0
	asl.l	#2,D0
	movea.l	D0,A0
	move.b	(A0),D4
	addq.l	#1,D0
	movea.l	D0,A3
	move.l	D3,D0
	subq.l	#1,D0
	bne.b	\4
	moveq	#$1D,D0
	cmp.b	D0,D4
	bhi	\2
	move.l	D4,D0
\2	movea.l	A3,a1
	lea	($7e,SP),a0
	bsr	lbC00DD34
	moveq	#$1D,D0
	cmp.b	D0,D4
	bhi	\3
	move.l	D4,D0
\3	clr.b	($7E,SP,D0.L)
	lea	($24,SP),A0
	move.l	A0,D1
	lea	($7E,SP),A0
	move.l	A0,D2
	movea.l	(4,A5),A6
	sys	MatchPatternNoCase
	bra.b	\6

\4	move.l	D4,D0
	cmp.l	D7,D0
	blt.b	\5
	move.l	D7,D0
	movea.l	A3,A0
	movea.l	($A8,SP),A1
	movea.l	($1C,A5),A6
	sys	Strnicmp
	tst.l	D0
	bne.b	\5
	moveq	#1,D0
	bra.b	\6

\5	moveq	#0,D0
\6	move.w	D0,D5
	beq.b	\11
	movea.l	($B0,SP),A1
	movea.l	($1E,A1),A0
	move.l	D4,D0
	addq.l	#1,D0
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A1,-(SP)
	move.l	A5,-(SP)
	jsr	(A0)
	lea	(16,SP),SP
	movea.l	D0,A0
	tst.l	D0
	beq.b	\11
	move.l	(4,A2),D0
	beq.b	\7
	subq.l	#1,D0
	beq.b	\8
	subq.l	#2,D0
	beq.b	\8
	subq.l	#1,D0
	beq.b	\8
	bra.b	\9

\7	moveq	#1,d0
	bra.b	\10

\8	moveq	#2,d0
	bra.b	\10

\9	moveq	#3,d0
\10	move.b	d0,(9,A0)
	move.b	#$3A,($12,A0,D4.W)
	move.l	($B0,SP),d0
	movea.l	A5,a1
	bsr	_sort
\11	move.l	A2,D1
	movea.l	(4,A5),A6
	moveq	#$1D,D2
	sys	NextDosEntry
	movea.l	D0,A2
	tst.l	D0
	bne	\1
	moveq	#$1D,D1
	sys	UnLockDosList
	movea.l	($B0,SP),A2
	move.l	(14,A2),d7
	bne.b	\12
	movea.l	($A4,SP),A0
	btst	#4,($17F,A0)
	bne.b	\12
	movea.l	(8,A5),A6
	suba.l	A0,A0
	sys	DisplayBeep
\12	move.l	d7,D0
	movem.l	(SP)+,D2-D5/D7/A2/A3/A5/A6
	adda.w	#$78,SP
	rtd	#20

	ifd	rom
_copyright	movea.l	(_exec,A4),A6
	moveq	#OLTAG_COPYRIGHT1,D0
	bsr	\1
	moveq	#OLTAG_COPYRIGHT2,D0
	sys	TaggedOpenLibrary
	movea.l	D0,A0
	moveq	#-1,d0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	moveq	#OLTAG_COPYRIGHT3,D0
	bsr	\1
	moveq	#OLTAG_COPYRIGHT4,D0
\1	sys	TaggedOpenLibrary
	movea.l	D0,A0
	moveq	#-1,d0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	lea	(_cr,pc),a0
	moveq	#1,d0
	move.l	A3,a1
	move.l	A5,d1
	endc

_doio:	movea.l	($98,A1),A1
	move.w	#CMD_WRITE,(io_Command,A1)
	movem.l	d0/a0,(io_Length,a1)		length/data
	movem.l	d0/d1/A6,-(SP)
	movea.l	(_exec,A4),A6
	sys	DoIO
	movem.l	(sp)+,d0/a0/a6
	clr.l	($40,a0)
	rts

lbC008410	suba.w	#$14,SP
	movem.l	D2/D4-D7/A2/A3/A5,-(SP)
	move.l	a2,d4
	move.l	d0,d5
	movea.l	a1,A3
	movea.l	($38,SP),A5
	move.l	A5,a0
	bsr	lbC0071B4	Duplock() of cd
	move.l	D0,D7
	beq.b	\1
	move.l	D7,a1
	move.l	A5,a0
	bsr	_CurrentDir
\1	move.l	d4,D1
	movea.l	(4,A5),A6
	sys	PathPart
	movea.l	D0,A2
	move.b	(A2),D2
	clr.b	(A2)
	moveq	#-2,d1
	movea.l	d4,a1
	move.l	A5,a0
	bsr	_Lock
	move.b	D2,(A2)
	move.l	D0,D2
	bne	\2
	move.l	D7,d0
	beq	\11
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
	moveq	#0,D0
	bra	\11

\2	move.l	d4,D1
	sys	FilePart
	lea	($24,SP),A2
	movea.l	a2,a0
	move.l	D0,(A0)+
	sub.l	d4,D0
	move.l	d5,D1
	sub.l	D0,D1
	move.l	D1,(A0)+
	moveq	#1,D0
	move.w	D0,(A0)+
	move.w	D0,(A0)+
	move.l	A5,(A0)+
	move.l	($78,A3),D0	window
	beq.b	\3
	movea.l	d0,a0
	moveq	#1,d0
	bsr	lbC00D666	set busy pointer
\3	move.l	($48,SP),-(SP)
	pea	($24,SP)
	move.l	D2,-(SP)
	move.l	A3,-(SP)
	bsr	lbC007360
	movea.l	(A5),A6
;	sys	Forbid
	cmp.l	(A2),d4
	bne	\7
	movea.l	A3,a0
	bsr	lbC0094C2
	tst.l	D0
	beq	\7
	movea.l	d0,A0		get our process
	move.l	(pr_CLI,A0),D0	get CLI
	beq	\7
	asl.l	#2,D0
	movea.l	D0,A0
	move.l	(cli_CommandDir,A0),D4
	clr.l	-(sp)		place for our temp locks list
	movea.l	sp,a0
	move.l	d2,-(sp)
	bsr	\6		add current dir to our list
	movea.l	(sp)+,a1
	move.l	A5,a0
	bsr	_UnLock
	asl.l	#2,D4
	movea.l	D4,A2
	beq	\5
\4	move.l	(4,A2),D2	get lock
	move.l	sp,d6
.next	movea.l	d6,a0
	move.l	(a0),d6
	beq	.add
	movea.l	d6,a0
	move.l	(4,a0),d1
	movea.l	d2,a1
	movea.l	a5,a0
	moveq	#21,d0		SameLock()
	bsr	_l2
;	tst.l	d0
	bne	.next
	bra	.ok	we have examined this dir already, so skip

.add	bsr	\6
	tst.l	d6
	beq	.ok
	move.l	($4c,SP),-(SP)
	pea	($28,SP)
	move.l	D6,-(SP)
	move.l	A3,-(SP)
	bsr	lbC007360
	tst.l	d2
	bne	.ok
	move.l	D6,a1
	move.l	A5,a0
	bsr	_UnLock
.ok	tst.l	d4
	beq	\5
	move.l	(A2),D0
	asl.l	#2,D0
	movea.l	D0,A2
	bne.b	\4
	moveq	#0,d4

\5	move.l	a2,d1
	lea	(C.MSG,PC),A1
	movea.l	a5,a0
	moveq	#22,d0		GetDeviceProc()
	bsr	_l2
	movea.l	D0,a2
	bne	\4
	move.l	(sp)+,d6
.loop	beq	\7
	movea.l	d6,a1
	move.l	(4,a1),d2
	move.l	(a1),d6
	moveq	#8,d0
	sys	FreeMem
	move.l	D2,a1
	move.l	A5,a0
	bsr	_UnLock
	tst.l	d6
	bra	.loop

\6	move.l	d2,a1
	move.l	a0,d2
	movea.l	a5,a0
	bsr	_DupLock
	move.l	D0,D6
	beq.b	.bad		;fatal
	moveq	#8,d0
	moveq	#MEMF_ANY,d1
	sys	AllocMem
	movea.l	d2,a0
	move.l	d0,d2
	beq	.bad
	move.l	d0,(a0)
	movea.l	d0,a0
	clr.l	(a0)+
	move.l	d6,(a0)
.bad	rts

\7	;sys	Permit

; add resident commands to completer

	movea.l	(4,a5),A0		dosbase
	movea.l	(dl_Root,a0),a0		RootNode
	movea.l	(rn_Info,a0),a0		DosInfo
	adda.l	a0,a0
	adda.l	a0,a0
	sys	Forbid
	movea.l	(di_NetHand,a0),a2
	adda.l	a2,a2
	adda.l	a2,a2
.nxt	move.l	(seg_Next,a2),d2
	move.l	(seg_UC,a2),d1
	bpl	.good
	addq.l	#2,d1
	bne	.skip
.good	lea	(seg_Name,a2),a1
	moveq	#0,d6
	move.b	(a1)+,d6
	moveq	#ST_FILE,d0	simulate type
	moveq	#FIBF_DELETE|FIBF_WRITE,d1 simulate protection
	lea	($20,SP),a0
	bsr	lbC007E08
	tst.w	D0
	beq	.skip
	addq.l	#1,d6
	move.l	D6,-(SP)
	pea	(seg_Name+1,a2)
	movea.l	($50,sp),a2
	movea.l	($1E,A2),A0
	move.l	a2,-(SP)
	move.l	A5,-(SP)
	jsr	(A0)
	lea	(16,SP),SP
	tst.l	d0
	beq	.skip
	movea.l	d0,a0
	move.b	#' ',(17,A0,D6.L)
	moveq	#2,d0	
	move.b	d0,(9,A0)
	move.l	a2,d0
	movea.l	A5,a1
	bsr	_sort	sort entry
.skip	lsl.l	#2,d2
	movea.l	d2,a2
	bne	.nxt
.done	sys	Permit

	move.l	($78,A3),D0	window
	beq.b	\8
	movea.l	d0,a0
	moveq	#0,d0		set normal pointer
	bsr	lbC00D666
\8	tst.l	D7
	beq.b	\9
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
\9	movea.l	($48,SP),A2
	tst.l	(14,A2)
	bne.b	\10
	btst	#4,($17F,A3)
	bne.b	\10
	movea.l	(8,A5),A6
	suba.l	A0,A0
	sys	DisplayBeep
\10	move.l	(14,A2),D0
\11	movem.l	(SP)+,D2/D4-D7/A2/A3/A5
	adda.w	#$14,SP
	rts

lbC0085E2	suba.w	#$14,SP
	movem.l	D4-D7/A2/A3/A5/A6,-(SP)
	move.l	($44,SP),D7
	movea.l	($3C,SP),A3
	movea.l	($38,SP),A5
	movea.l	($40,SP),A2
	move.l	A3,a1
	move.l	A5,a0
	bsr	lbC0071B4
	move.l	D0,D6
	beq.b	lbC008612
	move.l	D6,a1
	move.l	A5,a0
	bsr	_CurrentDir
lbC008612	tst.l	D7
	ble.b	lbC008666
	move.l	A2,a1
	adda.l	d7,a2
	move.b	(A2),D4
	clr.b	(A2)
	moveq	#-2,d1
	move.l	A5,a0
	bsr	_Lock
	move.b	D4,(A2)
	suba.l	d7,a2
	move.l	D0,D5
	bne.b	lbC008668
	tst.l	D6
	beq.b	lbC008650
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
lbC008650	btst	#4,($17F,A3)
	bne.b	lbC008662
	movea.l	(8,A5),A6
	suba.l	A0,A0
	sys	DisplayBeep
lbC008662	moveq	#0,D0
	bra.b	lbC0086CC

lbC008666	moveq	#0,D5
lbC008668	lea	($24,SP),A6
	move.l	A2,(A6)+
	clr.l	(A6)+
	clr.w	(A6)+
	moveq	#$40,D0
	lsl.l	#8,D0
	and.l	($160,A3),D0
	move.w	D0,($2E,SP)
	move.l	A5,($30,SP)
	move.l	D6,D0
	tst.l	D5
	beq.b	lbC00868A
	move.l	D5,D0
lbC00868A	movea.l	($48,SP),A2
	move.l	A2,-(SP)
	pea	($24,SP)
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	bsr	lbC007360
	tst.l	D5
	beq.b	lbC0086B0
	move.l	D5,a1
	move.l	A5,a0
	bsr	_UnLock
lbC0086B0	tst.l	D6
	beq.b	lbC0086C8
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_CurrentDir
	move.l	D0,a1
	move.l	A5,a0
	bsr	_UnLock
lbC0086C8	move.l	(14,A2),D0
lbC0086CC	movem.l	(SP)+,D4-D7/A2/A3/A5/A6
	adda.w	#$14,SP
	rts

_newlist	NEWLIST	a0
	rts

lbC00877E	movem.l	D7/A5,-(SP)
	move.l	d1,D7
	movea.l	a0,A5
	move.l	d0,-(sp)
	bsr	_newlist
	move.l	(sp)+,d0
	clr.l	(14,A5)
	clr.l	($12,A5)
	move.b	d0,($1A,A5)
	tst.l	D7
	ble.b	\1
	move.l	D7,D0
	addi.l	#$200,D0
	bsr	lbC0097BA
	move.l	D0,(14,A5)
	bne.b	\2
	tst.l	D7
\1	bne.b	\3
\2	move.l	D7,($16,A5)
	moveq	#1,D0
	bra.b	\4

\3	clr.l	($16,A5)
	moveq	#0,D0
\4	movem.l	(SP)+,D7/A5
	rts

lbC0087D6	move.l	A5,-(SP)
	movea.l	a0,A5
	bsr	_newlist
	lea	(26,a5),a0
	clr.l	-(A0)
	clr.l	-(A0)
	move.l	-(A0),D0
	beq.b	\1
	movea.l	D0,a0
	bsr	lbC009360
	clr.l	(14,A5)
\1	movea.l	(SP)+,A5
	rts

lbC008802	movem.l	D7/A2/A3/A5/A6,-(SP)
	movea.l	A0,a3
	movea.l	a1,A5
	moveq	#0,D7
	bra.b	\2

\1	addq.l	#1,A3
\2	moveq	#$20,D0
	cmp.b	(A3),D0
	beq.b	\1
	movea.l	A3,A2
	bra.b	\7

\3	move.b	(A2),D0
	moveq	#$20,D1
	cmp.b	D1,D0
	beq.b	\4
	moveq	#$3A,D1
	cmp.b	D1,D0
	bne.b	\5
\4	addq.l	#2,D7
	bra.b	\6

\5	addq.l	#1,D7
\6	addq.l	#1,A2
\7	tst.b	(A2)
	bne.b	\3
	move.l	D7,D0
	addq.l	#3,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(_exec,A4),A6
	sys	AllocVec
	move.l	D0,($68,A5)
	beq.b	\15
	movea.l	D0,A2
	moveq	#$7E,D0
	cmp.b	(A3),D0
	bne.b	\8
	clr.w	($6C,A5)
	addq.l	#1,A3
	bra.b	\14

\8	move.w	#1,($6C,A5)
	bra.b	\14

\9	addq.l	#1,A3
\10	move.b	(A3),D0
	moveq	#$20,D1
	cmp.b	D1,D0
	beq.b	\9
	moveq	#$3A,D1
	cmp.b	D1,D0
	beq.b	\9
	bra.b	\12

\11	move.b	(A3)+,(A2)+
\12	move.b	(A3),D0
	moveq	#$20,D1
	cmp.b	D1,D0
	beq.b	\13
	moveq	#$3A,D1
	cmp.b	D1,D0
	beq.b	\13
	tst.b	D0
	bne.b	\11
\13	move.b	#$3A,(A2)
	addq.l	#2,A2
\14	tst.b	(A3)
	bne.b	\10
\15	move.l	($68,A5),D0
	movem.l	(SP)+,D7/A2/A3/A5/A6
	rts

lbC0088A2	movem.l	d2/A2/A3/A5/A6,-(SP)
	movea.l	(A5),A6
	move.l	#$ffff,d2
	movea.l	a0,A5		semaphore
	lea	(KingCONDC1.MSG,pc),A1
	sys	Forbid
	sys	FindSemaphore
	movea.l	D0,A3
	tst.l	D0
	beq	\5
	movea.l	A3,A0
	sys	ObtainSemaphore
	sys	Permit
	addq.w	#1,($66,A3)
	move.l	($154,A5),D0
	move.l	($44,A3),D1
	cmp.l	D0,D1
	beq.b	\1
	cmp.l	d2,D0
	beq.b	\1
	lea	($2E,A3),A2
	movea.l	A2,a0
	bsr	lbC0087D6
	moveq	#1,d0
	move.l	($154,A5),d1
	movea.l	A2,a0
	bsr	lbC00877E
\1	move.l	($158,A5),D0
	move.l	($60,A3),D1
	cmp.l	D0,D1
	beq.b	\2
	cmp.l	d2,D0
	beq.b	\2
	lea	($4A,A3),A2
	movea.l	A2,a0
	bsr	lbC0087D6
	moveq	#2,d0
	move.l	($158,A5),d1
	movea.l	A2,a0
	bsr	lbC00877E
\2	tst.l	($15C,A5)
	beq.b	\4
	move.l	($68,A3),D0
	beq.b	\3
	movea.l	D0,A1
	sys	FreeVec
\3	movea.l	($15C,A5),a0
	movea.l	A3,a1
	bsr	lbC008802
\4	movea.l	A3,A0
	sys	ReleaseSemaphore
	move.l	A3,D0
	bra	\15

\5	sys	Permit
	moveq	#$7C,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	sys	AllocVec
	movea.l	D0,A2
	tst.l	D0
	beq	\14
	movea.l	A2,A0
	sys	InitSemaphore
	lea	($6E,A2),A0
	movea.l	A0,A1
	lea	(KingCONDC1.MSG,pc),A0
	moveq	#13,D0
	sys	CopyMem
	clr.b	(9,A2)
	lea	($6E,A2),A0
	move.l	A0,(10,A2)
	move.w	#1,($66,A2)
	lea	($2E,A2),A0
	move.l	($154,A5),D0
	cmp.l	d2,D0
	bne.b	\6
	moveq	#$40,D1
	lsl.l	#8,D1
	bra.b	\7

\6	move.l	D0,D1
\7	;movea.l	($18,SP),A3
	moveq	#1,d0
	bsr	lbC00877E
	tst.w	D0
	beq.b	\13
	move.l	($158,A5),D0
	cmp.l	d2,D0
	bne.b	\8
	moveq	#$40,D1
	lsl.l	#8,D1
	bra.b	\9

\8	move.l	D0,D1
\9	moveq	#2,d0
	lea	($4A,A2),a0
	bsr	lbC00877E
	tst.w	D0
	beq.b	\12
	lea	(RAMRADVD0SD0.MSG,pc),A0
	move.l	($15C,A5),D0
	beq.b	\10
	movea.l	D0,A0
\10	movea.l	A2,a1
	bsr	lbC008802
	tst.l	D0
	beq.b	\11
	movea.l	A2,A1
	sys	AddSemaphore
	move.l	A2,D0
	bra.b	\15

\11	lea	($4A,A2),a0
	bsr	lbC0087D6
\12	pea	($2E,A2)
	movea.l	sp,A0
	bsr	lbC0087D6
	addq.l	#4,sp
\13	movea.l	A2,A1
	sys	FreeVec
\14	moveq	#0,D0
\15	movem.l	(SP)+,d2/A2/A3/A5/A6
	rts

lbC008A98	movem.l	A3/A5/A6,-(SP)
	movea.l	a0,A5
	movea.l	(_exec,A4),A6
	sys	Forbid
	lea	(KingCONDC1.MSG,pc),A1
	sys	FindSemaphore
	tst.l	D0
	beq.b	lbC008AF4
	movea.l	D0,A3
	movea.l	A3,A1
	sys	RemSemaphore
	movea.l	A3,A0
	sys	ObtainSemaphore
	sys	ReleaseSemaphore
	move.l	($68,A3),D0
	beq.b	lbC008AD4
	movea.l	D0,A1
	sys	FreeVec
lbC008AD4	lea	($4A,A3),a0
	bsr	lbC0087D6
	lea	($2E,A3),A0
	bsr	lbC0087D6
	movea.l	A3,A1
	sys	FreeVec
lbC008AF4	sys	Permit
	movem.l	(SP)+,A3/A5/A6
	rts

lbC008AFE	movem.l	A2/A3/A5/A6,-(SP)
	movea.l	a0,A5
	movea.l	a1,A2
	movea.l	(_exec,A4),A6
	sys	Remove
	movea.l	($1A,A2),A1
	bra.b	\2

\1	movea.l	(A1),A3
	move.b	(9,A1),D0
	extb.l	D0
	sub.l	D0,($12,A5)
	move.b	(9,A1),D0
	extb.l	D0
	movea.l	(14,A5),a0
	IFD	rom
	sys	FreePooled
	ELSE
	bsr	lbC009368
	ENDC
	movea.l	A3,A1
\2	move.l	A1,D0
	bne.b	\1
	lea	($1E,A2),a0
	bsr	_strlen
	moveq	#$1F,D1
	add.l	D1,D0
	sub.l	D0,($12,A5)
	movea.l	A2,a1
	movea.l	(14,A5),a0
	IFD	rom
	sys	FreePooled
	ELSE
	bsr	lbC009368
	ENDC
	movem.l	(SP)+,A2/A3/A5/A6
	rts

lbC008B6C	movem.l	A2/A3,-(SP)
	movea.l	a0,A2
	movea.l	(a2),a1
	bra.b	\2

\1	;move.l	A1,-(SP)
	move.l	A2,a0
	bsr	lbC008AFE
	movea.l	a3,a1
\2	move.l	(A1),A3
	move.l	A3,D0
	bne.b	\1
	movea.l	A2,a0
	bsr	_newlist
	clr.l	($12,A2)
	movem.l	(SP)+,A2/A3
	rts

lbC008BA2	movem.l	A3/A5/A6,-(SP)
	movea.l	a0,A5
	movea.l	(_exec,A4),A6
	lea	(KingCONDC1.MSG,pc),A1
	sys	Forbid
	sys	FindSemaphore
	tst.l	D0
	beq.b	lbC008BE8
	movea.l	D0,A3
	movea.l	A3,A0
	sys	ObtainSemaphore
	sys	Permit
	lea	($4A,A3),a0
	bsr.b	lbC008B6C
	lea	($2E,A3),A0
	bsr.b	lbC008B6C
	movea.l	A3,A0
	sys	ReleaseSemaphore
	bra.b	lbC008BEC

lbC008BE8	sys	Permit
lbC008BEC	movem.l	(SP)+,A3/A5/A6
	rts

lbC008BF2	suba.w	#12,SP
	movem.l	D2/D7/A2/A3/A5/A6,-(SP)
	movea.l	($28,SP),A5
	movea.l	($30,SP),A3
	movea.l	($2C,SP),A2
	moveq	#0,D0
	move.b	(A3),D0
;	tst.l	D0
	beq.b	\1
	subq.l	#1,D0
	beq	\8
	bra	\12

\1	suba.l	A1,A1
	movea.l	(A5),A6
	sys	FindTask
	movea.l	D0,A1
	move.l	(pr_WindowPtr,A1),($1c,sp)
	moveq	#-1,D1
	move.l	D1,(pr_WindowPtr,A1)
	move.l	D0,($20,SP)
	movea.l	(4,A5),A6
	moveq	#2,D1
	moveq	#0,D2
	sys	AllocDosObject
	move.l	D0,($18,SP)
	beq.b	\7
	movea.l	(A2),A2
	bra.b	\6

\2	lea	($1E,A2),A0
	move.l	A0,D1
	movea.l	(4,A5),A6
	moveq	#-2,D2
	sys	Lock
	move.l	D0,D7
	beq.b	\4
	move.l	D7,D1
	move.l	($18,SP),D2
	sys	Examine
	tst.l	D0
	beq.b	\3
	movea.l	D2,A0
	adda.w	#$84,A0
	lea	(14,A2),A1
	move.l	A0,D1
	move.l	A1,D2
	sys	CompareDates
	tst.l	D0
	bpl.b	\3
	move.l	A2,a1
	move.l	($2c,SP),a0
	bsr	lbC008AFE
\3	move.l	D7,D1
	sys	UnLock
	bra.b	\5

\4	move.l	A2,a1
	move.l	($2c,SP),a0
	bsr	lbC008AFE
\5	movea.l	A3,A2
\6	movea.l	(A2),A3
	move.l	A3,D0
	bne.b	\2
	move.l	($18,SP),D2
	movea.l	(4,A5),A6
	moveq	#2,D1
	sys	FreeDosObject
\7	movea.l	($20,SP),A0
	move.l	($1C,SP),(pr_WindowPtr,A0)
	movea.l	($30,SP),A0
	move.b	#1,(A0)
	bra.b	\12

\8	moveq	#1,D0
	cmp.b	($1A,A2),D0
	bne.b	\9
	movea.l	(8,A2),A0
	bra.b	\10

\9	movea.l	(A2),A0
\10	movea.l	(A2),A1
	tst.l	(A1)
	beq.b	\11
	move.l	A0,a1
	move.l	A2,a0
	bsr	lbC008AFE
	bra.b	\12

\11	move.b	#2,(A3)
\12	movem.l	(SP)+,D2/D7/A2/A3/A5/A6
	adda.w	#12,SP
	rtd	#12

lbC008E5A	suba.w	#$14,SP
	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	move.l	($48,SP),D7
	movea.l	($40,SP),A5
	movea.l	($4C,SP),A2
	suba.l	A3,A3
	clr.b	($33,SP)
	bra	\5

\1	movea.l	($50,SP),a0
	move.l	($44,SP),a1
	movem.l	D7/A2/A3/A5/A6,-(SP)
	movea.l	(A5),A6
	movea.l	a0,A3
	movea.l	a1,A5
	bsr	_strlen
	move.l	D0,D7
	moveq	#$1F,D1
	add.l	D1,D7
	move.l	($12,A5),D0
	add.l	D7,D0
	cmp.l	($16,A5),D0
	blt.b	\2
	moveq	#0,D0
	bra.b	\4

\2	move.l	D7,d0
	movea.l	(14,A5),a0
	IFD	rom
	sys	AllocPooled
	ELSE
	bsr	lbC009364
	ENDC
	tst.l	D0
	beq.b	\3
	movea.l	a2,A0
	movea.l	D0,A2
	adda.w	#$84,A0
	lea	(14,A2),A1
	move.l	(A0)+,(A1)+
	move.l	(A0)+,(A1)+
	move.l	(A0)+,(A1)+
	clr.l	(A1)+
	move.l	D7,D0
	moveq	#$1E,D1
	sub.l	D1,D0
	movea.l	A3,A0
	sys	CopyMem
	lea	($1E,A2),A0
	move.l	A0,(10,A2)
	add.l	D7,($12,A5)
\3	move.l	A2,D0
\4	movem.l	(SP)+,D7/A2/A3/A5/A6
	movea.l	D0,A3
	tst.l	D0
	bne.b	\6
	pea	($33,SP)
	move.l	($48,SP),-(SP)
	move.l	A5,-(SP)
	bsr	lbC008BF2
\5	move.l	A3,D0
	bne.b	\6
	moveq	#2,D0
	cmp.b	($33,SP),D0
	bne	\1
\6	move.l	A3,($38,SP)
	move.l	A3,D0
	beq	\37
	move.l	($1A,A3),($34,SP)
	movea.l	(4,A5),A6
	moveq	#1,D1
	moveq	#0,D2
	sys	AllocDosObject
	move.l	D0,($28,SP)
	beq	\35
	movea.l	D0,A0
	clr.l	(12,A0)
	clr.l	(4,A0)
	moveq	#$40,D0
	lsl.l	#5,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(A5),A6
	sys	AllocVec
	move.l	D0,($2C,SP)
	beq	\34
\7	movea.l	($2C,SP),A2
	move.l	D5,-(SP)
	move.l	D7,D1
	move.l	A2,D2
	moveq	#$40,D3
	lsl.l	#5,D3
	moveq	#4,D4
	move.l	($2C,SP),D5
	movea.l	(4,A5),A6
	sys	ExAll
	move.l	(SP)+,D5
	move.l	D0,D6
	bne.b	\8
	sys	IoErr
	moveq	#$74,D1
	add.l	D1,D1
	cmp.l	D1,D0
	bne	\33
\8	movea.l	($28,SP),A0
	tst.l	(A0)
	bls	\32
\9	move.l	A3,D0
	beq	\28
	moveq	#1,D0
	movea.l	($44,SP),A0
	cmp.b	($1A,A0),D0
	beq.b	\10
	move.l	(8,A2),D0
	bmi.b	\10
	moveq	#$42,D0
	and.l	($10,A2),D0
	subq.l	#2,D0
	beq	\28
\10	suba.l	A3,A3
	bra	\23

\11	movea.l	A5,A0
	movea.l	($44,SP),A1
\12	subq.l	#4,SP
	movem.l	D6/D7/A2/A3/A5/A6,-(SP)
	movea.l	a2,A3
	movea.l	A1,A5
	move.l	A0,($18,SP)
	moveq	#0,D7
	moveq	#0,D6
	move.l	(4,A3),D0
	btst	d6,D0
	bne.b	\13
	movea.l	D0,a0
	bsr	_strlen
	addq.l	#4,D0
	andi.w	#$FFFC,D0
	move.l	D0,D6
	moveq	#10,D1
	add.l	D1,D6
	bra.b	\14

\13	movea.l	D0,a0
	bsr	_strlen
	move.l	D0,D7
	moveq	#11,D1
	add.l	D1,D7
\14	cmp.l	D6,D7
	ble.b	\15
	move.l	D7,D0
	bra.b	\16

\15	move.l	D6,D0
\16	move.l	($12,A5),D1
	add.l	D0,D1
	cmp.l	($16,A5),D1
	blt.b	\17
	moveq	#0,D0
	bra.b	\22

\17	cmp.l	D6,D7
	ble.b	\18
	move.l	D7,D0
	bra.b	\19

\18	move.l	D6,D0
\19	movea.l	(14,A5),a0
	movea.l	(_exec,A4),A6
	IFD	rom
	sys	AllocPooled
	ELSE
	bsr	lbC009364
	ENDC
	movea.l	D0,A2
	tst.l	D0
	beq.b	\21
	movea.l	A2,A1
	clr.l	(A1)+
	move.l	($10,A3),(A1)+
	move.l	(8,A3),D0
	move.b	D0,(A1)+
	movea.l	(4,A3),A0
	tst.l	D6
	beq.b	\20
	move.l	D6,D0
	move.b	D0,(A1)+
	moveq	#10,D1
	sub.l	D1,D0
	bsr	lbC00935C
	add.l	D6,($12,A5)
	bra.b	\21

\20	move.l	D7,D0
	move.b	D0,(A1)+
	moveq	#10,D1
	sub.l	D1,D0
;	movea.l	(_exec,A4),A6
	jsr	(-$270,A6)
	add.l	D7,($12,A5)
\21	move.l	A2,D0
\22	movem.l	(SP)+,D6/D7/A2/A3/A5/A6
	addq.l	#4,SP

	movea.l	D0,A3
	tst.l	D0
	bne.b	\24
	pea	($33,SP)
	move.l	($48,SP),-(SP)
	move.l	A5,-(SP)
	bsr	lbC008BF2
\23	moveq	#2,D0
	cmp.b	($33,SP),D0
	bne	\11
	bra	\27

\24	tst.l	($34,SP)
	beq.b	\25
	movea.l	($34,SP),A0
	move.l	A3,(A0)
	bra.b	\26

\25	movea.l	($38,SP),A0
	move.l	A3,($1A,A0)
\26	move.l	A3,($34,SP)
	bra.b	\28

\27	movea.l	($44,SP),A3
	movea.l	A3,A0
	movea.l	($38,SP),A1
	movea.l	(a5),A6
	jsr	(-$F0,A6)
	move.l	($38,SP),a1
	move.l	A3,a0
	bsr	lbC008AFE
	clr.l	($38,SP)
\28	move.l	(8,A2),D0
	move.l	($10,A2),D1
	movea.l	($54,SP),A0
	movea.l	(4,A2),A1
	bsr	lbC007E08
	tst.w	D0
	beq.b	\31
	movea.l	(4,A2),a0
	bsr	_strlen
	move.l	D0,D5
	addq.l	#1,D5
	movea.l	($58,SP),A1
	movea.l	($1E,A1),A0
	move.l	D5,-(SP)
	move.l	(4,A2),-(SP)
	move.l	A1,-(SP)
	move.l	A5,-(SP)
	jsr	(A0)
	lea	(16,SP),SP
	movea.l	D0,A0
	tst.l	D0
	beq.b	\31
	move.l	(8,A2),D0
	bmi.b	\29
	moveq	#$2F,D1
	bra.b	\30

\29	moveq	#$20,D1
\30	move.b	D1,($11,A0,D5.L)
	move.l	(8,A2),D0
	smi	D0
	moveq	#1,D1
	sub.b	D0,D1
	move.b	D1,(9,A0)
	move.l	($58,SP),d0
	movea.l	A5,a1
	bsr	_sort
\31	movea.l	(A2),A2
	movea.l	($38,SP),A3
	move.l	A2,D0
	bne	\9
\32	tst.l	D6
	bne	\7
\33	movea.l	($2C,SP),A1
	movea.l	(A5),A6
	sys	FreeVec
\34	move.l	($28,SP),D2
	movea.l	(4,A5),A6
	moveq	#1,D1
	sys	FreeDosObject
\35	move.l	A3,D0
	beq.b	\36
	movea.l	($44,SP),A0
	movea.l	A3,A1
	movea.l	(a5),A6
	sys	AddHead
\36	moveq	#1,D0
\37	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	adda.w	#$14,SP
	rts

lbC00908A	movem.l	D7/A2/A3/A5/A6,-(SP)
	movea.l	($28,SP),A5
	movea.l	([$24,SP],$1A),A2
	bra.b	lbC00910C

lbC00909C	lea	(10,A2),A3
	move.b	(8,A2),D0
	move.l	(4,A2),D1
	movea.l	($1C,SP),A0
	movea.l	A3,A1
	bsr	lbC007E08
	tst.w	D0
	beq.b	lbC00910A
	movea.l	A3,a0
	bsr	_strlen
	move.l	D0,D7
	addq.l	#1,D7
	movea.l	($1E,A5),A0
	move.l	D7,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	move.l	($24,SP),-(SP)
	jsr	(A0)
	lea	($10,SP),SP
	movea.l	D0,A3
	tst.l	D0
	beq.b	lbC00910A
	move.b	(8,A2),D0
	bmi.b	lbC0090E6
	moveq	#$2F,D1
	bra.b	lbC0090E8

lbC0090E6	moveq	#$20,D1
lbC0090E8	move.b	D1,($11,A3,D7.L)
	move.b	(8,A2),D0
	smi	D0
	moveq	#1,D1
	sub.b	D0,D1
	move.b	D1,(9,A3)
	movea.l	A3,a0
	move.l	A5,d0
	movea.l	($18,SP),a1
	bsr	_sort
lbC00910A	movea.l	(A2),A2
lbC00910C	move.l	A2,D0
	bne.b	lbC00909C
	movea.l	($24,SP),A2
	movea.l	A2,A1
	movea.l	(_exec,A4),A6
	jsr	(-$FC,A6)
	movea.l	($20,SP),A0
	movea.l	A2,A1
	jsr	(-$F0,A6)
	movem.l	(SP)+,D7/A2/A3/A5/A6
	rts

lbC0091DC	suba.w	#$58+176,SP
	movem.l	D2/D3/D6/D7/A2/A3/A5/A6,-(SP)
	move.l	($84+176,SP),D7
	moveq	#1,D6
	movea.l	($7C+176,SP),A5
	movea.l	(4,A5),A6
	moveq	#DOS_FIB,D1
	moveq	#0,D2
	sys	AllocDosObject
	movea.l	D0,A3
	move.l	D0,($20,SP)
	bne.b	\1
	moveq	#0,D6
	bra	\17

\1	move.l	D7,D1
	move.l	A3,D2
	sys	Examine
	tst.l	D0
	beq	\17
	move.l	(4,A3),D0
	bmi	\17
	move.l	D7,D1
	lea	($27,SP),A0
	move.l	A0,D2
	moveq	#64,d3
	lsl.l	#2,d3
	sys	NameFromLock
	tst.l	D0
	beq	\17
	movea.l	(A5),A6
	lea	(KingCONDC1.MSG,pc),A1
	sys	Forbid
	sys	FindSemaphore
	movea.l	D0,A2
	tst.l	D0
	bne.b	\2
	movea.l	($80+176,SP),a0
	;move.l	A5,-(SP)
	bsr	lbC0088A2
	movea.l	D0,A2
	tst.l	D0
	bne.b	\2
	sys	Permit
	moveq	#0,D6
	bra	\17

\2	movea.l	A2,A0
	sys	ObtainSemaphore
	sys	Permit
\3	move.l	($68,a2),d0
	beq	\9
	movem.l	d2/D6-D7/A3/a5/A6,-(SP)
	movea.l	d0,a3
	suba.l	A1,A1
	movea.l	(A5),A6
	sys	FindTask
	movea.l	(4,A5),A6
	movea.l	D0,A5
	lea	($b8,a5),a5
	move.l	(A5),d6
	moveq	#-1,D1
	move.l	D1,(A5)
	move.l	d7,D0
	asl.l	#2,D0
	movea.l	D0,A1
	move.l	(fl_Volume,A1),D7
	bra.b	\7

\4	move.l	A3,D1
	moveq	#-2,D2
	sys	Lock
	move.l	D0,D2
	beq.b	\6
	asl.l	#2,D0
	movea.l	D0,A0
	cmp.l	(fl_Volume,A0),D7
	bne.b	\5
	move.l	D2,D1
	sys	UnLock
	move.w	($6C,a2),D0
	bra.b	\8

\5	move.l	D2,D1
	sys	UnLock
\6	tst.b	(A3)+
	bne.b	\6
\7	tst.b	(A3)
	bne.b	\4
	move.w	($6C,a2),D0
	seq	d0
\8	move.l	d6,(A5)
	movem.l	(SP)+,d2/D6-D7/A3/A5/A6
	tst.w	D0
	bne.b	\10
\9	moveq	#0,D6
	bra	\16

\10	moveq	#1,D0
	movea.l	($88+176,SP),A0
	cmp.w	(12,A0),D0
	bne.b	\11
	lea	($4A,A2),A3
	bra.b	\12

\11	lea	($2E,A2),A3
\12	tst.l	(14,A3)
	bne.b	\13
	moveq	#0,D6
	bra	\16

\13	movea.l	A3,A0
	lea	($27,SP),A1
	sys	FindName
	movea.l	D0,A5
	tst.l	D0
	beq.b	\15
	movea.l	($20,SP),A0
	lea	($84,A0),A1
	lea	(14,A5),A0
	move.l	A1,D1
	move.l	A0,D2
	movea.l	([$7C+176,SP],4),A6
	sys	CompareDates
	tst.l	D0
	bmi.b	\14
	move.l	($8C+176,SP),-(SP)
	move.l	A5,-(SP)
	move.l	A3,-(SP)
	move.l	($94+176,SP),-(SP)
	move.l	($8C+176,SP),-(SP)
	bsr	lbC00908A
	lea	($14,SP),SP
	bra.b	\16

\14	move.l	A5,D0
	beq.b	\15
	move.l	A5,a1
	move.l	A3,a0
	bsr	lbC008AFE
\15	move.l	($8C+176,SP),-(SP)
	move.l	($8C+176,SP),-(SP)
	pea	($2F,SP)
	move.l	($2C,SP),-(SP)
	move.l	D7,-(SP)
	move.l	A3,-(SP)
	move.l	($94+176,SP),-(SP)
	bsr	lbC008E5A
	lea	($1C,SP),SP
	move.w	D0,D6
\16	movea.l	A2,A0
	movea.l	(_exec,A4),A6
	sys	ReleaseSemaphore
\17	move.l	($20,SP),d2
	beq.b	\18
	movea.l	([$7C+176,SP],4),A6
	moveq	#2,D1
	sys	FreeDosObject
\18	move.w	D6,D0
	movem.l	(SP)+,D2/D3/D6/D7/A2/A3/A5/A6
	adda.w	#$58+176,SP
	rts

lbC00935C	;movem.l	A0/A1,-(SP)
	lsr.l	#2,D0
	beq	\1
	subq.l	#1,D0
\2	move.l	(A0)+,(A1)+
	dbra	D0,\2
\1	;movem.l	(SP)+,A0/A1
	rts

lbC009360	move.l	A6,-(SP)
	movea.l	(_exec,A4),A6
	IFD	rom
	sys	DeletePool
	ELSE
	bsr	lbC00E2EC
	ENDC
	movea.l	(SP)+,A6
	rts

lbC00936C	moveq	#0,d0
\1	move.b	(A0)+,D0
	move.b	(_chart,pc,d0.w),d0
	beq.b	\1
	subq.b	#8,d0
	bne	\1
	subq.l	#1,a0
	move.l	A0,D0
	rts

lbC009434	moveq	#$20,D1
	cmp.b	D1,D0
	bcc.b	\1
	moveq	#10,D1
	cmp.b	D1,D0
	beq.b	\1
	move.b	d0,d1
	moveq	#$40,D0
	add.l	D1,D0
	bra.b	\2

\1	moveq	#0,D0
\2	rts

_ChkTask:	tst.l	d0	preserve a0!!!
	beq	\6
	move.l	A6,-(SP)
	movea.l	(_exec,A4),A6
	sys	Forbid
	movea.l	(TaskReady,A6),a1
	bra.b	\2

\1	cmpa.l	d0,a1
	beq	\5
	movea.l	(a1),a1
\2	tst.l	(a1)
	bne	\1
	movea.l	(TaskWait,A6),A1
	bra	\3

\4	cmpa.l	d0,a1
	beq	\5
	movea.l	(a1),a1
\3	tst.l	(a1)
	bne	\4
	moveq	#0,d0
\5	sys	Permit
	movea.l	(SP)+,A6
\6	rts

lbC0094C2	move.l	A3,-(SP)
	movea.l	a0,A3
	move.l	($EC,A3),d0
	bsr.b	_ChkTask
	tst.l	D0
	bne	\2
	move.l	($E8,A3),d0
	bsr	_ChkTask
	move.l	D0,($EC,A3)
\2	movea.l	(SP)+,A3
	rts

lbC0094F8	movem.l	A5/A6,-(SP)
	movea.l	A0,A5
	movea.l	(_exec,A4),A6
	sys	RemHead
_1.2	movea.l	D0,A1
	move.l	(14,A1),D0
	addq.l	#3,D0
	andi.w	#$FFFC,D0
	moveq	#$13,D1
	add.l	D1,D0
	subq.l	#1,(14,A5)
	sub.l	D0,($12,A5)
	sys	FreeMem
	movem.l	(SP)+,A5/A6
	rts

lbC009530	movem.l	A5/A6,-(SP)
	movea.l	(20-4,SP),A5
	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	RemTail
	bra	_1.2

lbC009568	movem.l	A5/A6,-(SP)
	movea.l	(20-4,SP),A5
	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	RemHead
_2.2	movea.l	D0,A1
	move.l	(14,a1),D0
	addq.l	#3,D0
	andi.w	#$FFFC,D0
	moveq	#$13,D1
	add.l	D1,D0
	subq.l	#1,(14,A5)
	sub.l	D0,($12,A5)
	movea.l	($1A,A5),a0
	IFD	rom
	sys	FreePooled
	ELSE
	bsr	lbC009368
	ENDC
	movem.l	(SP)+,A5/A6
	rts

lbC0095A8	movem.l	A5/A6,-(SP)
	movea.l	(20-4,SP),A5
	movea.l	A5,A0
	movea.l	(_exec,A4),A6
	sys	RemTail
	bra	_2.2

lbC0095E4	bsr	lbC0094F8
lbC0095E8	movea.l	(8,SP),A0
	tst.l	(14,A0)
	bne.b	lbC0095E4
	rts

lbC00960C	movea.l	(8,SP),A0
	clr.l	(14,A0)
	clr.l	($12,A0)
	bsr	_newlist
	rts

; add filename to completer list

lbC009622	movem.l	D6/D7/A2/A3/A5/A6,-(SP)
	move.l	($28,SP),D0	length of the string
	beq	\8
	move.l	d0,d7
	movea.l	($20,SP),A5	list
	movea.l	($1C,SP),A3	lib bases
	move.l	D7,D6
	addq.l	#3,D6
	andi.w	#$FFFC,D6
	move.l	($16,A5),D0
	ble.b	\4
	bra.b	\3

\2	move.l	A5,a0
	bsr	lbC0094F8
	move.l	($16,A5),D0
\3	sub.l	D6,D0
	moveq	#$13,D1
	sub.l	D1,D0
	move.l	($12,A5),D1
	cmp.l	D0,D1
	bhi.b	\2
\4	move.l	D6,D0
	moveq	#$13,D1
	add.l	D1,D0
	moveq	#1,D1
	swap	D1
	movea.l	(_exec,A4),A6
	sys	AllocMem
	movea.l	D0,A2
	tst.l	D0
	beq.b	\7
	movea.l	A5,A0
	movea.l	A2,A1
	sys	AddTail
	move.l	($24,SP),D0
	btst	#0,D0
	lea	($12,A2),A3
	bne.b	\5
	movea.l	D0,A0
	move.l	D6,D0
	movea.l	A3,A1
	bsr	lbC00935C
	bra.b	\6

\5	movea.l	D0,A0
	move.l	D7,D0
	movea.l	A3,A1
	sys	CopyMem
\6	clr.b	($12,A2,D7.L)
	move.l	D7,(14,A2)
	lea	($12,A2),A0
	move.l	A0,(10,A2)
	lea	(14,A5),A0
	addq.l	#1,(A0)+
	move.l	(A0),D0
	add.l	D6,D0
	moveq	#$13,D1
	add.l	D1,D0
	move.l	D0,(A0)+
\7	move.l	A2,D0
\8	movem.l	(SP)+,D6/D7/A2/A3/A5/A6
	rts

lbC0096D2	movem.l	D5-D7/A2/A3/A5/A6,-(SP)
	move.l	($2C,SP),D7
	movea.l	($24,SP),A5
	movea.l	($20,SP),A3
	bne.b	lbC0096EA
	moveq	#0,D0
	bra	lbC009794

lbC0096EA	move.l	D7,D6
	addq.l	#3,D6
	andi.w	#$FFFC,D6
	move.l	($16,A5),D0
	ble.b	lbC009716
	bra.b	lbC009704

lbC0096FA	move.l	A5,-(SP)
	move.l	A3,-(SP)
	bsr	lbC009568
	addq.l	#8,SP
lbC009704	move.l	($16,A5),D0
	sub.l	D6,D0
	moveq	#$13,D1
	sub.l	D1,D0
	move.l	($12,A5),D1
	cmp.l	D0,D1
	bhi.b	lbC0096FA
lbC009716	move.l	D6,D5
	moveq	#$13,D0
	add.l	D0,D5
lbC00971C	move.l	D5,d0
	movea.l	($1A,A5),a0
	movea.l	(_exec,A4),A6
	IFD	rom
	sys	AllocPooled
	ELSE
	bsr	lbC009364
	ENDC
	movea.l	D0,A2
	tst.l	D0
	beq.b	lbC009780
	movea.l	A5,A0
	movea.l	A2,A1
	jsr	(-$F6,A6)
	move.l	($28,SP),D0
	btst	#0,D0
	lea	($12,A2),A3
	bne.b	lbC009754
	movea.l	D0,A0
	move.l	D6,D0
	movea.l	A3,A1
	bsr	lbC00935C
	bra.b	lbC00975E

lbC009754	movea.l	D0,A0
	move.l	D7,D0
	movea.l	A3,A1
	jsr	(-$270,A6)
lbC00975E	clr.b	($12,A2,D7.L)
	move.l	D7,(14,A2)
	lea	($12,A2),A0
	move.l	A0,(10,A2)
	lea	(14,A5),A6
	addq.l	#1,(A6)+
	move.l	(A6),D0
	add.l	D6,D0
	moveq	#$13,D1
	add.l	D1,D0
	move.l	D0,(A6)+
	bra.b	lbC009792

lbC009780	move.l	(14,A5),D0
	ble.b	lbC00971C
	move.l	A5,-(SP)
	move.l	A3,-(SP)
	bsr	lbC009568
	addq.l	#8,SP
	bra.b	lbC00971C

lbC009792	move.l	A2,D0
lbC009794	movem.l	(SP)+,D5-D7/A2/A3/A5/A6
	rts

lbC00979A	move.l	A5,-(SP)
	movea.l	(12,SP),A5
	move.l	A5,-(SP)
	move.l	(12,SP),-(SP)
	bsr	lbC00960C
	addq.l	#8,SP
	movea.l	($1A,A5),A0
	movea.l	(SP)+,A5
	bra	lbC009360

lbC0097BA	movem.l	d2/A5/A6,-(SP)
	movea.l	(_exec,A4),A6
	move.l	d0,D2
	sys	Forbid
	move.l	#MEMF_LARGEST|MEMF_PUBLIC,D1
	sys	AvailMem
	move.l	D2,D1
	addi.l	#$400,D1
	suba.l	A5,A5
	cmp.l	D1,D0
	bls.b	lbC00981A
	move.l	D2,d1
	moveq	#MEMF_PUBLIC,d0
	IFD	rom
	sys	CreatePool
	ELSE
	bsr	lbC00E2A4
	ENDC
	tst.l	D0
	beq.b	lbC00981A
	movea.l	D0,A5
	moveq	#1,d0
	movea.l	A5,a0
	IFD	rom
	sys	AllocPooled
	ELSE
	bsr	lbC009364
	ENDC
	tst.l	D0
	beq.b	lbC00981A
	movea.l	D0,a1
	moveq	#1,d0
	movea.l	A5,a0
	IFD	rom
	sys	FreePooled
	ELSE
	bsr	lbC009368
	ENDC
lbC00981A	sys	Permit
	move.l	A5,D0
	movem.l	(SP)+,d2/A5/A6
	rts

lbC009826	movem.l	D7/A5,-(SP)
	movea.l	a0,A5
	move.l	a1,D7
;	movea.l	A5,a0
	bsr	_newlist
	clr.l	(18,A5)
	clr.l	(14,A5)
	tst.b	d0
	beq.b	\1
	lea	(30,A5),A1		buffer dynamic
	lea	(lbC009622,PC),A0
	move.l	A0,(A1)+		30
	lea	(lbC0095E8,PC),A0
	move.l	A0,(A1)+		34
	lea	(lbC009530,PC),A0
	move.l	A0,(A1)+		38
	lea	(lbC0094F8,PC),A0
	move.l	A0,(A1)+		42
	lea	(lbC0095E8,PC),A0
	bra.b	\2

\1	move.l	D7,D0			buffer static
	addi.l	#$400,D0
	bsr	lbC0097BA
	move.l	D0,(26,A5)
	beq.b	\3
	lea	(30,A5),A1
	lea	(lbC0096D2,PC),A0
	move.l	A0,(A1)+		30
	lea	(lbC00960C,PC),A0
	move.l	A0,(A1)+		34
	lea	(lbC0095A8,PC),A0
	move.l	A0,(A1)+		38
	lea	(lbC009568,PC),A0
	move.l	A0,(A1)+		42
	lea	(lbC00979A,PC),A0
\2	move.l	A0,(A1)+		46
	move.l	D7,(22,A5)
	moveq	#1,D0
\3	movem.l	(SP)+,D7/A5
	rts

lbC0098BA	movem.l	D6/D7/A2/A3/A5/A6,-(SP)
	movea.l	(_exec,A4),A6
	movea.l	a1,A5
	movea.l	($10E,A5),A1
	movea.l	A1,A2
	move.l	d0,D7
	adda.l	D7,A2
	movea.l	a0,A3
	lea	($15FC,A5),A0
	cmpa.l	A0,A2
	bhi.b	\1
	movea.l	A3,A0
	move.l	D7,D0
	jsr	(-$270,A6)
	add.l	D7,($10E,A5)
	bra.b	\\2

\1	lea	($15FC,A5),A0
	move.l	A0,D6
	sub.l	($10E,A5),D6
	movea.l	A3,A0
	move.l	D6,D0
	jsr	(-$270,A6)
	move.l	D7,D0
	sub.l	D6,D0
	movea.l	A3,A0
	adda.l	D6,A0
	lea	($DFC,A5),A1
	jsr	(-$270,A6)
	movea.l	A5,A0
	adda.l	D7,A0
	suba.l	D6,A0
	lea	($DFC,A0),A1
	move.l	A1,($10E,A5)
\\2	movem.l	(SP)+,D6/D7/A2/A3/A5/A6
	rts

lbC009AA0	moveq	#'T',d0	SD
	bra	_lab

lbC009AA2	moveq	#'S',d0	SU
	bra	_lab

lbC0099D6	moveq	#'C',d0	CUF
	bra	_lab

lbC009998	moveq	#'D',d0	CUB
_lab	movem.l	A5/d2,-(SP)
	move.l	d0,d2
	movea.l	a0,A5
	moveq	#1,D0
	move.b	#$9B,(A5)+
	cmp.l	D0,D1
	ble.b	\1
	move.l	D1,-(SP)
	lea	(d.MSG,pc),a0
	movea.l	A5,a1
	bsr	_RawDoFmt
	adda.l	d0,a5
	addq.l	#1,D0
\1	move.b	d2,(A5)
	addq.l	#1,D0
	movem.l	(SP)+,A5/d2
	rts

lbC009A14	move.b	#$9B,(A0)+
	moveq	#3,D0		cursor ON
	tst.b	d1
	bne.b	\2

\1	moveq	#4,D0		cursor OFF
	move.b	#'0',(A0)+
\2	move.b	#$20,(A0)+
	move.b	#'p',(A0)
	rts

lbC009A4A	move.b	#$9B,(A0)+
	move.b	#'J',(A0)	ED (erase in display)
	moveq	#2,D0
	rts

lbC009A5C	moveq	#-$65,d0
	move.b	d0,(a0)+
	move.b	#'7',(a0)+
	move.b	#'m',(a0)+
	move.b	d1,(a0)+
	move.b	d0,(a0)+
	move.b	#'2',(a0)+
	move.b	#'7',(a0)+
	move.b	#'m',(a0)+
	moveq	#8,D0
	rts

lbC009A84	move.b	#$9B,(A0)+
	move.b	#'P',(A0)	DCH (delete character)
	moveq	#2,D0
	rts


lbC009B1E	move.l	A5,-(SP)
	movea.l	a0,A5
	move.b	#$9B,(A5)+
	move.l	d1,-(SP)
	lea	(d.MSG,pc),a0
	movea.l	A5,a1
	bsr	_RawDoFmt
	adda.l	d0,a5
	move.b	#';',(A5)+
	move.b	#'1',(A5)+
	move.b	#'H',(A5)
	addq.l	#4,D0
	movea.l	(SP)+,A5
	rts

lbC009B54	move.l	A5,-(SP)
	movea.l	a0,A5
	move.b	#$9B,(A5)+
	move.l	d1,-(SP)
	lea	(d.MSG,pc),a0
	movea.l	A5,a1
	bsr	_RawDoFmt
	adda.l	d0,a5
	move.b	#'A',(A5)	CUU (cursor up)
	addq.l	#2,D0
	movea.l	(SP)+,A5
	rts

lbC009B7E	move.b	#$9B,(A0)+
	move.b	#'>',(A0)+
	move.b	#'1',(A0)+
	moveq	#'h',D0		enable scroll
	tst.b	d1
	bne.b	\1
	moveq	#'l',D0		disable scroll
\1	move.b	D0,(A0)+
	moveq	#4,D0
	rts

*** a0 MUST be saved!
lbC009BA2	movem.l	D2-d4,-(SP)
	moveq	#';',d2
	moveq	#'1',D3
	moveq	#'2',d4
	movea.l	a0,A1
	move.b	#$9B,(A1)+
	btst	#13,D0
	bne.b	\1
	move.b	D3,(A1)+
	move.b	#'0',(A1)+	IECLASS_MENULIST
	move.b	d2,(a1)+	;
\1	btst	#10,D0
	bne.b	\2
	move.b	D3,(A1)+
	move.b	D4,(A1)+	IECLASS_SIZEWINDOW
	move.b	d2,(a1)+	;
\2	btst	#1,D0
	beq.b	\3
	move.b	D3,(A1)+
	move.b	D3,(A1)+	IECLASS_CLOSEWINDOW
	move.b	d2,(a1)+	;
\3	btst	#19,D0
	beq.b	\4
	move.b	d3,(A1)+	IECLASS_RAWKEY
	move.b	d2,(A1)+	;
	move.b	d4,(A1)+	IECLASS_RAWMOUSE
\4	moveq	#'{',D0
	tst.b	d1
	bne	\5
	moveq	#'}',d0
\5	cmp.b	(-1,a1),d2
	bne	\6
	subq.l	#1,a1
\6	move.b	D0,(A1)+
	sub.l	a0,a1
	move.l	a1,d0
	movem.l	(SP)+,D2-d4
	rts

;IN:
;a0 - ptr to sgr
;a1 - output buffer for ascii string (min 21 chars)
;OUT:
;d0 - length of ascii written
sgr2ascii	move.l	A3,-(SP)
;	moveq	#-$65,D0
	move.l	a1,d1
	move.b	#$9b,(A1)+
	move.b	#'0',(a1)+
	move.b	#';',(a1)+
	move.b	(A0)+,D0
	beq	\8
	lsr.b	#1,d0		btst #0,D0
	bcc	\2
	lea	(ascii.MSG,pc),a3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\2	lsr.b	#1,d0		btst #1,D0
	bcc	\3
	lea	(ascii.MSG0,pc),A3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\3	lsr.b	#1,d0		btst #2,D0
	bcc	\4
	lea	(ascii.MSG1,pc),A3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\4	lsr.b	#1,d0		btst #3,D0
	bcc	\5
	lea	(ascii.MSG2,pc),A3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\5	lsr.b	#1,d0		btst #4,D0
	bcc	\6
	lea	(ascii.MSG3,pc),A3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\6	lsr.b	#1,d0		btst #5,D0
	bcc	\8
	lea	(ascii.MSG4,pc),A3
	move.b	(a3)+,(a1)+
	move.b	(a3)+,(a1)+
\8	addq.l	#1,a0
;	moveq	#0,D0
;	move.b	(A0),D0
;	move.l	d0,-(sp)	background	
	moveq	#40,D0
	add.b	(A0),D0
	move.l	d0,-(sp)	char cell
	moveq	#30,D0
	add.b	-(A0),D0
	move.l	d0,-(sp)	char
	lea	(ddm.MSG,pc),a0
	movea.l	a1,a3
	suba.l	d1,a3
	bsr	_RawDoFmt
	lea	(4,SP),SP
	add.l	a3,d0
	movea.l	(SP)+,A3
	rts

lbC009F68	suba.w	#$28,SP
	move.l	A2,-(SP)
	lea	(4,SP),A1
	movea.l	($38,SP),A2
	move.l	#PGA_Total,(A1)+
	movea.l	($3C,SP),A0
	moveq	#0,D0
	move.w	(A0),D0
	move.l	D0,(A1)+
	move.l	#PGA_Top,(A1)+
	move.w	(4,A0),D0
	move.l	D0,(A1)+
	move.l	#PGA_Visible,(A1)+
	move.w	(2,A0),D0
	move.l	D0,(A1)+
	move.l	#GA_ID,(A1)+
	moveq	#$64,D0
	move.l	D0,(A1)+
	clr.l	(A1)+
	cmpi.l	#OM_UPDATE,(A2)
	bne.b	\1
	move.l	(12,A2),D0
	bra.b	\2

\1	moveq	#0,D0
\2	move.l	D0,-(SP)
	move.l	(8,A2),-(SP)
	pea	(12,SP)
	pea	(OM_NOTIFY).W
	movea.l	sp,a1
	movea.l	($44,SP),a2
	movea.l	($44-4,SP),a0
	bsr	lbC00DA9E
	lea	(16,SP),SP
	movea.l	(SP)+,A2
	adda.w	#$28,SP
	rtd	#16

lbC00DA8A	tst.l	d0
	beq	lbr
	move.l	A2,-(SP)
	movea.l	d0,a2
	movea.l	(-4,A2),A0
	bra	lbC00DAB6

lbC00DA9E	move.l	A2,D0
	beq	lbr
	move.l	A0,D0
	beq	lbr
	move.l	A2,-(SP)
lbC00DAB2	movea.l	(cl_Super,A0),A0
lbC00DAB6	pea	(\r,PC)
	move.l	(8,A0),-(SP)
	rts

\r	movea.l	(SP)+,A2
lbr	rts

lbC00DAC6	tst.l	d1
	beq	lbr
	move.l	A0,D0
	beq	lbr
	move.l	A2,-(SP)
	movea.l	d1,a2
	bra	lbC00DAB2


_chart	dc.b	8	0
	dc.b	0	1
	dc.b	0	2
	dc.b	0	3
	dc.b	0	4
	dc.b	0	5
	dc.b	0	6
	dc.b	3	7 BEL=Bell
	dc.b	3	8 BS=Backspace
	dc.b	5	9 HT=Horizontal Tabulation
	dc.b	7	A LF=LineFeed
	dc.b	7	B VT=Vertical Tabulation
	dc.b	7	C FF=Form Feed
	dc.b	6	D CR=Carriage Return
	dc.b	3	E SO=Shift Out
	dc.b	3	F SI=Shift In
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	3	ESC
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	8	@
	dc.b	8	A
	dc.b	8	B
	dc.b	8	C
	dc.b	8	D
	dc.b	8	E
	dc.b	8	F
	dc.b	0
	dc.b	8	H
	dc.b	8	I
	dc.b	8	J
	dc.b	8	K
	dc.b	8	L
	dc.b	8	M
	dc.b	0
	dc.b	0
	dc.b	8	P
	dc.b	0
	dc.b	8	R
	dc.b	8	S
	dc.b	8	T
	dc.b	0
	dc.b	0
	dc.b	8	W
	dc.b	0
	dc.b	0
	dc.b	8	Z
	dc.b	2	[
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	8	h
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	8	l
	dc.b	8	m
	dc.b	8	n
	dc.b	0
	dc.b	8	p
	dc.b	8	q
	dc.b	8	r
	dc.b	8	s
	dc.b	8	t
	dc.b	8	u
	dc.b	8	v
	dc.b	0
	dc.b	8	x
	dc.b	8	y
	dc.b	0
	dc.b	8	{
	dc.b	8	|
	dc.b	8	}
	dc.b	8	~
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	7	IND=Index
	dc.b	7	NEL=Next Line
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	7	RI=ring indicator
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	1	CSI=Control Sequence Introducer
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0
	dc.b	0

modelclass.MSG	dc.b	'modelclass',0
icclass.MSG	dc.b	'icclass',0
propgclass.MSG	dc.b	'propgclass',0
sysiclass.MSG	dc.b	'sysiclass',0
buttongclass.MSG	dc.b	'buttongclass',0
	dc.b	7
Selectfilenam.MSG	dc.b	'Select filename',0
	dc.b	8
Selectcommand.MSG	dc.b	'Select command',0
	dc.b	9
Selectdevice.MSG	dc.b	'Select device',0
	dc.b	10
Savereview.MSG	dc.b	'Save review',0

lbC00A11C	suba.w	#$20,SP
	movem.l	D2/D3/D5-D7/A2-A6,-(SP)
	move.l	A2,($34,SP)
	clr.l	($44,SP)
	movea.l	A0,A2
	movea.l	A1,A5
	movea.l	($24,A2),A1
	movea.l	($34,SP),A3
	move.l	A0,($30,SP)
	move.l	A1,($2C,SP)
	move.l	(A5),D0
	subi.l	#OM_NEW,D0
	beq.b	lbC00A162
	subq.l	#2,D0		OM_SET
	beq.b	lbC00A1BC
	subq.l	#1,D0		OM_GET
	beq	lbC00A3BC
	subq.l	#4,D0		OM_UPDATE
	beq.b	lbC00A1BC
	bra	lbC00A40E

lbC00A162	movea.l	A5,a1
	move.l	A3,d1
	move.l	A2,a0
	bsr	lbC00DAC6
	move.l	D0,($44,SP)
	beq	lbC00A420
	movea.l	D0,A3
	moveq	#0,D0
	move.w	($20,A2),D0
	movea.l	($2C,SP),A2
	movea.l	($1C,A2),A6
	adda.l	D0,A3
	move.l	#PGA_Total,D0
	moveq	#0,D1
	movea.l	(4,A5),A0
	sys	GetTagData
	move.w	D0,(A3)
	movea.l	(4,A5),A0
	move.l	#PGA_Top,D0
	moveq	#0,D1
	sys	GetTagData
	move.w	D0,(4,A3)
	clr.w	(14,A3)
	bra	lbC00A420

lbC00A1BC	movea.l	A3,A0
	moveq	#0,D0
	move.w	($20,A2),D0
	move.l	A5,a1
	move.l	A0,d1
	move.l	A2,a0
	adda.l	D0,A3
	bsr	lbC00DAC6
	move.l	(4,A5),($40,SP)
	bra	lbC00A3A2

lbC00A1DE	move.l	(A2),D0
	subi.l	#$80000001,D0
	beq	lbC00A25E
	subq.l	#1,D0
	beq	lbC00A25E
	subi.l	#$31005,D0
	beq	lbC00A200
	subq.l	#1,D0
	beq	lbC00A23E
	subq.l	#1,D0
	beq	lbC00A21E
	bra	lbC00A3A2

lbC00A25E	movea.l	($2C,SP),A6
	movea.l	(8,A6),A6
	lea	($3C,SP),A0
	lea	($38,SP),A1
	jsr	(-$54,A6)
	move.w	(14,A3),D0
	bne.b	lbC00A288
	move.l	($3C,SP),D1
	move.l	D1,(6,A3)
	move.l	($38,SP),D0
	move.l	D0,(10,A3)
lbC00A288	cmpi.l	#$108,(A5)
	bne	lbC00A38E
	btst	#0,(15,A5)
	beq	lbC00A38E
	move.w	(14,A3),D0
	beq.b	lbC00A2E2
	move.l	($38,SP),D7
	move.l	(10,A3),D6
	move.l	($3C,SP),D5
	sub.l	(6,A3),D5
	moveq	#9,D1
	cmp.l	D1,D5
	bls.b	lbC00A2C0
	move.l	#$989680,D1
	bra.b	lbC00A2D8

lbC00A2C0	move.l	D5,D1
	asl.l	#6,D1
	sub.l	D5,D1
	move.l	D1,D2
	asl.l	#5,D2
	sub.l	D1,D2
	asl.l	#3,D2
	add.l	D5,D2
	asl.l	#6,D2
	add.l	D7,D2
	sub.l	D6,D2
	move.l	D2,D1
lbC00A2D8	cmpi.l	#$51615,D1
	bls	lbC00A38E
lbC00A2E2	addq.w	#1,(14,A3)
	move.w	(14,A3),D0
	andi.l	#$FFFF,D0
	divu.w	#$19,D0
	moveq	#0,D2
	move.w	D0,D2
	moveq	#0,D0
	move.w	(2,A3),D0
	moveq	#3,D1
	divs.l	D1,D0
	move.l	D0,D6
	addq.l	#1,D6
	cmp.l	D1,D6
	bge.b	lbC00A310
	move.l	D6,D0
	bra.b	lbC00A312

lbC00A310	move.l	D1,D0
lbC00A312	addq.l	#1,D2
	cmp.l	D0,D2
	blt.b	lbC00A324
	cmp.l	D1,D6
	bge.b	lbC00A320
	move.l	D6,D0
	bra.b	lbC00A322

lbC00A320	move.l	D1,D0
lbC00A322	move.l	D0,D2
lbC00A324	move.l	(A2),D0
	cmpi.l	#$80000001,D0
	bne.b	lbC00A34C
	move.w	(4,A3),D1
	cmp.w	D2,D1
	bcc.b	lbC00A33A
	moveq	#0,D2
	bra.b	lbC00A346

lbC00A33A	moveq	#0,D0
	move.w	D2,D0
	moveq	#0,D3
	move.w	D1,D3
	sub.l	D0,D3
	move.l	D3,D2
lbC00A346	move.w	D2,(4,A3)
	bra.b	lbC00A378

lbC00A34C	cmpi.l	#$80000002,D0
	bne.b	lbC00A378
	moveq	#0,D0
	move.w	D2,D0
	moveq	#0,D1
	move.w	(4,A3),D1
	add.l	D0,D1
	move.l	D1,D2
	moveq	#0,D0
	move.w	(2,A3),D0
	moveq	#0,D1
	move.w	(A3),D1
	sub.l	D0,D1
	cmp.l	D1,D2
	bgt.b	lbC00A374
	move.l	D2,D1
lbC00A374	move.w	D1,(4,A3)
lbC00A378	move.l	A3,-(SP)
	move.l	A5,-(SP)
	move.l	($3C,SP),-(SP)
	move.l	($3C,SP),-(SP)
	bsr	lbC009F68
	bra.b	lbC00A3A2

lbC00A200	move.l	(4,A2),D0
	move.w	D0,(A3)
	bra	lbC00A378

lbC00A21E	move.l	(4,A2),D0
	move.w	D0,(4,A3)
	bra	lbC00A378

lbC00A23E	move.l	(4,A2),D0
	move.w	D0,(2,A3)
	bra	lbC00A378

lbC00A38E	cmpi.l	#$108,(A5)
	bne.b	lbC00A3A2
	btst	#0,(15,A5)
	bne.b	lbC00A3A2
	clr.w	(14,A3)
lbC00A3A2	movea.l	($2C,SP),A6
	movea.l	($1C,A6),A6
	lea	($40,SP),A0
	sys	NextTagItem
	movea.l	D0,A2
	tst.l	D0
	bne	lbC00A1DE
	bra.b	lbC00A420

lbC00A3BC	moveq	#0,D0
	move.w	($20,A2),D0
	adda.l	D0,A3
	move.l	(4,A5),D0
	subi.l	#$80031007,D0
	beq.b	lbC00A3E6
	subq.l	#2,D0
	bne.b	lbC00A3F6
	movea.l	(8,A5),A0
	moveq	#0,D0
	move.w	(4,A3),D0
	move.l	D0,(A0)
	movea.w	#1,A3
	bra.b	lbC00A408

lbC00A3E6	movea.l	(8,A5),A0
	moveq	#0,D0
	move.w	(A3),D0
	move.l	D0,(A0)
	movea.w	#1,A3
	bra.b	lbC00A408

lbC00A3F6	move.l	A5,a1
	move.l	($38-4,SP),d1
	move.l	A2,a0
	bsr	lbC00DAC6
	movea.l	D0,A3
lbC00A408	move.l	A3,($44,SP)
	bra.b	lbC00A420

lbC00A40E	move.l	A5,a1
	move.l	A3,d1
	move.l	A2,a0
	bsr	lbC00DAC6
	move.l	D0,($44,SP)
lbC00A420	move.l	($44,SP),D0
	movem.l	(SP)+,D2/D3/D5-D7/A2-A6
	adda.w	#$20,SP
	rts

lbC00A468	move.l	($90,a3),d0
	beq.b	\6
	movem.l	d5-D7/A3/A5/A6,-(SP)
	move.l	a3,d6
	move.l	a5,d5
	movea.l	d0,A5
	move.l	(4,A5),D0
	beq	\1
	tst.l	($1C,A5)
	beq	\1
	move.l	($1C,A5),-(SP)
	pea	($10A).W
	movea.l	sp,a1
;	move.l	D0,-(SP)
	bsr	lbC00DA8A
	addq.l	#8,sp
\1	lea	($18,A5),A3
	moveq	#6,D7
	movea.l	d5,A6
	movea.l	(8,A6),A6
\2	tst.l	(4,A3)
	beq.b	\3
	movea.l	(4,A5,D7.L*4),A0
	sys	DisposeObject
	clr.l	(4,A3)
\3	subq.l	#1,D7
	subq.l	#4,A3
	bpl.b	\2
	move.l	($20,A5),D0
	beq.b	\4
	movea.l	d6,A0
	movea.l	($88,A0),A0
	movea.l	D0,A1
	sys	FreeScreenDrawInfo
\4	move.l	(A5),D0
	beq.b	\5
	movea.l	D0,A0
	sys	FreeClass
	clr.l	(A5)
\5	clr.l	($20,A5)
	movem.l	(SP)+,d5-D7/A3/A5/A6
 	moveq	#0,D0
\6	rts

lbC00A4F4	moveq	#0,D0
	tst.b	($71,A1)
	bne	\2
\1	move.w	d0,($5C,A1)
	move.w	($7A,A1),D0
	lea	($D6,A1,d0.l),A0
	move.l	A0,($66,A1)
	rts

\2	movem.l	a1/A6,-(SP)
	lea	($7C,A1),A0
	move.w	($78,A1),D0
	move.w	d0,($7A,A1)
	lea	($D6,A1),A1
	movea.l	(_exec,A4),A6
	sys	CopyMem
	movem.l	(SP)+,a1/A6
	moveq	#0,d0
	move.b	d0,($71,A1)
	bra	\1


lbC00A672	movem.l	D2-D7/A2/A6,-(SP)
	movea.l	(8,A5),A6
	moveq	#$32,d7		default minheight
	movea.l	($88,A3),A0	screen
	movea.l	(sc_ViewPort+vp_ColorMap,a0),a1
	movea.l	(cm_vpe,a1),a1
	move.w	(vpe_DisplayClip+ra_MaxX,A1),D0
	sub.w	(vpe_DisplayClip+ra_MinX,A1),D0
	addq.w	#1,D0
	cmpi.w	#$190,D0
	bge	\1
	moveq	#13,D6		default sizing gadget width for lores
	moveq	#11,D5		default sizing gadget height for lores
	moveq	#18,D3		default win depth gadget width for lores?
	moveq	#SYSISIZE_LOWRES,D4
	bra	\2

\1	moveq	#18,D6		default sizing gadget width for hires
	moveq	#10,D5		default sizing gadget height for hires
	moveq	#24,D3		default win depth gadget width for hires?
	moveq	#SYSISIZE_MEDRES,D4
\2	sys	GetScreenDrawInfo
	movea.l	d0,a2
	move.l	d0,d1
	beq	\def1
	moveq	#DEPTHIMAGE,d0
	bsr	\newob2
	move.l	D0,d2
	beq	\def1
	move.l	d2,a0
	clr.l	-(sp)
	movea.l	sp,a1
	move.l	#IA_Width,d0
	sys	GetAttr
	move.l	(sp)+,d0	current DEPTHIMAGE width
	bne	.ok
	move.l	d3,d0		default width of win depth gadget
.ok	move.l	d0,d3
	movea.l	d2,a0
	sys	DisposeObject
\def1	move.l	d3,($94+36,sp)	save current width of win depth gadget
	move.l	a2,d1
	moveq	#SIZEIMAGE,d0
	bsr	\newob2
	move.l	D0,d2
	beq	\def2
	move.l	d2,a0
	clr.l	-(sp)
	movea.l	sp,a1
	move.l	#IA_Height,d0
	sys	GetAttr
	move.l	(sp)+,d0	current SIZEIMAGE height
	bne	.ok1
	move.l	d5,d0		default height of sizing gadget
.ok1	move.l	d0,d5		
	movea.l	d2,a0
	sys	DisposeObject
\def2	movea.l	($88,A3),A0	screen
	movea.l	(sc_Font,A0),A1
	moveq	#0,D0
	move.w	(ta_YSize,A1),D0
	move.b	(sc_WBorTop,A0),D2
	extb.l	d2
	add.l	D0,D2
	addq.l	#2,D2		top
	move.l	d2,d7
	addq.l	#1,d7
	add.l	d5,d7		+sizing gadget height
	addq.l	#8,d7
	move.l	($90,A3),d0
	bne	.3
	movea.l	a2,a1
	sys	FreeScreenDrawInfo
	moveq	#0,d0
	bra	\9

.3	exg	d0,a2
	move.l	d0,($20,a2)
	beq	\7
	btst	#5,($163,A3)
	beq.b	\3
\3	move.b	(sc_WBorLeft,A0),D0
	extb.l	D0
	moveq	#0,d1
	move.w	(sc_Width,A0),D1
	sub.l	D0,D1
	btst	#4,($161,A3)
	beq.b	\4
	move.b	(sc_WBorRight,A0),D0
	extb.l	D0
	sub.l	D0,D1
	move.w	D1,($5E,A2)
	moveq	#0,D0
	bra	\9

\4	move.l	D6,D0
	sub.l	D0,D1
	move.w	D1,($5E,A2)
	move.l	A2,-(SP)
	suba.l	A0,A0
	suba.l	A2,A2
	lea	(modelclass.MSG,PC),A1
	moveq	#$10,D0
	moveq	#0,D1
	sys	MakeClass
	tst.l	D0
	beq.b	\6
	movea.l	D0,A1
	lea	(lbC00A11C,PC),A0
	move.l	A0,(8,A1)
	clr.l	(12,A1)
	move.l	A5,($24,A1)
\6	movea.l	(SP)+,A2
	move.l	D0,(A2)
	bne.b	\8
\7	bsr	lbC00A468
	bra	\9

\newob	move.l	($20,A2),d1
\newob2	move.l	a2,-(sp)
	clr.l	-(SP)
	move.l	d1,-(SP)
	move.l	#SYSIA_DrawInfo,-(SP)
	move.l	d0,-(sp)
	move.l	#SYSIA_Which,-(SP)
	move.l	D4,-(SP)
	move.l	#SYSIA_Size,-(SP)
	clr.l	-(SP)
	move.l	#IA_Top,-(SP)
	clr.l	-(SP)
	move.l	#IA_Left,-(SP)
	suba.l	A0,A0
	lea	(sysiclass.MSG,PC),A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(11*4,SP),SP
	movea.l	(SP)+,A2
	rts

\8	move.l	A2,-(SP)
	clr.l	-(SP)
	suba.l	A1,A1
	movea.l	(A2),A0
	movea.l	SP,A2
	sys	NewObjectA
	addq.l	#4,sp
	movea.l	(SP)+,A2
	move.l	D0,(4,A2)
	beq	\7
	move.l	A2,-(SP)
	clr.l	-(SP)
	pea	(-1).W
	move.l	#ICA_TARGET,-(SP)
	suba.l	A0,A0
	lea	(icclass.MSG,PC),A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(3*4,SP),SP
	movea.l	(SP)+,A2
	move.l	D0,($1C,A2)
	beq	\7
	moveq	#UPIMAGE,d0
	bsr	\newob
	move.l	D0,(12,A2)
	beq	\7
	moveq	#DOWNIMAGE,d0
	bsr	\newob
	move.l	D0,(16,A2)
	beq	\7
	movea.l	(16,a2),a0
	clr.l	-(sp)
	movea.l	sp,a1
	move.l	#IA_Height,d0
	sys	GetAttr
	move.l	(sp),d4		current DOWNIMAGE height
	bne	.ok2
	moveq	#11,d4		default height of an arrow
.ok2	movea.l	(16,a2),a0
	clr.l	(sp)
	movea.l	sp,a1
	move.l	#IA_Width,d0
	sys	GetAttr
	move.l	(sp)+,d0	current DOWNIMAGE width
	bne	.ok3
	move.l	d6,d0
.ok3	move.l	d0,d6
	move.l	A2,-(SP)
	add.l	d4,d7
	add.l	d4,d7		there are two arrows
	move.l	d7,d1
	subq.l	#8,d1
	neg.l	D1		relheight
	clr.l	-(SP)
	pea	(1).W
	move.l	#PGA_NewLook,-(SP)
	pea	(2).W
	move.l	#PGA_Visible,-(SP)
	pea	(2).W
	move.l	#PGA_Total,-(SP)
	clr.l	-(SP)
	move.l	#PGA_Top,-(SP)
	pea	(1).W
	move.l	#PGA_Borderless,-(SP)
	pea	(1).W
	move.l	#GA_RightBorder,-(SP)
	move.l	D1,-(SP)
	move.l	#GA_RelHeight,-(SP)
	move.l	D2,-(SP)
	move.l	#GA_Top,-(SP)
	moveq	#-6,d0
	moveq	#2,D1
	moveq	#15,d2
	cmp.l	d2,d6
	ble	.skip
	moveq	#-8,d0
	moveq	#3,D1
.skip	add.l	D6,D0		gadget width
	move.l	D0,-(SP)
	move.l	#GA_Width,-(SP)
	add.l	d1,d0
	neg.l	d0
	move.l	D0,-(SP)
	move.l	#GA_RelRight,-(SP)
	move.l	(4,A2),-(SP)
	suba.l	A0,A0
	move.l	#ICA_TARGET,-(SP)
	lea	(propgclass.MSG,PC),A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(23*4,SP),SP
	movea.l	(SP)+,A2
	move.l	D0,(8,A2)
	beq	\7
	move.l	A2,-(SP)
	move.l	d4,d1
	add.l	d1,d1
	add.l	d5,d1		calculate window minheight
	subq.l	#1,d1
	neg.l	d1
	moveq	#1,D2
	move.l	D2,D3
	sub.l	D6,D3
	clr.l	-(SP)
	move.l	(12,A2),-(SP)
	move.l	#GA_Image,-(SP)
	move.l	D2,-(SP)
	move.l	#GA_RightBorder,-(SP)
;	pea	(11).W
;	move.l	#GA_Height,-(SP)
	move.l	D6,-(SP)
	move.l	#GA_Width,-(SP)
	move.l	D3,-(SP)
	move.l	#GA_RelRight,-(SP)
	move.l	D1,-(SP)
	move.l	#GA_RelBottom,-(SP)
	move.l	(8,A2),-(SP)
	move.l	#GA_Previous,-(SP)
	pea	(lbL00F1B0,pc)
	move.l	#ICA_MAP,-(SP)
	move.l	(4,A2),-(SP)
	move.l	#ICA_TARGET,-(SP)
	suba.l	A0,A0
	lea	(buttongclass.MSG,PC),A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(17*4,SP),SP
	movea.l	(SP)+,A2
	move.l	D0,($14,A2)
	beq	\7
	subq.l	#1,d4	
	neg.l	d4
	sub.l	D5,D4
	move.l	A2,-(SP)
	clr.l	-(SP)
	move.l	(16,A2),-(SP)
	move.l	#GA_Image,-(SP)
	move.l	D2,-(SP)
	move.l	#GA_RightBorder,-(SP)
;	pea	(11).W
;	move.l	#GA_Height,-(SP)
	move.l	D6,-(SP)
	move.l	#GA_Width,-(SP)
	move.l	D3,-(SP)
	move.l	#GA_RelRight,-(SP)
	move.l	D4,-(SP)
	move.l	#GA_RelBottom,-(SP)
	move.l	($14,A2),-(SP)
	move.l	#GA_Previous,-(SP)
	pea	(lbL00F1C0,pc)
	move.l	#ICA_MAP,-(SP)
	move.l	(4,A2),-(SP)
	move.l	#ICA_TARGET,-(SP)
	suba.l	A0,A0
	lea	(buttongclass.MSG,PC),A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(17*4,SP),SP
	movea.l	(SP)+,A2
	move.l	D0,($18,A2)
	beq	\7
	clr.l	-(SP)
	clr.l	-(SP)
	move.l	#PGA_Visible,-(SP)
	clr.l	-(SP)
	move.l	#PGA_Top,-(SP)
	clr.l	-(SP)
	move.l	#PGA_Total,-(SP)
	move.l	(8,A2),-(SP)
	move.l	#ICA_TARGET,-(SP)
	movea.l	(4,A2),A0
	movea.l	SP,A1
	sys	SetAttrsA
	lea	(9*4,SP),SP
	move.l	($1C,A2),-(SP)
	pea	($109).W
	move.l	(4,A2),d0
	movea.l	sp,a1
	bsr	lbC00DA8A
	addq.l	#8,sp
	move.l	(8,A2),D0
\9	moveq	#$32,d1		default minheight
	cmp.l	d1,d7
	bcc	\10
	move.l	d1,d7
\10	move.l	d7,($90+36,sp)		minheight
	movem.l	(SP)+,D2-D7/A2/A6
	rts

lbC00AAC2	subq.l	#8,SP
	movem.l	D5-D7/A2/A3/A5,-(SP)
	move.l	D1,D6
	move.l	D0,D7
	movea.l	($24,SP),A3
	move.l	A0,($18,SP)
	move.l	A1,($1C,SP)
	cmp.l	D7,D6
	blt.b	\1
	move.l	D7,d0
;	move.l	A1,-(SP)
	move.l	A0,d1
	move.l	A3,a0
	bra.b	\7

\1	movea.l	A3,A5
	adda.l	D7,A5
	movea.l	A3,A2
	bra.b	\4

\2	cmpi.b	#$9B,(A2)
	lea	(1,A2),A3
	bne.b	\3
	movea.l	A3,A0
	bsr	lbC00936C
	movea.l	D0,A0
	lea	(1,A0),A2
	bra.b	\4

\3	subq.l	#1,D6
	movea.l	A3,A2
\4	cmpa.l	A5,A2
	bcc.b	\5
	tst.l	D6
	bgt.b	\2
\5	movea.l	($24,SP),A1
	cmpa.l	A1,A2
	bls.b	\8
	movea.l	A1,A0
	adda.l	D7,A0
	cmpa.l	A0,A2
	movea.l	($18,SP),A5
	bcc.b	\6
	moveq	#10,D0
	cmp.b	(-1,A1,D7.L),D0
	bne.b	\6
	move.b	(A2),D5
	move.b	D0,(A2)
	move.l	A2,D0
	move.l	A1,D1
	sub.l	D1,D0
	addq.l	#1,D0
;	move.l	D0,-(SP)
	move.l	D1,a0
	move.l	($24-8,SP),a1
	move.l	A5,d1
	bsr	_doio
	move.b	D5,(A2)
	bra.b	\8

\6	move.l	A2,D0
	move.l	A1,D1
	sub.l	D1,D0
;	move.l	D0,-(SP)
	move.l	D1,a0
	move.l	($24-8,SP),a1
	move.l	A5,d1
\7	bsr	_doio
\8	movem.l	(SP)+,D5-D7/A2/A3/A5
	addq.l	#8,SP
	rtd	#4

lbC00AB70	suba.w	#$1C,SP
	movem.l	D2/D4-D6/A2/A3/A5,-(SP)
	movea.l	($40,SP),A5
	movea.l	($90,A5),A3
	move.w	($46,SP),D5
	move.w	($5A,A3),D0
	cmp.w	D5,D0
	bne.b	\1
	tst.w	($FE,A5)
	beq	\37
\1	movea.l	($98,A5),A0
	movea.l	($18,A0),A1
	move.w	($2C,A1),D6
	addq.w	#1,D6
	moveq	#0,d4
	move.w	($2A,A1),D4
	addq.w	#1,D4
	moveq	#0,D0
	move.w	D5,D0
	cmp.l	($32,A3),D0
	blt.b	\2
	moveq	#1,D5
	move.w	#1,($34,SP)
	lea	($28,A3),A0
	move.l	A0,($56,A3)
	movea.l	A0,A2
	lea	($1C,SP),a0
	move.b	#12,(A0)
	moveq	#1,D0
	move.w	D0,($32,SP)
	bra	\27

\2	tst.w	($FE,A5)
	bne.b	\3
	moveq	#0,D0
	move.w	D6,D0
	moveq	#0,D1
	move.w	($5A,A3),D1
	move.l	D1,D2
	add.l	D0,D2
;	moveq	#0,D0
	move.w	D5,D0
	cmp.l	D2,D0
	bge.b	\3
;	moveq	#0,D0
	move.w	D1,D0
;	moveq	#0,D1
	move.w	D6,D1
	moveq	#0,D2
	move.w	D5,D2
	add.l	D1,D2
	cmp.l	D0,D2
	bgt	\16
\3	move.w	#1,($34,SP)
	lea	($1C,SP),a0
	move.w	D6,D5
	move.b	#12,(A0)
	moveq	#1,D0
	move.w	($46,SP),D6
	movea.l	($56,A3),A2
	move.w	D0,($32,SP)
	move.w	($5A,A3),D0
	cmp.w	D0,D6
	bls.b	\9
	move.l	D6,D1
	sub.w	D0,D6
;	moveq	#0,D0
	move.w	D1,D0
	move.l	($32,A3),D1
	sub.l	D0,D1
	subq.l	#1,D1
;	moveq	#0,D0
	move.w	D6,D0
	cmp.l	D1,D0
	bgt.b	\6
	bra.b	\5

\4	movea.l	(A2),A2
\5	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	bne.b	\4
	bra.b	\15

\6	move.l	D1,D6
	movea.l	($2C,A3),A2
	bra.b	\8

\7	movea.l	(4,A2),A2
\8	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	bne.b	\7
	bra.b	\15

\9	move.l	D6,D0
	move.w	($5A,A3),D1
	sub.w	D0,D1
	move.l	D1,D6
	cmp.w	D0,D6
	bhi.b	\12
	bra.b	\8

\12	move.w	($46,SP),D6
	movea.l	($24,A3),A2
	bra.b	\14

\13	movea.l	(A2),A2
\14	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	bne.b	\13
\15	move.l	A2,D0
	beq	\27
	bra	\26

\16	move.w	($5A,A3),D0
	cmp.w	D0,D5
	bls.b	\21
	sub.w	D0,D5
	moveq	#0,D0
	move.w	D5,D0
	moveq	#0,D1
	move.w	D6,D1
	sub.l	D0,D1
	addq.l	#1,D1
	move.w	D1,($3C-8,SP)
	move.l	D0,d1
	lea	($20-4,SP),a0
	bsr	lbC009AA2
	movea.l	($56,A3),A2
	move.w	D5,D6
	move.w	D0,($32,SP)
	bra.b	\18

\17	movea.l	(A2),A2
\18	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	bne.b	\17
	move.l	A2,($56,A3)
	move.w	($34,SP),D6
	subq.w	#1,D6
	bra.b	\20

\19	movea.l	(A2),A2
\20	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	beq.b	\27
	move.l	A2,D0
	bne.b	\19
	bra.b	\27

\21	move.w	($5A,A3),D0
	sub.w	D5,D0
	move.l	D0,D5
	move.w	#1,($34,SP)
	moveq	#0,D1
	move.w	D5,D1
	lea	($1c,SP),a0

\22	bsr	lbC009AA0
	movea.l	($56,A3),A2
	move.w	D5,D6
	move.w	D0,($32,SP)
	bra.b	\25

\24	movea.l	(4,A2),A2
\25	move.l	D6,D0
	subq.w	#1,D6
	tst.w	D0
	bne.b	\24
	move.l	A2,D0
	beq.b	\27
\26	move.l	A2,($56,A3)
\27	tst.w	($4A,SP)
	beq.b	\28
	move.w	($32,SP),D6
	moveq	#0,D0
	move.w	D6,D0
	lea	($1C,SP),A0
	adda.l	D0,A0
	moveq	#0,D1
	move.w	($34,SP),D1
	bsr	lbC009B1E
	moveq	#0,D1
	move.w	D6,D1
	pea	($24-8,SP)
	add.l	D0,D1
	moveq	#0,D0
	move.w	D1,D0
	move.l	D4,D1
	movea.l	($48-8,SP),A0
	movea.l	A5,A1
	bsr	lbC00AAC2
	bra.b	\29

\28	moveq	#0,D5
\29	moveq	#0,D0
	cmp.w	D0,D5
	bls	\35
	move.l	A2,D0
	beq.b	\30
	tst.l	(A2)
	beq.b	\30
	pea	($12,A2)
	move.l	D4,D1
	move.l	(14,A2),D0
	movea.l	($40,SP),A0
	movea.l	A5,A1
	bsr	lbC00AAC2
	movea.l	A2,A0
	movea.l	(A0),A2
	bra.b	\31

\30	move.l	A2,D0
	beq.b	\31
	tst.w	($5C,A3)
	bls.b	\31
	lea	($D6,A3),A0
	move.l	($66,A3),D0
	move.l	A0,D1
	sub.l	D1,D0
	move.l	D4,D2
	move.l	D1,-(SP)
	move.l	D2,D1
	movea.l	($40,SP),A0
	movea.l	A5,A1
	bsr	lbC00AAC2
\31	subq.w	#1,D5
	bra.b	\35

\32	tst.l	(A2)
	beq.b	\33
	lea	($12,A2),A0
	move.l	(10,A2),D0
	sub.l	A0,D0
	moveq	#0,D1
	move.w	D0,D1
	movea.l	A2,A0
	adda.l	D1,A0
;	moveq	#0,D1
;	move.w	D0,D1
	move.l	(14,A2),D0
	pea	($12,A0)
	sub.l	D1,D0
	move.l	D4,D1
	movea.l	($40,SP),A0
	movea.l	A5,A1
	bsr	lbC00AAC2
	bra.b	\34

\33	tst.w	($5C,A3)
	bls.b	\34
	movea.l	A3,A0
	moveq	#0,D0
	move.w	($7A,A3),D0
	adda.l	D0,A0
	lea	($D6,A0),A1
	lea	($D6,A3),A0
	moveq	#0,D1
	move.w	D0,D1
	move.l	($66,A3),D0
	sub.l	A0,D0
	sub.l	D1,D0
	move.l	D4,D1
	move.l	A1,-(SP)
	movea.l	($40,SP),A0
	movea.l	A5,A1
	bsr	lbC00AAC2
\34	movea.l	A2,A0
	movea.l	(A0),A2
\35	move.l	D5,D0
	subq.w	#1,D5
	tst.w	D0
	beq.b	\36
	move.l	A2,D0
	bne.b	\32
\36	move.w	($46,SP),($5A,A3)
	clr.w	($FE,A5)
\37	movem.l	(SP)+,D2/D4-D6/A2/A3/A5
	adda.w	#$1C,SP
	rtd	#16

lbC00AE7A	move.l	($90,A0),d0
	beq	\5
	movem.l	D7/A2/A3/A6,-(SP)
	movea.l	(8,A1),A6
	movea.l	([$98,A0],24),A1
	moveq	#0,d7
	move.w	($2C,A1),D7
	addq.w	#1,D7
	btst	#4,($161,A0)
	bne	\4
	tst.l	($78,A0)
	beq	\4
	tst.l	($168,A0)
	bne	\4
	movea.l	d0,a3
	move.l	($32,A3),D1
	addq.l	#1,D1
	moveq	#0,d0
	move.w	($5A,A3),D0
	cmpi.w	#$FFFF,D0
	bne.b	\3
	cmp.w	D7,D1
	bcc.b	\1
	moveq	#0,D0
	bra.b	\3

\1	move.l	D1,D0
	sub.l	D7,D0
\3	clr.l	-(SP)
	move.l	D1,-(SP)
	move.l	#PGA_Total,-(SP)
	move.l	D7,-(SP)
	move.l	#PGA_Visible,-(SP)
	move.l	D0,-(SP)
	move.l	#PGA_Top,-(SP)
	movea.l	($78,A0),A1
	addq.l	#4,a3
	move.l	(a3)+,d7
	movea.l	(A3),A0
	suba.l	A2,A2
	movea.l	sp,a3
	sys	SetGadgetAttrsA
	movea.l	d7,A0
	movea.l	sp,A1
	sys	SetAttrsA
	lea	($1C,SP),SP
\4	movem.l	(SP)+,D7/A2/A3/A6
\5	rts

lbC00AF60	move.l	($90,a5),d0
	beq	\1
	move.l	A3,-(SP)
	movea.l	D0,a3
	movea.l	($46,a3),A0
	pea	($24,a3)
	move.l	a2,-(SP)
	jsr	(A0)
	addq.l	#8,sp
	clr.l	($9A,a3)
	movea.l	a3,A1
	bsr	lbC00A4F4
	move.l	a5,a0
	move.l	a2,a1
	bsr	lbC00AE7A
	movea.l	(SP)+,a3
\1	rts


lbC00AF9E	movem.l	D2/D6/D7/A2/A5,-(SP)
	move.w	#1,($FE,A0)
	move.l	($90,A0),d0
	movea.l	a1,A5
	beq	\4
	movea.l	d0,a2
	tst.l	($56,A2)
	beq	\4
	movea.l	($78,a0),a1	window
	move.l	(wd_Width,a1),d0
	move.l	d0,($140,a0)
	moveq	#1,D0
	move.w	D0,($FE,A0)
	movea.l	([$98,A0],$18),A1
	move.w	($2C,A1),D6
	addq.w	#1,D6
	tst.w	($5C,A2)
	shi	D0
	moveq	#0,D1
	sub.b	D0,D1
	move.l	($32,A2),D7
	add.l	D1,D7
	move.w	D6,D0
	move.w	($5A,A2),D1
	move.l	D7,D2
	sub.l	D1,D2
	cmp.l	D0,D2
	bge.b	\3
	move.w	D6,D0
	cmp.l	D0,D7
	bge.b	\1
	moveq	#0,D0
	bra.b	\2

\1	sub.l	D0,D7
	move.l	D7,D0
	bra.b	\2

\3	move.w	D1,D0
\2	pea	(1).W
	move.l	D0,-(SP)
	move.l	A0,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
\4	movem.l	(SP)+,D2/D6/D7/A2/A5
	rts

lbC00B032	movem.l	D7/A2/A3/A5,-(SP)
	movea.l	A1,A3
	movea.l	a3,A2
	adda.l	d0,A2
	movea.l	a0,A5
	moveq	#0,D7
	bra.b	\4

\1	cmpi.b	#$9B,(A3)
	bne.b	\2
	movea.l	A3,A0
	bsr	lbC00936C
	movea.l	D0,A3
	bra.b	\3

\2	addq.l	#1,D7
	move.b	(A3),(A5)+
\3	addq.l	#1,A3
\4	cmpa.l	A2,A3
	bcs.b	\1
	move.l	D7,D0
	movem.l	(SP)+,D7/A2/A3/A5
	rts

; save buffer to file
lbC00B06A	movem.l	D3-D5/D7/A2/A3/A5/A6,-(SP)
	exg	a2,a5
	movea.l	($90,a2),A2
	moveq	#0,D5
	move.l	a1,d4
	move.l	A2,-(SP)
	moveq	#11,d0	Open()
	move.l	#MODE_NEWFILE,d1
	movea.l	a5,a0
	bsr	_l2
	move.l	D0,D7
	beq	\14
	moveq	#11,D0
	cmp.b	d2,D0
	movea.l	($24,A2),A3
	bne	\6
	lea	($D6,A2),A0
	move.l	($62,A2),D0
	sub.l	A0,D0
	moveq	#MEMF_PUBLIC,D1
	movea.l	(_exec,A4),A6
	sys	AllocVec
	movea.l	D0,A2
	tst.l	D0
	beq	\12
	bra.b	\3

\2	move.l	(14,A3),d0
	move.l	A2,a0
	lea	($12,A3),a1
	bsr	lbC00B032
	move.l	D0,D3
	move.l	A2,a1
	move.l	D7,d1
	bsr	_Write
	cmp.l	D3,D0
	bne.b	\4
	movea.l	(A3),A3
\3	tst.l	(A3)
	bne.b	\2
\4	movea.l	(SP),A1
	tst.w	($5C,A1)
	bls.b	\5
	lea	($D6,A1),A0
	move.l	($66,A1),D0
	move.l	A0,D1
	sub.l	D1,D0
	move.l	A2,a0
	move.l	D1,a1
	bsr	lbC00B032
	move.l	A2,a1
	move.l	D7,d1
	bsr	_Write
\5	movea.l	A2,A1
	movea.l	(_exec,A4),A6
	sys	FreeVec
	bra	\11

\6	tst.l	(A3)
	beq.b	\9
	move.l	(14,A3),d0
	lea	($12,A3),a1
	move.l	D7,d1
	bsr	_Write
	bra.b	\8

\7	movea.l	(10,A3),a1
	move.l	a1,D1
	lea	($12,A3),A0
	sub.l	A0,D1
	move.l	(14,A3),D3
	sub.l	D1,D3
	move.l	D3,d0
	move.l	D7,d1
	bsr	_Write
	cmp.l	D3,D0
	bne.b	\10
\8	movea.l	A3,A0
	movea.l	(A0),A3
\9	tst.l	(A3)
	bne.b	\7
\10	tst.w	($5C,A2)
	bls.b	\11
	movea.l	A2,A0
	moveq	#0,D0
	move.w	($7A,A2),D0
	adda.l	D0,A0
	lea	($D6,A0),A1
	lea	($D6,A2),A0
	moveq	#0,D1
	move.w	D0,D1
	move.l	($66,A2),D0
	sub.l	A0,D0
	sub.l	D1,D0
	move.l	D7,d1
	bsr	_Write
\11	moveq	#1,D5
\12	move.l	D7,a1
	move.l	A5,a0
	moveq	#12,d0		Close()
	bsr	_l1
\13	movea.l	d4,a1
	moveq	#13,d0		SetProtection()
	moveq	#2,d1
	movea.l	a5,a0
	bsr	_l2

\14	move.w	D5,D0
	addq.l	#4,sp
	movem.l	(SP)+,D3-D5/D7/A2/A3/A5/A6
	rts

;ins:
;a0 - ascii buffer
;a1 - SGR longword
ascii2sgr	movem.l	d6-D7/A2/A3/A5,-(SP)
	moveq	#0,d6
	movea.l	a0,A5
	movea.l	A5,A2
	movea.l	a1,A3
\1	move.b	(A5),D0
	moveq	#';',D1
	cmp.b	D1,D0
	beq.b	\2
	moveq	#'m',D1
	cmp.b	D1,D0
	bne	\22
\2	lea	(-1,A5),a0
	movea.l	A2,a1
	bsr	ascii2long
	move.l	D0,D7
	beq	.chk
	moveq	#40,D1
	cmp.l	D1,D0
	blt.b	\3
	moveq	#49,d0
	cmp.l	d0,d7
	blt	.set
	move.b	(_sgr+2,a4),(2,a3)
	bra	\21
	
.set	sub.l	D1,D7
	move.b	D7,(2,A3)	set cursor cell colour
	bra	\21

.chk	cmpi.b	#'>',(a2)+
	bne	\5
	moveq	#1,d6
	bra	\2

\3	moveq	#39,D0
	cmp.l	D0,D7
	bne.b	\4
	move.b	(_sgr+1,a4),(1,a3)
;	clr.b	(2,A3)		default cursor cell colour =0
;	move.b	#1,(1,A3)	default cursor colour =1
	bra	\21

\4	moveq	#30,D0
	cmp.l	D0,D7
	blt.b	\5
	sub.l	D0,D7
	move.b	D7,(1,A3)	set cursor colour
	bra	\21

\b	move.b	d7,(3,a3)	set background colour
	bra	\21

\5	tst.l	d6
	bne	\b
	moveq	#29,D0
	cmp.l	d0,d7
	bcc	\21		=not used
	move.w	(\6,PC,D7.W*2),D0
	jmp	(\7,PC,D0.W)

\6	dc.w	\8-\7	0=normal cols and attribs
\7	dc.w	\9-\7	1=bold
	dc.w	\10-\7	2=faint
	dc.w	\11-\7	3=italic
	dc.w	\12-\7	4=underscore
	dc.w	\21-\7	5=not used
	dc.w	\21-\7	6=not used
	dc.w	\13-\7	7=reversed
	dc.w	\14-\7	8=concealed
	dc.w	\21-\7	9=not used
	dc.w	\21-\7	10=not used
	dc.w	\21-\7	11=not used
	dc.w	\21-\7	12=not used
	dc.w	\21-\7	13=not used
	dc.w	\21-\7	14=not used
	dc.w	\21-\7	15=not used
	dc.w	\21-\7	16=not used
	dc.w	\21-\7	17=not used
	dc.w	\21-\7	18=not used
	dc.w	\21-\7	19=not used
	dc.w	\21-\7	20=not used
	dc.w	\21-\7	21=not used
	dc.w	\15-\7	22=bold off
	dc.w	\16-\7	23=italics off
	dc.w	\17-\7	24=underscore off
	dc.w	\21-\7	25=not used
	dc.w	\21-\7	26=not used
	dc.w	\18-\7	27=reversed off
	dc.w	\19-\7	28=concealed off

\8	move.l	(_sgr,a4),(a3)
	bra.b	\21

\9	bset	#0,(A3)
	bra.b	\21

\10	bset	#1,(A3)
	bra.b	\21

\11	bset	#2,(A3)
	bra.b	\21

\12	bset	#3,(A3)
	bra.b	\21

\13	bset	#4,(A3)
	bra.b	\21

\14	bset	#5,(A3)
	bra.b	\21

\15	moveq	#0,d0
	bra.b	\20

\16	moveq	#2,d0
	bra.b	\20

\17	moveq	#3,d0
	bra.b	\20

\18	moveq	#4,d0
	bra.b	\20

\19	moveq	#5,d0
\20	bclr	d0,(a3)
\21	lea	(1,A5),A2
	moveq	#'m',D0
	cmp.b	(A5),D0
	beq.b	\23
\22	addq.l	#1,A5
	bra	\1

\23	movem.l	(SP)+,d6-D7/A2/A3/A5
	rts

lbC00B2E6	tst.b	d3
	bne	\6
;	move.l	A2,-(SP)
;	movea.l	($90,a3),a2
	lea	($66,a2),A0
	movea.l	(A0),A1
	addq.l	#1,(A0)
	move.b	#10,(A1)+
	move.l	a1,d0
	lea	($D6,a2),A1
;	move.l	(A0),D0
;	move.l	A1,D1
	sub.l	a1,D0
	move.l	D0,-(SP)
	move.l	a1,-(SP)
	pea	($24,a2)
	move.l	a5,-(SP)
	movea.l	($42,a2),a0
	jsr	(A0)
	lea	(16,SP),SP
	btst	#0,($161,a3)
	bne.b	\2
	tst.l	d0
	beq.b	\1
	movea.l	d0,A0
	moveq	#0,D0
	move.w	($7A,a2),D0
	lea	($12,a0,d0.l),a1
	move.l	A1,(10,a0)
	adda.l	D0,A0
\1	movea.l	a2,A1
	bsr	lbC00A4F4
	bra.b	\3

\2	lea	($D6,a2),A0
	move.l	A0,($66,a2)
	clr.w	($5C,a2)
\3	tst.l	($78,a3)
	beq.b	\6
	btst	#0,($35,a2)
	bne.b	\4
	btst	#1,($162,a3)
	bne.b	\5
	btst	#5,($160,a3)
	beq.b	\5
\4	move.l	($32,a2),D0
	moveq	#$1D,D1
	divsl.l	D1,D1:D0
	tst.l	D1
	bne.b	\6
\5	move.l	a3,a0
	move.l	a5,a1
	bsr	lbC00AE7A
\6	;movea.l	(SP)+,A2
	rts

lbC00B3A6	movem.l	D2-D7/A2/A3/A5/a6,-(SP)
;	illegal
	movea.l	a1,A5
	movea.l	d1,A3
	move.l	($90,A3),d2	buffer was allocated?
	beq	\38
	movea.l	D2,A2
	btst	#3,($162,A3)	review is disabled?
	sne	d3
;	bne	\38
	beq	\0
	move.l	($66,a2),-(sp)
\0	clr.l	-(sp)
	movea.l	a0,A6		printed txt
	moveq	#0,d2
	move.b	(a6),d4
	move.l	d0,D7		38 its length
	tst.l	($78,A3)	window
	bne.b	\1
	moveq	#0,D6
	move.w	($60,A2),D6
	bra	\37

\1	movea.l	([$98,A3],io_Unit),A1	ConUnit
	moveq	#0,d0
	move.w	(cu_XRSize,A1),D0	char raster size
	moveq	#0,D6
	move.w	($5E,A2),D6		max # of columns for this screen
	divs.l	D0,D6
	subq.l	#1,D6
	move.w	D6,($60,A2)
	bra	\37

\loop	moveq	#0,d0
	move.b	(A6)+,D2
	move.l	($6A,A2),D1
	beq	\15
	addq.l	#1,($6A,A2)
	movea.l	D1,A0
	moveq	#'>',D1
	cmp.b	D1,D2
	bne.b	\3
	move.l	a0,(SP)
\3	move.b	D2,(A0)
	movea.l	($6A,A2),A1
	movea.l	($66,A2),A0
	suba.l	a0,a1
	moveq	#$1E,D1
	cmpa.l	D1,a1
	ble.b	\4
	move.l	A0,($6A,A2)
\4	move.b	(_chart,pc,d2.w),d0
	beq	\37
	subq.b	#8,d0
	bne	\37
	cmpi.b	#$9B,(A0)
	bne	\37
	move.l	d2,D1
	moveq	#'A',D0
	sub.l	D0,D1
	beq	\A		A cursor up
	subq.l	#1,D1
	beq	\B		B cursor down
	subq.l	#3,D1
	beq	\B		E cursor next line
	subq.l	#1,D1
	beq	\A		F cursor preceding line
	subq.l	#7,D1
	beq	\M		M delete line
	moveq	#32,D0
	sub.l	D0,D1
	beq	\m		m select gfx rendition
	subq.l	#6,d1		s aSDSS set current SGR as default
	bne	\14

\s	cmp.b	(-2,a0,a1.l),d0	space? (aSDSS is $20,$73)
	bne	\14
	move.l	($74,A2),(_sgr,a4)	save current SGR
	st	(_asdss,a4)
	bra	\14

\m	lea	($74,A2),a1
	addq.l	#1,a0
	bsr	ascii2sgr
	move.l	(SP),d0
	beq	.5
	clr.l	(sp)
	movea.l	d0,a1
	sub.l	($66,A2),d0
	move.b	($74+3,a2),(_sgr+3,a4)	save background
	subq.l	#1,d0
	beq	\5
	move.b	#'m',(-1,a1)
	move.l	a1,($6a,a2)
.5	btst	#0,($161,A3)	NOSTYLES?
	bne.b	\5
	lea	($74,A2),A0
	lea	($7C,A2),a1
	bsr	sgr2ascii
	move.w	D0,($78,A2)
	move.l	($6A,A2),($66,A2)
;	move.w	#1,($70,A2)
	st.b	($71,a2)
\5	clr.b	($6E,A2)
	bra.b	\14

\B	clr.w	($72,A2)
	movea.l	($66,A2),A0
	addq.l	#1,A0
	movea.l	($6A,A2),A1
	cmpa.l	A0,A1
	bls.b	\7
	subq.l	#1,a1
	exg	a0,a1
	bsr	ascii2long
	bra.b	\8

\7	moveq	#1,D0
\8	move.l	D0,D5
	bra.b	\10

\9	bsr	lbC00B2E6
\10	move.l	D5,D0
	subq.l	#1,D5
	tst.l	D0
	bgt.b	\9
	bra.b	\13

\A	tst.w	($5C,A2)
	bls.b	\14
	clr.w	($72,A2)
	bsr	lbC00B2E6
	bra.b	\13

\M	clr.w	($72,A2)
	movea.l	A2,A1
	bsr	lbC00A4F4
\13	move.b	#10,($6E,A2)
\14	clr.l	($6A,A2)
	bra	\37

\15	move.b	(_chart,PC,d2.w),d0
	move.w	(\16,PC,D0.W*2),D1
	moveq	#13,d0
	jmp	(\17,PC,D1.W)

\16	dc.w	0$-\17	0
\17	dc.w	1$-\17	1
	dc.w	2$-\17	2
	dc.w	3$-\17	3
	dc.w	4$-\17	4
	dc.w	5$-\17	5
	dc.w	.6-\17	6
	dc.w	7$-\17	7
	dc.w	0$-\17	8

0$	;moveq	#13,D0		0
	cmp.b	($6E,A2),D0
	bne.b	\19
	movea.l	A2,A1
	bsr	lbC00A4F4
\19	move.b	d2,D4
	bra	\32

5$	;moveq	#13,D0		5
	cmp.b	($6E,A2),D0
	bne.b	\21
	movea.l	A2,A1
	bsr	lbC00A4F4
\21	moveq	#0,D0
	move.w	($5C,A2),D0
	moveq	#8,D5
	divsl.l	D5,D1:D0
	sub.l	D1,D5
	moveq	#0,D0
	move.w	($5C,A2),D0
	move.l	D5,D1
	add.l	D0,D1
	cmp.l	D6,D1
	ble.b	\23
	moveq	#0,D1
	move.w	D0,D1
	move.l	D6,D5
	sub.l	D1,D5
	bra.b	\23

\22	movea.l	($66,A2),A0
	addq.l	#1,($66,A2)
	move.b	#$20,(A0)
	addq.w	#1,($5C,A2)
	subq.l	#1,D5
\23	moveq	#1,D0
	cmp.l	D0,D5
	bgt.b	\22
	moveq	#$20,D4
	bra.b	\33

7$	tst.w	($5C,A2)	7
	bhi.b	\25
	tst.w	($72,A2)
	bne.b	.6
\25	moveq	#10,D4
	bra.b	\33

1$	;moveq	#13,D0		1
	cmp.b	($6E,A2),D0
	bne.b	\27
	movea.l	A2,A1
	bsr	lbC00A4F4
\27	movea.l	($66,A2),A0
	lea	(1,A0),A1
	move.l	A1,($6A,A2)
	move.b	#$9B,(A0)
	bra.b	.6

2$	;moveq	#13,D0		2
	move.b	($6E,A2),d1
	cmp.b	d1,D0
	bne.b	\29
	movea.l	A2,A1
	bsr	lbC00A4F4
	bra.b	4$

\29	moveq	#$1B,D0
	cmp.b	d1,D0
	beq	\27
	move.b	d2,D4
	bra.b	\32

;6$	;illegal
;	bra	.6

3$	;moveq	#13,D0		3
	cmp.b	($6E,A2),D0
	bne.b	.6
	movea.l	A2,A1
	bsr	lbC00A4F4
.6	moveq	#0,D4		6
4$	tst.b	D4		4
\32	beq	\36
\33	moveq	#$1B,D5
	cmp.b	($6E,A2),D5
	bne	\31
	cmpi.b	#'c',d4
	bne	\36
	moveq	#1,d0
	swap	d0
	lea	($74,A2),a0
	move.l	d0,(A0)		reset SGR
	move.l	d0,(_sgr,a4)
	clr.b	(_asdss,a4)
	lea	($7C,A2),a1
	bsr	sgr2ascii
	move.w	D0,($78,A2)
	movea.l	($66,a2),a1
	add.l	D0,($66,a2)
	subq.w	#1,d0
	lea	($7C,A2),a0
.c	move.b	(a0)+,(a1)+
	dbf	d0,.c
;	move.w	#1,($70,A2)
	st	($71,a2)
	bra	\36

\31	moveq	#10,D1
	cmp.b	D1,D4
	sne	D5
	neg.b	D5
	beq.b	\35
	movea.l	($66,A2),A0
	addq.l	#1,($66,A2)
	move.b	D4,(A0)
	addq.w	#1,($5C,A2)
	moveq	#0,D0
	move.w	($5C,A2),D0
	cmp.l	D6,D0
	bgt.b	\35
	move.l	($62,A2),D0
	moveq	#$1F,D1
	sub.l	D1,D0
	move.l	($66,A2),D1
	cmp.l	D0,D1
	blt.b	\36
\35	move.w	D5,($72,A2)
	bsr	lbC00B2E6
\36	move.b	d2,($6E,A2)
\37	subq.l	#1,D7
	bpl	\loop
	addq.l	#4,sp
	tst.b	d3
	beq	\38
	clr.w	($5C,A2)
	lea	($66,a2),a2
	move.l	(sp)+,(a2)+	restore $66+a2
	clr.l	(a2)+		clear $6a+a2
	clr.b	(a2)		clear $6e+a2
\38	movem.l	(SP)+,D2-D7/A2/A3/A5/a6
	rts


lbC00B6B2	movem.l	A5/A6,-(SP)
	movea.l	d0,A5
	moveq	#13,D0
	cmp.b	($6E,A5),D0
	bne.b	\1
;	movea.l	(12,SP),A0
	movea.l	A5,A1
	bsr	lbC00A4F4
\1	move.w	($5C,A5),($9E,A5)
	move.l	($66,A5),($A0,A5)
	move.b	($6E,A5),($A8,A5)
	lea	($70,A5),A0
	lea	($AA,A5),A1
	move.l	(a0)+,(a1)+
	moveq	#0,D0
	move.l	(A0)+,(A1)+
	move.w	(A0),d0
	move.w	D0,(A1)+
	addq.l	#4,a0
	movea.l	(_exec,A4),A6
	sys	CopyMem
	lea	($D6,A5),A0
	move.l	($66,A5),D0
	sub.l	A0,D0
	movea.l	($D2,A5),A1
	sys	CopyMem
	movem.l	(SP)+,A5/A6
	rts

lbC00B734	movem.l	A5/A6,-(SP)
	movea.l	d0,A5
	move.w	($9E,A5),($5C,A5)
	move.l	($A0,A5),($66,A5)
	move.b	($A8,A5),($6E,A5)
	lea	($AA,A5),A0
	lea	($70,A5),A1
	move.l	(a0)+,(a1)+
	moveq	#0,D0
	move.l	(A0)+,(A1)+
	move.w	(A0)+,d0
	move.w	D0,(A1)
	addq.l	#4,a1
	movea.l	(_exec,A4),A6
	sys	CopyMem
	lea	($D6,A5),A0
	move.l	($66,A5),D0
	move.l	A0,D1
	sub.l	D1,D0
	movea.l	($D2,A5),A0
	movea.l	D1,A1
	sys	CopyMem
	movem.l	(SP)+,A5/A6
	rts

lbC00B7DE	move.l	($90,A3),d0
	beq	\6
	suba.w	#44,SP
	movem.l	D4-D7/A2/A3/A5,-(SP)
;	movea.l	($50,SP),A3
	movea.l	d0,a2
;	move.l	($4c,SP),a0
	movea.l	a5,a0
	move.l	($5C,SP),D7
	moveq	#0,D6
	movea.l	($58,SP),A5
	move.l	($2C,A2),($9A,A2)
	bsr	lbC00B6B2
	move.l	($32,A2),D6
	movea.l	($54,SP),A0
	cmpa.l	A0,A5
	bls.b	\1
	move.l	A5,D0
	sub.l	($54,SP),D0
;	move.l	D0,-(SP)
;	move.l	A0,-(SP)
	move.l	A3,d1
	move.l	($58-12,SP),a1
	bsr	lbC00B3A6
\1	lea	($75,A2),A0
	move.b	(A0),D4
	move.b	d4,d0
	moveq	#3,d1
	not.b	D0
	and.b	D1,d0
	move.b	D0,(A0)+
	move.b	(A0),D0
	move.b	D0,(47,sp)
	not.b	D0
	and.b	D1,d0
	move.b	D0,(A0)
	subq.l	#2,a0
;	lea	($74,A2),a0
	lea	(28,SP),a1
	bsr	sgr2ascii
	movea.l	($54,SP),A3
	move.l	D0,D5
	addq.l	#1,D5
	adda.l	D7,A3
	cmpa.l	A3,A5
	bcc.b	\2
;	moveq	#0,D1
	move.b	(A5),D1
	bra.b	\3

\2	moveq	#$20,D1
\3	move.b	D1,(28,SP,D0.L)
	lea	($76,A2),a0
	move.b	(47,SP),(A0)
	move.b	D4,-(A0)
	subq.l	#1,a0
	lea	(28,SP,d5.l),A1
;	adda.l	D5,A1
	bsr	sgr2ascii
	add.l	D5,d0
	lea	(32-4,SP),a0
	move.l	($58-8,SP),d1
	move.l	($58-12,SP),a1
	bsr	lbC00B3A6
	subq.l	#1,A3
	cmpa.l	A3,A5
	bcc.b	\4
	move.l	($54,SP),D0
	sub.l	A5,D0
	add.l	D7,D0
	subq.l	#1,D0
;	move.l	D0,-(SP)
	lea	(1,A5),a0
	move.l	($58-8,SP),d1
	move.l	($58-12,SP),a1
	bsr	lbC00B3A6
\4	move.l	($32,A2),D0
	sub.l	D6,D0
	move.l	D0,D6
	bgt	\5
	moveq	#0,D0
\5	movem.l	(SP)+,D4-D7/A2/A3/A5
	adda.w	#44,SP
\6	rtd	#20

lbC00B8F2	move.l	($90,A0),d0
	beq.b	\7
	movem.l	D7/A2/A3/A5,-(SP)
	movea.l	a1,A5
	moveq	#0,D7
	move.l	D0,a3
	tst.l	($9A,A3)
	beq.b	\6
	movea.l	($2C,A3),A2
	bra.b	\3

\1	cmpa.l	($56,A3),A2
	bne.b	\2
	clr.l	($56,A3)
\2	movea.l	(4,A2),A2
	movea.l	($4A,A3),A0
	pea	($24,A3)
	move.l	A5,-(SP)
	jsr	(A0)
	addq.l	#1,D7
	addq.l	#8,SP
\3	tst.l	(4,A2)
	beq.b	\4
	cmpa.l	($9A,A3),A2
	bne.b	\1
\4	tst.l	($56,A3)
	bne.b	\5
	lea	($28,A3),A0
	move.l	A0,($56,A3)
	move.l	($32,A3),D0
	move.w	D0,($5A,A3)
\5	clr.l	($9A,A3)
	move.l	A3,d0
	bsr	lbC00B734
\6	move.l	D7,D0
	movem.l	(SP)+,D7/A2/A3/A5
\7	rts

lbC00B96A	move.l	($90,A5),d0
	beq	\9
	movea.l	d0,a1
	move.l	($56,A1),d0
	beq	\9
	suba.w	#$28,SP
	movem.l	D2/D6/A2/A3/A5,-(SP)
	movea.l	a5,A3
	movea.l	a1,a2
	movea.l	a0,A5
	tst.l	($78,A3)
	bne.b	\3
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00B8F2
	bra	.7

\3	movea.l	([$98,A3],$18),A1
	moveq	#1,d6
	add.w	($2C,A1),D6
	move.l	D6,D0
	move.l	($32,A2),D1
	move.l	D1,D2
	addq.l	#1,D2
	cmp.l	D0,D2
	bge.b	\4
	moveq	#0,D0
	bra.b	\5

\4	sub.l	D6,D1
	addq.l	#1,D1
	move.l	D1,D0
\5	pea	(1).W
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00B8F2
	moveq	#0,D0
	move.w	($5A,A2),D0
	move.l	($32,A2),D1
	sub.l	D0,D1
	addq.l	#1,D1
	lea	($1c-8,SP),a0
	bsr	lbC009B1E
	move.l	D0,D2
	move.w	($5C,A2),D0
	moveq	#0,D1
	cmp.w	D1,D0
	bls.b	\6
	lea	($14,SP),A0
	moveq	#0,D1
	move.w	D0,D1
	adda.l	D2,A0
	bsr	lbC0099D6
	add.l	D0,D2
\6	lea	(20,SP),A0
	adda.l	D2,A0
	bsr	lbC009A4A
	add.l	D0,D2
	lea	($14,SP),A0
;	pea	(1).W
	moveq	#1,d1
	adda.l	D2,A0
	bsr	lbC009B7E
	add.l	D0,D2
	lea	($14,SP),A0
	moveq	#1,d1
	adda.l	D2,A0
	bsr	lbC009A14
	add.l	D0,D2
	move.l	D2,d0
	lea	($18-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
.7	clr.l	($56,A2)
	move.w	#$FFFF,($5A,A2)
\7	moveq	#1,D0
\8	movem.l	(SP)+,D2/D6/A2/A3/A5
	adda.w	#$28,SP
\9	rts

lbC00BA92	movem.l	A2/A3/A5/A6,-(SP)
	movea.l	(8,A5),A6
	move.l	($90,a3),d0
	beq	\3
	movea.l	D0,a5
	tst.l	(8,a5)
	beq	\3
	tst.l	($78,a3)
	beq.b	\3
	suba.l	A2,A2
	movea.l	($78,a3),a3
	tst.b	d1
	beq.b	\1
	movea.l	(8,a5),A0
	movea.l	a3,A1
	sys	OnGadget
	movea.l	($14,a5),A0
	movea.l	a3,A1
	sys	OnGadget
	movea.l	($18,a5),A0
	movea.l	a3,A1
	sys	OnGadget
	bra.b	\3

\1	movea.l	(8,a5),A0
	movea.l	a3,A1
	sys	OffGadget
	movea.l	($14,a5),A0
	movea.l	a3,A1
	sys	OffGadget
	movea.l	($18,a5),A0
	movea.l	a3,A1
	sys	OffGadget
\3	movem.l	(SP)+,A2/A3/A5/A6
	rts

lbC00BB30	suba.w	#$14,SP
	movem.l	D5-D7/A2/A3/A5,-(SP)
;	movea.l	($34,SP),A3
	movea.l	($98,A3),A0
	moveq	#0,D7
	move.w	([$18,A0],$2C),D5
;	movea.l	($38,SP),A2
;	movea.l	($30,SP),A5
	addq.w	#1,D5
	move.l	($2C,A2),($56,A2)
	move.l	($32,A2),D0
	subq.l	#1,D0
	move.w	D0,($5A,A2)
	move.l	($116,A3),D0
	tst.w	($11E,A3)
	bne.b	\1
	tst.l	d0
	bne.b	\2
\1	move.l	d0,-(SP)
	move.l	($11A,A3),-(SP)
	pea	($9FC,A3)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00B7DE
	move.l	D0,D7
\2	moveq	#0,D0
	move.w	D5,D0
	subq.l	#2,D0
	moveq	#0,D1
	move.w	($5A,A2),D1
	cmp.l	D0,D1
	blt.b	\3
	sub.w	D5,D1
	addq.w	#2,D1
	moveq	#0,D0
	move.w	D1,D0
	clr.l	-(SP)
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
	bra.b	\4

\3	moveq	#0,D0
	move.l	D0,-(SP)
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
\4	move.w	#1,($FE,A3)
	moveq	#0,d1
	lea	($18,SP),a0
	bsr	lbC009A14
	lea	($18,SP),A0
	adda.l	D0,A0
	move.l	D0,D6
;	clr.l	(SP)
	moveq	#0,d1
	bsr	lbC009B7E
	add.l	D0,D6
	move.l	D6,d0
	lea	(28-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	move.l	D7,D0
	movem.l	(SP)+,D5-D7/A2/A3/A5
	adda.w	#$14,SP
	rts

lbC00BC02	move.l	($90,A5),d1
	beq	\13
	movem.l	D2/D4-D7/A2/A3/A5,-(SP)
	movea.l	a5,A3
	movea.l	a2,a5
	movea.l	d1,A2
	move.b	d0,D7
	movea.l	([$98,A3],$18),A1
	move.w	($2C,A1),D5
	addq.w	#1,D5
	moveq	#0,D0
	move.w	D5,D0
	move.l	($32,A2),D1
	cmp.l	D0,D1
	blt	\12
	tst.l	($56,A2)
	bne.b	\1
	moveq	#$2A,D0
	cmp.b	D0,D7
	beq	\12
	moveq	#$39,D0
	cmp.b	D0,D7
	beq	\12
	moveq	#$17,D0
	cmp.b	D0,D7
	beq	\12
	bsr	lbC00BB30
\1	move.w	($5A,A2),D4
	moveq	#0,D0
	move.b	D7,D0
	moveq	#$12,D1
	sub.l	D1,D0
	beq	\7
	subq.l	#5,D0
	beq	\8
	moveq	#$13,D1
	sub.l	D1,D0
	beq.b	\4
	moveq	#15,D1
	sub.l	D1,D0
	beq	\10
	moveq	#$2A,D1
	sub.l	D1,D0
	beq	\9
	subq.l	#1,D0
	bne	\11
	move.w	($5A,A2),D0
	moveq	#0,D1
	cmp.w	D1,D0
	bls	\11
	moveq	#0,D1
	move.w	D5,D1
	subq.l	#1,D1
	moveq	#0,D2
	move.w	D0,D2
	cmp.l	D1,D2
	bge.b	\2
	moveq	#0,D1
	bra.b	\3

\2	moveq	#0,D1
	move.w	D5,D1
	moveq	#0,D2
	move.w	D0,D2
	sub.l	D1,D2
	addq.l	#1,D2
	move.l	D2,D1
\3	move.l	D1,D4
	bra	\11

\4	moveq	#0,D0
	move.w	D5,D0
	move.l	($32,A2),D1
	move.l	D1,D6
	sub.l	D0,D6
	addq.l	#1,D6
	moveq	#0,D0
	move.w	($5A,A2),D0
	cmp.l	D6,D0
	bge.b	\11
	swap	D5
	clr.w	D5
	swap	D5
	move.l	D5,D2
	add.l	D2,D2
	sub.l	D2,D1
	moveq	#0,D2
	move.w	D0,D2
	cmp.l	D1,D2
	ble.b	\5
	move.l	D6,D1
	bra.b	\6

\5	moveq	#0,D1
	move.w	D5,D1
	moveq	#0,D2
	move.w	D0,D2
	add.l	D1,D2
	subq.l	#1,D2
	move.l	D2,D1
\6	move.l	D1,D4
	bra.b	\11

\7	moveq	#0,D4
	bra.b	\11

\8	moveq	#0,D0
	move.w	D5,D0
	move.l	($32,A2),D4
	sub.l	D0,D4
	addq.l	#1,D4
	bra.b	\11

\9	move.w	($5A,A2),D0
	moveq	#0,D1
	cmp.w	D1,D0
	bls.b	\11
	move.w	D0,D4
	subq.w	#1,D4
	bra.b	\11

\10	moveq	#0,D0
	move.w	D5,D0
	move.l	($32,A2),D1
	sub.l	D0,D1
	addq.l	#1,D1
	moveq	#0,D0
	move.w	($5A,A2),D0
	cmp.l	D1,D0
	bge.b	\11
	move.w	D0,D4
	addq.w	#1,D4
\11	move.w	($5A,A2),D0
	cmp.w	D0,D4
	beq.b	\12
	moveq	#0,D2
	move.w	D0,D2
	move.w	D4,($5A,A2)
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
	move.w	D2,($5A,A2)
	pea	(1).W
	moveq	#0,D1
	move.w	D4,D1
	move.l	D1,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
\12	movem.l	(SP)+,D2/D4-D7/A2/A3/A5
\13	rts

lbC00BD7E	move.l	($90,a0),d0
	beq.b	\1
	movea.l	d0,A0
	move.l	($56,A0),d0
\1	rts

lbC00BD98	movem.l	D4-D7/A2/A3/A5,-(SP)
	movea.l	([$20,SP],$90),A5
	move.w	($2A,SP),D7
	moveq	#-1,D5
	moveq	#-1,D4
	move.l	#$FFFFD8F0,D6
	move.l	A5,D0
	beq	lbC00BE5C
	tst.l	($56,A5)
	beq	lbC00BE5C
	tst.l	($9A,A5)
	beq	lbC00BE5C
	movea.l	($66,A5),A0
	movea.l	($24,A5),A3
	clr.b	(A0)
	bra	lbC00BE56

lbC00BDD6	movea.l	($56,A5),A0
	cmpa.l	A3,A0
	bne.b	lbC00BDE0
	moveq	#0,D5
lbC00BDE0	movea.l	($9A,A5),A0
	cmpa.l	(A0),A3
	bne.b	lbC00BDF4
	moveq	#0,D4
	moveq	#0,D6
	move.w	($9E,A5),D6
	neg.l	D6
lbC00BDF4	tst.w	D4
	bmi.b	lbC00BE4A
	tst.l	(A3)
	beq.b	lbC00BE02
	movea.l	(10,A3),A0
	bra.b	lbC00BE06

lbC00BE02	lea	($D6,A5),A0
lbC00BE06	movea.l	A0,A2
	bra.b	lbC00BE30

lbC00BE0A	cmpi.b	#$9B,(A2)
	bne.b	lbC00BE1E
	movea.l	A2,A0
	bsr	lbC00936C
	movea.l	D0,A0
	lea	(1,A0),A2
	bra.b	lbC00BE30

lbC00BE1E	move.w	($26,SP),D0
	cmp.w	D4,D0
	bne.b	lbC00BE2A
	cmp.w	D7,D5
	beq.b	lbC00BE5C
lbC00BE2A	addq.l	#1,A2
	addq.l	#1,D6
	addq.w	#1,D4
lbC00BE30	move.b	(A2),D0
	moveq	#10,D1
	cmp.b	D1,D0
	beq.b	lbC00BE3C
	tst.b	D0
	bne.b	lbC00BE0A
lbC00BE3C	moveq	#10,D0
	cmp.b	(A2),D0
	bne.b	lbC00BE54
	moveq	#0,D4
	tst.w	D5
	bmi.b	lbC00BE54
	bra.b	lbC00BE52

lbC00BE4A	cmp.w	D5,D7
	beq.b	lbC00BE5C
	tst.w	D5
	bmi.b	lbC00BE54
lbC00BE52	addq.w	#1,D5
lbC00BE54	movea.l	(A3),A3
lbC00BE56	move.l	A3,D0
	bne	lbC00BDD6
lbC00BE5C	move.l	D6,D0
	movem.l	(SP)+,D4-D7/A2/A3/A5
	rtd	#12

lbC00BE64	suba.w	#$18,SP
	movem.l	D7/A2/A3/A5,-(SP)
	movea.l	($90,A3),A2
	move.l	A2,D0
	beq	lbC00BF86
	move.l	A3,a0
	move.l	($56,A2),(16,SP)
	move.l	A5,a1
	bsr	lbC00B8F2
	tst.l	($10,SP)
	bne.b	lbC00BEBA
	move.l	($32,A2),D0
	bgt.b	lbC00BEA2
	tst.w	($5C,A2)
	bls.b	lbC00BEBA
lbC00BEA2	bsr	lbC00BB30
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00B8F2
	bra.b	lbC00BEEE

lbC00BEBA	move.w	#1,($FE,A3)
	moveq	#0,d1
	lea	(20,SP),a0
	bsr	lbC009A14
	lea	(20,SP),A0
	adda.l	D0,A0
	move.l	D0,D7
;	clr.l	(SP)
	moveq	#0,d1
	bsr	lbC009B7E
	add.l	D0,D7
	move.l	D7,d0
	lea	($18-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
lbC00BEEE	tst.l	($10,SP)
	beq.b	lbC00BF0C
	move.l	($116,A3),-(SP)
	move.l	($11A,A3),-(SP)
	pea	($9FC,A3)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00B7DE
lbC00BF0C	move.w	($5A,A2),D0
	cmpi.w	#$FFFF,D0
	beq.b	lbC00BF2C
	pea	(1).W
	moveq	#0,D1
	move.w	D0,D1
	move.l	D1,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
lbC00BF2C	tst.l	($10,SP)
	bne.b	lbC00BF7C
	lea	($14,SP),a0
	bsr	lbC009A4A
	lea	($14,SP),A0
;	pea	(1).W
	moveq	#1,d1
	adda.l	D0,A0
	move.l	D0,D7
	bsr	lbC009B7E
	add.l	D0,D7
	lea	($14,SP),A0
	moveq	#1,d1
	adda.l	D7,A0
	bsr	lbC009A14
	add.l	D0,D7
	move.l	D7,d0
	lea	($18-4,SP),a0
	move.l	A3,a1
	move.l	A5,d1
	bsr	_doio
	clr.l	($56,A2)
	move.w	#$FFFF,($5A,A2)
lbC00BF7C	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
lbC00BF86	movem.l	(SP)+,D7/A2/A3/A5
	adda.w	#$18,SP
	rts

lbC00BF90	movem.l	D4/D5/D7/A2/A3/A5/A6,-(SP)	check/process window messages
	subq.l	#8,SP
	movea.l	a0,a3
	movea.l	a1,A5
	move.l	($78,A3),d0	window
	beq	\12
	tst.l	($168,A3)
	bne	\12
	movea.l	D0,a0
	tst.l	(wd_UserPort,A0)
	beq	\12
	movea.l	([$98,A3],io_Unit),A1
	move.w	(cu_YMax,A1),D5
	addq.w	#1,D5
	bra	.11

\1	movea.l	d0,a2
	move.l	(im_Class,A2),D7
	move.l	D7,D0
	moveq	#IDCMP_GADGETUP,D1
	cmp.l	D1,D0
	beq.b	\2
	moveq	#0,D4
	cmpi.l	#IDCMP_IDCMPUPDATE,D0
	bne.b	\4
	movea.l	(im_IAddress,A2),A0
	movea.l	($1C,A5),A6
	move.l	#GA_ID,D0
	moveq	#0,D1
	sys	GetTagData
	move.l	D0,D4
	bra.b	\4

\2	move.w	([im_IAddress,A2],gg_GadgetID),D4

\4	movea.l	A2,A1
	movea.l	(A5),A6
	sys	ReplyMsg
	move.w	D4,D0
	moveq	#$64,D1
	sub.w	D1,D0
	beq.b	\5
	subq.w	#1,D0
	beq	\10
	bra	\11

\5	movea.l	($90,A3),A2
	move.l	A2,D0
	beq	\11
	movea.l	(4,A2),A0
	lea	(4,SP),A1
	movea.l	(8,A5),A6
	move.l	#PGA_Top,D0
	sys	GetAttr
	movea.l	(4,A2),A0
	movea.l	SP,A1
	move.l	#PGA_Total,D0
	sys	GetAttr
	tst.w	($5C,A2)
	shi	D0
	moveq	#0,D1
	sub.b	D0,D1
	move.l	($32,A2),D7
	add.l	D1,D7
	move.l	(4,SP),D0
	ble.b	\7
	add.l	D7,D0
	sub.l	(SP),D0
	bpl.b	\6
	moveq	#0,D0
\6	move.l	D0,(4,SP)
\7	moveq	#0,D1
	move.w	($5A,A2),D1
	cmp.l	D0,D1
	beq	\11
;	moveq	#0,D1
	move.w	D5,D1
	cmp.l	D1,D7
	blt.b	\11
	move.l	($56,A2),D1
	bne.b	\8
	moveq	#0,D1
	move.w	D5,D1
	add.l	D1,D0
	cmp.l	D7,D0
	beq.b	\11
\8	tst.l	($56,A2)
	bne.b	\9
	bsr	lbC00BB30
	add.l	D0,(4,SP)
	move.l	(4,SP),D0
	pea	(1).W
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00AE7A
	bra.b	\11

\9	move.l	(4,SP),D0
	pea	(1).W
	move.l	D0,-(SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC00AB70
	bra.b	\11

\10	moveq	#IDCMP_GADGETUP,D0
	cmp.l	D0,D7
	bne.b	\11
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00D9B6
	move.w	D0,($104,A3)
\11	movea.l	($78,A3),a0	window
.11	movea.l	(wd_UserPort,A0),A0
	movea.l	(A5),A6
	sys	GetMsg
	tst.l	D0
	bne	\1
\12	addq.l	#8,SP
	movem.l	(SP)+,D4/D5/D7/A2/A3/A5/A6
	rts

_donmk	btst	#2,d4		shortcuts enabled or disabled?
	bne	_donm
	move.l	a0,d1
	subq.l	#1,d1
	bra	_donm2

_donm	moveq	#0,d1
_donm2	move.b	d0,(a2)+	nm_Type
	clr.b	(a2)+

	move.l	d0,-(sp)
	move.l	a0,d0
	beq	\skip
	addq.l	#1,d0
	beq	\skip		barlabel
	move.l	(_cat,a4),d0
	beq	\skip
	movea.l	d0,a6
	movea.l	a0,a1
	subq.l	#1,a0
	tst.l	d1
	beq	.nokey
	subq.l	#1,a0
.nokey	moveq	#0,d0
	move.b	(a0),d0		string number
	movem.l	d1/a1,-(sp)
	movea.l	a6,a0
	movea.l	(_locb,a4),a6	locale base
	sys	GetCatalogStr
	movem.l	(sp)+,d1/a0
	cmpa.l	d0,a0
	beq	\skip		default string is used
	movea.l	d0,a0
	moveq	#NM_TITLE,d1
	sub.l	(sp),d1
	beq	\skip
	move.w	(a0)+,d0
	moveq	#32,d1		space
	lsr.l	#8,d0
	sub.b	d0,d1
	beq	\skip
	move.l	a0,d1
	subq.l	#2,d1
\skip	move.l	(sp)+,d0
	move.l	a0,(a2)+	nm_Label
	move.l	d1,(a2)+	nm_CommandKey
	move.w	d2,(a2)+	nm_Flags
	clr.l	(a2)+
	clr.l	(a2)+
	rts

number_of_menus=38
stack=(number_of_menus+1)*gnm_SIZEOF

lbC00C12E	suba.w	#stack,SP
	movem.l	D2-D5/A2/A3/A5/A6,-(SP)
	movem.l	($160,a3),d3-d5

	lea	(8*4,sp),a2

	lea	(Console.MSG,pc),a0
	moveq	#NM_TITLE,d0	menu title
	moveq	#0,d2		flags
	bsr	_donm

	moveq	#NM_ITEM,D0	textual menu item
	lea	(Clearwindow.msg,pc),a0
	bsr	_donmk

	lea	(Reset.MSG,pc),A0
	bsr	_donm

	lea	(Jumpscroll.MSG,pc),A0
	btst	#18,d3		jump scroll has been set?
	beq	lbC00C1AC
	moveq	#$40,D2
	lsl.l	#2,D2		set CHECKED if enabled
lbC00C1AC	ori.w	#CHECKIT!MENUTOGGLE,D2
	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	moveq	#0,d2
	bsr	_donm

	lea	(Minimize.MSG,pc),A1
	tst.l	d5
	bne	lbC00C1E8
	move.l	(20,a5),d1	wb.lib has been opened?
	beq	lbC00C1E8
	btst	#24,d3		NOICONIFY set?
	beq	lbC00C1EE
	btst	#30,d3		MENUFY set?
	bne	lbC00C1EE
lbC00C1E8	bset	d2,($160,a3)	=bset #24,d3 (NOICONIFY)
	movea.l	a1,A0
	bra.b	lbC00C1F2

lbC00C1EE	lea	(Iconify.MSG,pc),A0
lbC00C1F2	btst	#5,d3
	beq	lbC00C21A
	cmpa.l	a1,A0
	bne	lbC00C21A
	moveq	#ITEMENABLED,D2
lbC00C21A	bsr	_donmk

	lea	(Normalize.MSG,pc),A0
;	moveq	#0,d2
	btst	#5,d3
	beq	lbC00C24A
	moveq	#ITEMENABLED,D2
lbC00C24A	bsr	_donmk

	lea	(Maximize.MSG,pc),A0
;	moveq	#0,d2
;	btst	#5,d3
;	beq	lbC00C27A
;	moveq	#ITEMENABLED,D2
lbC00C27A	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	moveq	#0,d2
	bsr	_donm

	lea	(Nextscreen.MSG,pc),A0
	tst.l	D5
	beq	lbC00C2BA
	moveq	#ITEMENABLED,d2
lbC00C2BA	bsr	_donmk

	lea	(Gotoscreen.MSG,pc),A0
;	moveq	#0,D2
;	tst.l	d5
;	beq	lbC00C2DA
;	moveq	#ITEMENABLED,D2
lbC00C2DA	bsr	_donm

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	moveq	#0,d2
	bsr	_donm

	lea	(Halt.MSG,pc),A0
	bsr	_donmk

	lea	(Resume.MSG,pc),A0
	bsr	_donmk

	lea	(_keep,pc),A0
	btst	#26,d3		KEEPCLOSED has been set?
	beq	.nkc
	moveq	#64,d2
	lsl.l	#2,d2		set CHECKED if enabled
.nkc	ori.w	#CHECKIT|MENUTOGGLE,d2
	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	moveq	#0,d2
	bsr	_donm

	lea	(About.MSG,pc),A0
	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	bsr	_donm

	lea	(Close.MSG,pc),A0
	bsr	_donmk

	lea	(Complete.MSG,pc),A0
	moveq	#NM_TITLE,d0		menu title
	bsr	_donm

	lea	(Filename.MSG,pc),A0
	moveq	#NM_ITEM,d0
	bsr	_donmk

	lea	(Command.MSG,pc),A0
	bsr	_donmk

	lea	(Device.MSG,pc),A0
	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	bsr	_donm

	lea	(Enablecache.MSG,pc),a0
	btst	#1,d4
	beq	lbC00C44C
	moveq	#$40,D2
	lsl.l	#2,D2		set CHECKED if enabled
lbC00C44C	ori.w	#CHECKIT|MENUTOGGLE,D2
	btst	d1,d4		bit #0
	bne	lbC00C460
	ori.w	#ITEMENABLED,D2
lbC00C460	bsr	_donm

	lea	(Resetcache.MSG,pc),a0
	moveq	#0,d2
	btst	d2,d4		bit #0
	bne	lbC00C486
	moveq	#ITEMENABLED,D2
lbC00C486	bsr	_donm

	lea	(Purgecache.MSG,pc),a0
;	moveq	#0,D2
;	btst	d2,d4
;	bne	lbC00C4AA
;	moveq	#ITEMENABLED,D2
lbC00C4AA	bsr	_donm

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	moveq	#0,d2
	bsr	_donm

	lea	(Showinfo.MSG,pc),A0
	btst	#14,d3
	bne.b	lbC00C4EE
	moveq	#$40,D2
	lsl.l	#2,D2		set CHECKED if enabled
lbC00C4EE	ori.w	#CHECKIT|MENUTOGGLE,D2
	bsr	_donmk

	moveq	#NM_TITLE,d0		menu title
	moveq	#0,d2
	lea	(Review.MSG,pc),a0
	btst	#10,d3			NOREVIEW?
	sne	D1
	sub.b	D1,D2
	bsr	_donm

	moveq	#NM_ITEM,d0
	lea	(Enabled.MSG,pc),A0
	moveq	#0,D2
	btst	#11,d3
	bne	lbC00C544
	moveq	#$40,D2
	lsl.l	#2,D2		set CHECKED if enabled
lbC00C544	ori.w	#CHECKIT|MENUTOGGLE,D2
	bsr	_donmk

	lea	(Clearbuffer.MSG,pc),A0
	moveq	#0,d2
	bsr	_donmk

	moveq	#-1,d1		barlabel
	movea.l	d1,a0
	bsr	_donm

	lea	(Saveplaintext.MSG,pc),A0
	bsr	_donm

	lea	(Savewithstyle.MSG,pc),A0
	btst	#16,d3
	beq	lbC00C5A6
	moveq	#ITEMENABLED,D2
lbC00C5A6	bsr	_donm

	lea	(History.MSG,pc),a0
	btst	#9,d3
	sne	d0
	moveq	#0,D2
	sub.b	D0,D2
	moveq	#NM_TITLE,d0
	bsr	_donm

	lea	(Enabled.MSG2,pc),A0
	moveq	#NM_ITEM,d0
	moveq	#0,D2
	tst.w	d3		#15
	bmi	lbC00C5EC
	moveq	#$40,D2
	lsl.l	#2,D2		set CHECKED if enabled
lbC00C5EC	ori.w	#CHECKIT|MENUTOGGLE,D2
	bsr	_donm

	lea	(Clearbuffer.MSG2,pc),A0
	moveq	#0,d2
	bsr	_donm

	suba.l	a0,a0
	moveq	#0,d0
	bsr	_donm

	lea	(8*4,SP),A0
	movea.l	(12,A5),A6
	suba.l	a1,A1
	sys	CreateMenusA
	movea.l	D0,A2
	tst.l	D0
	beq	lbC00C686
	tst.l	d5
	seq	D0
	moveq	#0,D1
	sub.b	D0,D1
	move.l	A2,-(SP)
	clr.l	-(SP)
	move.l	D1,-(SP)
	move.l	#GTMN_NewLookMenus,-(SP)
	movea.l	A2,A0
	movea.l	($80,A3),A1
	movea.l	SP,A2
	sys	LayoutMenusA
	lea	(12,SP),SP
	movea.l	(SP)+,A2
	tst.l	D0
	beq	lbC00C67C
	movea.l	($78,A3),A0
	movea.l	A2,A1
	movea.l	(8,A5),A6
	sys	SetMenuStrip
	tst.l	D0
	beq	lbC00C67C
	move.l	A2,($A4,A3)
	move.l	A2,D0
	bra	lbC00C688

lbC00C67C	movea.l	A2,A0
	movea.l	(12,A5),A6
	sys	FreeMenus
lbC00C686	moveq	#0,D0
lbC00C688	movem.l	(SP)+,D2-D5/A2/A3/A5/A6
	adda.w	#stack,SP
	rts

lbC00C692	movem.l	d2-d5/A2/A3/A6,-(SP)
	movea.l	(8,a1),a6	intuition
	movea.l	a0,A3
	move.l	($A4,A3),d0	menu strip
	beq	.out
	movea.l	D0,A2
	movea.l	($78,A3),A0	our window
	cmpa.l	($1C,A0),A2	our window's menu strip?
	bne	.out
	lea	($160,a3),a3
	move.l	(a3),d2		get option bits
	move.l	#$f802,d4	'Review/Enabled' ($f802)
	moveq	#11,d3
	moveq	#1,d5
	bsr	.cmi
	addq.l	#1,d4		'History/Enabled' ($f803)
	moveq	#15,d3
	bsr	.cmi
	moveq	#61,d0
	add.l	d0,d4		'Console/Jump scroll' ($f840)
	moveq	#18,d3
	moveq	#0,d5
	bsr	.cmi
	move.l	d2,(a3)+
	move.l	(a3),d2
	moveq	#65,d0
	add.l	d0,d4		'Complete/Enable cache' ($f881)
	moveq	#1,d3
	bsr	.cmi
        move.l	d2,(a3)
	move.l	-(a3),d2
	move.w	#$F901,d4	'Complete/Show .info' ($f901)
	moveq	#14,d3
	moveq	#1,d5
	bsr	.cmi
	move.w	#$F9a0,d4	'Complete/Keep closed' ($f9a0)
	moveq	#26,d3
	moveq	#0,d5
	bsr	.cmi
	move.l	d2,(a3)
.out	movem.l	(SP)+,d2-d5/A2/A3/A6
	rts

.cmi	movea.l	a2,a0
	move.l	d4,d0
	sys	ItemAddress
	movea.l	d0,a0
	move.b	(mi_Flags,A0),d0
	tst.b	d5
	bne	\siz
\ciz	btst	d5,d0
	beq	.clr		clear bit if zero
.set	bset	d3,d2
	rts

\siz	btst	#0,d0
	beq	.set		set bit if zero
.clr	bclr	d3,d2
	rts

lbC00C786	movem.l	a2/A3/A5/A6,-(SP)	ghost/unghost menus
	move.l	($A4,A3),d0
	beq	\4
	movea.l	d0,a2
	movea.l	($78,a3),A0
	cmpa.l	($1C,A0),a2
	bne	\4
	movea.l	(A5),A6
	sys	Forbid
	movea.l	($78,a3),A0
	movea.l	(8,A5),A6
	move.l	d1,-(sp)
	sys	ClearMenuStrip
	move.l	(SP)+,d1
	beq.b	\2
	moveq	#0,d0
	bset	d0,(13,a2)
	movea.l	(a2),A0
	bset	d0,(13,A0)
	movea.l	(A0),A0
	btst	#2,($162,a3)	NOREVIEW?
	bne.b	\1
	bset	d0,(13,A0)
\1	movea.l	(A0),A0
	btst	d1,($162,a3)	#1
	bne.b	\3
	bset	d0,(13,A0)
	bra.b	\3

\2	moveq	#3,d0
	movea.l	a2,a0
.1	bclr	d1,(13,a0)
	movea.l	(a0),a0
	dbf	d0,.1
\3	movea.l	($78,a3),A0
	movea.l	a2,A1
	movea.l	(8,A5),A6
	sys	ResetMenuStrip
	movea.l	(a5),A6
	sys	Permit
\4	movem.l	(SP)+,a2/A3/A5/A6
	rts

_getstr	move.l	(_cat,a4),d0
	beq	\ret
	movea.l	d0,a0
	moveq	#0,d0
	move.b	(-1,a1),d0
	move.l	a6,-(sp)
	movea.l	(_locb,a4),a6	locale base
	sys	GetCatalogStr
	movea.l	(sp)+,a6
	movea.l	d0,a1
\ret	rts

lbC00C830	suba.w	#20,SP
	movem.l	A2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	moveq	#EasyStruct_SIZEOF,D0
	lea	(16,SP),A2
	move.l	D0,(A2)+
	clr.l	(A2)+
	lea	(about,pc),a1
	bsr	_getstr
	move.l	a1,(a2)+
	lea	(_about,pc),A1
	move.l	A1,(A2)+
	lea	(OK.MSG,pc),a1
	bsr	_getstr
	move.l	A1,(A2)
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00170E
	move.l	A3,-(SP)
	lea	(status,pc),a1
	bsr	_getstr
	pea	(a1)
	lea	(fixes,pc),a1
	bsr	_getstr
	pea	(a1)
	movea.l	($78,A3),A0
	suba.l	A2,A2
	movea.l	sp,a3
	lea	(28,SP),A1
	movea.l	(8,A5),A6
	sys	EasyRequestArgs
	addq.l	#8,sp
	movea.l	(SP)+,A0
	move.l	A5,a1
	bsr	lbC001746
	movem.l	(SP)+,A2/A3/A5/A6
	adda.w	#20,SP
	rts

lbC00C8E8
	ifd	scn
	tst.l	(_scnm,a4)	scn message pending?
	bne	\np
.try	move.l	(_client,a4),d0	notify request has been allocated?
	beq	\np
	movea.l	(32,a5),a6	scn base
	movea.l	d0,a0
	sys	RemWorkbenchClient
	tst.l	d0
	bne	\ok
	movea.l	(4,a5),a6
	moveq	#10,d1
	sys	Delay
	bra	.try

\ok	clr.l	(_client,a4)
\np	endc

	move.l	($D0,A3),D0
	beq	.ret
	movea.l	D0,A0
	movea.l	(20,a5),A6	wblib
	sys	RemoveAppWindow
	clr.l	($D0,A3)
.ret	rts

lbC00C92E	suba.w	#$11E,SP
	movem.l	D2/D3/D5-D7/A2/A3/A5/A6,-(SP)
	move.l	($A4+178,SP),D7
;	move.b	#$20,($83+178,SP)
;	move.b	#$22,($82+178,SP)
	move.w	#$2220,($82+178,SP)
	move.l	D7,D6
	bra	\19

\1	movea.l	($94+178,SP),A3
	movea.l	($A0+178,SP),A5
	move.l	#256,d0
	lea	($31-4,SP),a1
	move.l	(A2),d1
	move.l	A3,a0
	bsr	_NameFromLock
	tst.w	D0
	beq	\16
	movea.l	(44,A3),A6		input.device
	sys	PeekQualifier
	moveq	#64,D3
	asl.l	#2,d3
	moveq	#0,D2
	move.w	D0,D2
	move.l	D2,D0
	moveq	#$30,D1
	and.l	D1,D0
	beq.b	\4
	movea.l	(4,A2),A0
	tst.b	(A0)
	bne.b	\3
	lea	($2D,SP),A0
	move.l	A0,D1
	movea.l	(4,A3),A6
	sys	FilePart
	movea.l	D0,A0
	tst.b	(A0)
	beq.b	\2
	movea.l	D0,a1
	move.l	d3,d0
	lea	($2d,SP),a0
	bsr	lbC00DD34
\2	lea	($2D,SP),A0
	move.l	A0,D1
	lea	(nullbyte2,PC),A0
	move.l	A0,D2
	sys	AddPart
	clr.w	($8C+178,SP)
	bra.b	\7

\3	move.w	#1,($8C+178,SP)
	move.l	d3,d0
	movea.l	A0,a1
	lea	($2d,SP),a0
	bsr	lbC00DD34
	bra.b	\7

\4	btst	#3,D2
	beq.b	\6
	movea.l	(4,A2),A0
	tst.b	(A0)
	bne.b	\5
	lea	($2D,SP),A0
	move.l	A0,D1
	movea.l	(4,A3),A6
	jsr	(-$36C,A6)
	movea.l	D0,A0
	clr.b	(A0)
\5	lea	($2D,SP),A0
	move.l	A0,D1
	lea	(nullbyte2,PC),A0
	move.l	A0,D2
	movea.l	(4,A3),A6
	sys	AddPart
	clr.w	($8C+178,SP)
	bra.b	\7

\6	movea.l	(4,A2),A0
	tst.b	(A0)
	sne	D0
	neg.b	D0
	ext.w	D0
	move.w	D0,($8C+178,SP)
	move.l	A0,D2
	lea	($2D,SP),A0
	move.l	A0,D1
	movea.l	(4,A3),A6
	sys	AddPart
\7	clr.w	($8E+178,SP)
	lea	($2D,SP),A3
	moveq	#$20,D0
	bra.b	\10

\8	cmp.b	(A3),D0
	bne.b	\9
	move.w	#1,($8E+178,SP)
\9	addq.l	#1,A3
\10	tst.b	(A3)
	bne.b	\8
	tst.w	($8E+178,SP)
	beq.b	\11
	cmpi.l	#$1FF,D7
	bge.b	\11
	moveq	#1,d1
	move.l	D7,d0
	movea.l	A5,a1
	movea.l	($A8+178-12,SP),a0
	pea	($92+178-16,SP)
	bsr	lbC003280
	addq.l	#1,A5
	addq.l	#1,D7
\11	move.l	A3,D1
	lea	($2D,SP),A0
	sub.l	A0,D1
	move.l	#$1FF,D0
	sub.l	D7,D0
	cmp.l	D1,D0
	blt.b	\12
	move.l	D1,D0
\12	move.l	D0,D2
	move.l	D0,d1
	move.l	D7,d0
	movea.l	A5,a1
	move.l	A0,-(SP)
	movea.l	($A8+178-8,SP),a0
	bsr	lbC003280
	adda.l	D2,A5
	add.l	D2,D7
	movea.l	($28,SP),A3
	tst.w	($8E+178,SP)
	beq.b	\14
	cmpi.l	#$1FF,D7
	bge.b	\14
	tst.w	($8C+178,SP)
	bne.b	\13
	move.l	($1E,A3),D0
	subq.l	#1,D0
	cmp.l	D0,D5
	bge.b	\14
\13	moveq	#1,d1
	move.l	D7,d0
	movea.l	A5,a1
	movea.l	($A8+178-12,SP),a0
	pea	($92+178-16,SP)
	bsr	lbC003280
	addq.l	#1,A5
	addq.l	#1,D7
\14	cmpi.l	#$1FF,D7
	bge.b	\16
	tst.w	($8C+178,SP)
	bne.b	\15
	move.l	($1E,A3),D0
	subq.l	#1,D0
	cmp.l	D0,D5
	bge.b	\16
\15	moveq	#1,d1
	move.l	D7,d0
	movea.l	A5,a1
	movea.l	($A8+178-12,SP),a0
	pea	($93+178-16,SP)
	bsr	lbC003280
	addq.l	#1,A5
	addq.l	#1,D7
\16	addq.l	#8,A2
	addq.l	#1,D5
\17	movea.l	($28,SP),A3
	cmp.l	($1E,A3),D5
	blt	\1
\18	movea.l	A3,A1
	movea.l	(_exec,A4),A6
	sys	ReplyMsg
\19	movea.l	($98+178,SP),A2
	movea.l	(_exec,A4),A6
.19	movea.l	($CC,A2),A0
	sys	GetMsg
	move.l	D0,($28,SP)
	beq	\20
	movea.l	D0,A3

	ifd	scn
	tst.w	($104,A2)
	bne	\skip
	tst.l	(_client,a4)		notify client has been added?
	beq	\skip
	moveq	#SCREENNOTIFY_TYPE_WORKBENCH,d0
	cmp.l	(snm_Type,a3),d0
	bne	\skip
	move.l	a3,(_scnm,a4)
	movea.l	($94+178,sp),a5
	tst.l	(snm_Value,a3)
	bne	\reopen
	move.l	A2,a0
	bsr	lbC001596
	movea.l	A2,a0
	bsr	_CloseWindow
	movea.l	a3,a1
	sys	ReplyMsg
	movea.l	($CC,A2),A0
	sys	WaitPort
	bra	.19

\reopen	tst.l	($78,A2)		is there a window?
	bne	\bogus
	movea.l	a2,a0
	movea.l	a5,a1
	moveq	#0,d0
	bsr	_OpenWindow		reopen window (deiconify)
	move.l	($78,A2),D0		open succeded?
	beq	\bogus
	movea.l	A2,a0
	bsr	lbC00BD7E
	tst.l	D0
	bne	\bogus
	move.l	($1A8+24+$11e+9*4,SP),d0	len of string
	lea	($9FC,A2),a0		ptr to string
	move.l	A2,d1
	movea.l	A5,a1
	bsr	lbC0031DE		print it
\bogus	clr.l	(_scnm,a4)
	bra	\18
\skip	endc

	moveq	#AMTYPE_APPWINDOW,D0
	cmp.w	(am_Type,A3),D0
	bne	\18
	movea.l	(am_ArgList,A3),A2
	moveq	#0,D5
	bra	\17

\20	cmp.l	D7,D6
	bge	\21
	movea.l	($78,A2),A0
	movea.l	([$94+178,SP],8),A6
	sys	ActivateWindow
\21	move.l	D7,D0
	sub.l	D6,D0
	movem.l	(SP)+,D2/D3/D5-D7/A2/A3/A5/A6
	adda.w	#$11E,SP
	rts

_addAppIcon	movem.l	A2-A4/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A2
	move.l	($DC,A3),d0
	beq.b	\1
	moveq	#0,d0
	move.l	($170,A3),d1
;	move.l	A3,a0
;	move.l	A2,a1
	bsr	lbC00D394
	lea	($184,A3),A0
	movea.l	($CC,A3),A1
	move.l	(20,A2),d0	wblib
	beq	.ret
	movea.l	d0,a6
	moveq	#0,D0
	move.l	D0,D1
	suba.l	A2,A2
	move.l	A3,-(SP)
	movea.l	($DC,A3),A3
	suba.l	a4,A4
	sys	AddAppIconA
	movea.l	(SP)+,A3
.ret	move.l	D0,($D4,A3)
\1	movem.l	(SP)+,A2-A4/A6
	rts

_addAppMenu	movem.l	A2/A3/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A2
	moveq	#0,d0
	move.l	($170,A3),d1
	move.l	A3,a0
	move.l	A2,a1
	bsr	lbC00D394
	lea	($184,A3),A0
	movea.l	($CC,A3),A1
	move.l	(20,A2),d0	wblib
	beq	.ret
	movea.l	d0,a6
	moveq	#0,D0
	move.l	D0,D1
	movea.l	d0,a2
	sys	AddAppMenuItemA
.ret	move.l	D0,($D8,A3)
	movem.l	(SP)+,A2/A3/A6
	rts

_remAppIcon:	move.l	A6,-(SP)
	movea.l	(20,a5),A6
	move.l	($D4,A3),D0
	beq	\ni
	movea.l	D0,A0
	sys	RemoveAppIcon
	clr.l	($D4,A3)
\ni	move.l	($D8,A3),D0
	beq	.ret
	movea.l	D0,A0
	sys	RemoveAppMenuItem
	clr.l	($D8,A3)
.ret	movea.l	(SP)+,A6
	rts

lbC00CC76	suba.w	#$28,SP
	movem.l	D7/A2/A3/A5/A6,-(SP)
	movea.l	a0,A3
	movea.l	a1,A5
	clr.b	($14,SP)
	movea.l	($178,A3),a0
	bsr	_strlen
	move.l	D0,D7
	lea	(ENVSys.MSG,PC),A0
	lea	($14,SP),A1
	moveq	#8,D0
	movea.l	(_exec,A4),A6
	sys	CopyMem
	moveq	#$1F,D0
	cmp.l	D0,D7
	bhi.b	\1
	move.l	D7,D0
\1	movea.l	($178,A3),A0
	lea	($1C,SP),A1
	sys	CopyMem
	addq.l	#8,D7
	moveq	#$27,D0
	cmp.l	D0,D7
	bhi.b	\2
	move.l	D7,D0
\2	clr.b	($14,SP,D0.L)
	lea	($14,SP),a1
	move.l	A5,a0
	bsr	_GetDiskObject
	movea.l	D0,A2
	tst.l	D0
	bne.b	\3
	suba.l	a1,a1
	move.l	A5,a0
	bsr	_GetDiskObject
	movea.l	D0,A2
	tst.l	D0
	beq.b	\4
	move.w	#2,($10,A2)
	move.l	#$35001f,(12,a2)
	lea	(lbL00F234,pc),A0
	move.l	A0,($16,A2)
	lea	(lbL00F248,pc),A0
	move.l	A0,($1A,A2)
\3	move.l	($144,A3),($3A,A2)
	move.l	($148,A3),($3E,A2)
\4	move.l	A2,($DC,A3)
	move.l	A2,D0
	movem.l	(SP)+,D7/A2/A3/A5/A6
	adda.w	#$28,SP
	rts

lbC00CDAA	cmpi.l	#IM_DRAW,(a1)
	beq	\draw
	move.l	a2,d1
	bsr	lbC00DAC6
\dummy	rts	do not remove the label!!! (snmaopt j+!)

	clrFO
.w	fo.l	1
.h	fo.l	1

\draw	addq.l	#4,a1
	move.l	(A1)+,d0	impd_RPort
	beq	\dummy
	link	a3,#_FO
	movem.l	d2-d7/A2/a5/A6,-(SP)
	movea.l	d0,a5		rastport
	movea.l	(cl_UserData,A0),A6	gfx base
	move.w	(A2)+,d6	ig_LeftEdge
	add.w	(A1)+,d6	impd_OffsetX
	ext.l	d6
	move.w	(A2)+,d5	ig_TopEdge
	add.w	(A1)+,d5	impd_OffsetY
	ext.l	d5
	move.w	(A2)+,d3	ig_Width
	ext.l	d3
	move.w	(A2),d2		ig_Height
	ext.l	d2
	move.l	(A1)+,d7	impd_State
	move.l	(A1),d0		impd_DrInfo
	beq	\3
	movea.l	d0,a2
	movea.l	(dri_Pens,a2),A2

	addq.l	#1,d6
	move.l	d3,(.w,a3)
	add.l	d6,d3
	move.l	d2,(.h,a3)
	subq.l	#2,d3

	movea.l	A5,A1
	moveq	#RP_JAM1,D0
	sys	SetDrMd

*        _______________
*       |
*       |
*       |
*       |
*       |

	moveq	#IDS_SELECTED,D0
	cmp.l	d7,D0
	seq	d4
	moveq	#3,D1
	sub.b	d4,D1
	move.w	(A2,D1.L*2),D0
	movea.l	A5,A1
	sys	SetAPen

	add.l	d5,d2
	subq.l	#2,d2
	move.l	d2,D1
	move.l	d6,D0
	movea.l	A5,A1
	sys	Move
	move.l	d6,D0
	move.l	d5,d1	^
	movea.l	A5,A1	|
	sys	Draw	|
	move.l	d3,d0
	move.l	d5,d1	---->
	movea.l	A5,A1	|
	sys	Draw	|

*        _______________
*       |               
*       |               
*       |               
*       |               
*       |               
*        ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	not.b	d4
	moveq	#3,D1
	sub.b	d4,D1
	moveq	#0,D0
	move.w	(A2,D1.L*2),D0
	movea.l	A5,A1
	sys	SetAPen
	move.l	d3,D0
	movea.l	A5,A1
	addq.l	#1,d2
	move.l	d2,D1
	sys	Move
	move.l	d6,D0
	movea.l	A5,A1	----
	move.l	d2,D1	|
	sys	Draw	|___ <

* clear the rectangle

	moveq	#5,d1
	moveq	#IDS_INACTIVENORMAL,D0
	cmp.l	d7,D0
	bne	1$
	moveq	#7,d1
1$	move.w	(A2,d1.L*2),D0
	movea.l	A5,A1
	sys	SetAPen

	move.l	d6,D0
	addq.l	#1,D0
	addq.l	#1,d5
	move.l	d5,d1
	exg	d2,d3
	subq.l	#1,d3
	subq.l	#1,d2
	movea.l	A5,A1
	sys	RectFill

*         _______________
*      | |               
*      | |               
*      | |               
*      | |               
*      | |               
*         ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	moveq	#IDS_INACTIVENORMAL,D0
	moveq	#4,D1
	cmp.l	d7,D0
	beq	\1
	move.w	(FILLPEN*2,A2),D0
	cmp.w	(SHADOWPEN*2,A2),D0
	bne	\1
	moveq	#7,d1
\1	move.w	(A2,D1.L*2),D0
	movea.l	A5,A1
	sys	SetAPen

	move.l	d6,d0
	subq.l	#1,d0
	move.l	d3,d1
	movea.l	A5,A1
	sys	Move
	move.l	d6,D0
	subq.l	#1,d0
	move.l	d5,D1
	movea.l	A5,A1
	sys	Draw

*         _______________
*      | |               
*      | |    ___        
*      | |   |___|       
*      | |               
*      | |               
*         ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	moveq	#5,d1
	move.w	(.w+2,a3),d0	image width
	cmpi.w	#24,d0
	bcc	\h
	cmpi.w	#18,d0
	bcc	\l
	moveq	#4,d1
\l	subq.w	#2,d1
\h	add.w	d1,d6		X left of bigger box
	addq.w	#1,d2
	sub.w	d1,d2		X right of bigger box

	move.w	(.h+2,a3),d4	image height
	addq.w	#1,d4
	moveq	#5,d0
	divu	d0,d4
	subq.w	#1,d4
	sub.w	d4,d3		Y bottom of bigger box
	add.w	d4,d5		Y top of bigger box
	movem.l	d2/d3/d5/d6,-(sp)

	moveq	#2,d4
	move.w	d3,d1
	sub.w	d5,d1		height of bigger box
	cmp.w	d0,d1
	bhi	.n
	moveq	#1,d4
.n	sub.w	d4,d3		Y bottom of smaller box
	moveq	#-2,d5
	add.w	d3,d5		Y top of smaller box
	addq.w	#2,d6		X left of smaller box
	move.l	d6,d2
	addq.w	#3,d2		X right of smaller box

x1	equr	d6
x2	equr	d2
y1	equr	d3
y2	equr	d5

	bsr	\do

*         _______________
*      | |  ___________  |
*      | | |   ___     | |
*      | | |  |___|    | |
*      | | |           | |
*      | |  ¯¯¯¯¯¯¯¯¯¯¯  |
*         ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

	move.l	d2,d1		X right of smaller box
	move.l	d5,d4		Y top of smaller box
	movem.l	(sp)+,d2/d3/d5/d6
	moveq	#IDS_SELECTED,D0
	cmp.l	d7,D0
	beq.b	\sel
	bsr	\do
	bra	\2

\do	move.l	x1,d0
	move.l	y1,d1
	movea.l	a5,a1
	sys	Move
	move.l	x1,d0
	move.l	y2,d1
	movea.l	A5,A1
	sys	Draw
	move.l	y2,d1
	move.l	x2,d0
	movea.l	A5,A1
	sys	Draw
	move.l	x2,d0
	move.l	y1,d1
	movea.l	A5,A1
	sys	Draw
	move.l	y1,d1
	move.l	x1,d0
	movea.l	A5,A1
	sys	Draw
	rts

\sel	move.l	d4,d0
	addq.w	#2,d1
	move.l	d1,d2
	subq.w	#2,d0
	cmp.w	d5,d0
	bcs	.ok
	move.l	d0,d5
.ok	bsr	\do

\2	moveq	#IDS_INACTIVENORMAL,D0
	sub.l	d7,D0
	beq.b	\3
	moveq	#0,D0
	move.w	(6,A2),D0
	movea.l	A5,A1
	sys	SetAPen
	addq.w	#1,d4
	addq.w	#3,d6
	move.l	d6,d0
	move.l	d4,d1
	movea.l	A5,A1
	sys	Move
	addq.w	#1,d6
	move.l	d6,d0
	move.l	d4,d1
	sys	Draw

	moveq	#0,D0
\3	movem.l	(SP)+,d2-d7/A2/a5/A6
	unlk	a3
	rts


ENV.MSG	dc.b	'ENV'
nullbyte2	dc.b	0	;static
ENVSys.MSG	dc.b	'ENV:Sys/',0
imageclass.MSG	dc.b	'imageclass',0
C.MSG	dc.b	'C:',0
Pool.MSG	dc.b	'Pool',0
;NULLPOINTER.MSG	dc.b	'***NULL POINTER***',0
;abcdef.MSG	dc.b	'0123456789abcdef /',0

	ifd	tbc
tbiclass.MSG	dc.b	'tbiclass',0
	endc

lbC00D17A	movem.l	D6/D7/A2/A6,-(SP)
	movea.l	($88,A3),A2		screen
;	movea.l	(sc_ViewPort+vp_ColorMap,a2),a1
;	movea.l	(cm_vpe,a1),a1
;	move.w	(vpe_DisplayClip+ra_MaxX,A1),D0
;	sub.w	(vpe_DisplayClip+ra_MinX,A1),D0
;	addq.w	#1,D0
;	cmpi.w	#$190,D0
;	blt.b	\1
;	moveq	#$18,D7
;	bra.b	\2
;\1	moveq	#$12,D7
	move.l	($94+20,sp),d7		current width of win depth gadget
\2	movea.l	(sc_Font,A2),A0
	moveq	#0,D0
	move.w	(4,A0),D0
	move.b	(sc_WBorTop,A2),D6
	extb.l	D6
	add.l	D0,D6
	addq.l	#1,D6
	moveq	#16,D0
	move.l	#MEMF_CLEAR|MEMF_PUBLIC,D1
	movea.l	(a5),A6
	sys	AllocMem
	move.l	D0,($84,A3)
	beq	\10
	movea.l	(8,A5),A6
	move.l	D0,-(SP)

	ifd	OS4
	movea.l	($88,A3),A0
	sys	GetScreenDrawInfo
	movea.l	(sp),a0
	move.l	d0,(12,a0)
	beq	1$
	moveq	#ICONIFYIMAGE,d1
	lea	(sysiclass.MSG,PC),A1
	bsr	\na

1$	movea.l	(SP)+,A2
	move.l	d0,(4,a2)
	bne	\1
	move.l	a2,-(sp)
	endc

	ifd	tbc
	ifnd	OS4
	movea.l	($88,A3),A0
	sys	GetScreenDrawInfo
	movea.l	(sp),a0
	move.l	d0,(12,a0)
	beq	2$
	else
	movea.l	(sp),a0
	move.l	(12,a0),d0
	endc

	moveq	#TBI_ICONIFYIMAGE,d1
	lea	(tbiclass.MSG,PC),A1
	bsr	\na
2$	movea.l	(SP)+,A2
	move.l	d0,(4,a2)
	bne	\1
	move.l	a2,-(sp)
	endc

	lea	(imageclass.MSG,PC),A1
	suba.l	A0,A0
	suba.l	A2,A2
	moveq	#0,D0
	move.l	D0,D1
	sys	MakeClass
	movea.l	(SP)+,A2
	move.l	D0,(A2)
	beq	\8
	lea	(lbC00CDAA,PC),A1
	movea.l	D0,A0
	move.l	A1,(cl_Dispatcher+h_Entry,A0)
	clr.l	(cl_Dispatcher+h_SubEntry,A0)
	move.l	(16,A5),(cl_UserData,A0)	gfx base
	move.l	A2,-(SP)
	clr.l	-(SP)
	move.l	D6,-(SP)
	move.l	#IA_Height,-(SP)
	move.l	D7,-(SP)
	move.l	#IA_Width,-(SP)
	clr.l	-(SP)
	move.l	#IA_Top,-(SP)
;	clr.l	-(SP)
	pea	(-1).w
	move.l	#IA_Left,-(SP)
	suba.l	A1,A1
	movea.l	SP,A2
	sys	NewObjectA
	lea	(9*4,SP),SP
	movea.l	(SP)+,A2
	move.l	D0,(4,A2)
	beq	\7

\1	move.l	A2,-(SP)
	clr.l	-(SP)
	move.l	D0,-(SP)
	move.l	#GA_Image,-(SP)
	moveq	#1,d1
	move.l	d1,-(sp)
	move.l	#GA_TopBorder,-(SP)
	ifd	OS4
	move.l	d1,-(sp)
	move.l	#GA_Titlebar,-(sp)
	endc
	move.l	D6,-(SP)
	move.l	#GA_Height,-(SP)
	subq.l	#1,d7
	move.l	D7,-(SP)
	move.l	#GA_Width,-(SP)
	move.l	d1,-(sp)
	move.l	#GA_RelVerify,-(SP)
	move.l	D7,D0
	add.l	D0,D0
	btst	#5,($163,A3)
	bne.b	\3
	btst	#1,($160,A3)
	bne.b	\3
	add.l	D7,D0
\3	sub.l	d0,d1
	move.l	D1,-(SP)
	move.l	#GA_RelRight,-(SP)
	clr.l	-(SP)
	move.l	#GA_Top,-(SP)
	pea	($65).W
	move.l	#GA_ID,-(SP)
	pea	(-1).W
	move.l	#ICA_TARGET,-(SP)
	lea	(buttongclass.MSG,PC),A1
	suba.l	A0,A0
	movea.l	SP,A2
	sys	NewObjectA
	ifd	OS4
	lea	($4c+8,sp),sp
	else
	lea	($4C,SP),SP
	endc
	movea.l	(SP)+,A2
	move.l	D0,(8,A2)
	beq.b	\6

	movea.l	D0,A1
	move.l	d0,d7
	movea.l	($78,A3),A0
	moveq	#0,D0
	sys	AddGadget
;	move.l	A2,-(SP)
	movea.l	d7,A0
	movea.l	($78,A3),A1
	suba.l	A2,A2
	moveq	#1,D0
	sys	RefreshGList
;	movea.l	(SP)+,A0
;	move.l	A0,($84,A3)
	move.l	d7,D0
	bra.b	\10

	ifd	OS4
\na	clr.l	-(SP)
	move.l	d0,-(sp)
	move.l	#SYSIA_DrawInfo,-(SP)
	move.l	d1,-(sp)
	move.l	#SYSIA_Which,-(SP)
	suba.l	A0,A0
	movea.l	SP,A2
	sys	NewObjectA
	lea	(5*4,SP),SP
	rts
	else

	ifd	tbc
\na	clr.l	-(SP)
	move.l	d0,-(sp)
	move.l	#SYSIA_DrawInfo,-(SP)
	move.l	d1,-(sp)
	move.l	#SYSIA_Which,-(SP)
	suba.l	A0,A0
	movea.l	SP,A2
	sys	NewObjectA
	lea	(5*4,SP),SP
	rts
	endc
	endc

\6	movea.l	(4,A2),A0
	sys	DisposeObject
\7	movea.l	(A2),A0
	move.l	a0,d0
	beq	\8
	sys	FreeClass
\8	movea.l	A2,A1
	movea.l	(A5),A6
	moveq	#16,d0
	sys	FreeMem
	moveq	#0,D0
\10	movem.l	(SP)+,D6/D7/A2/A6
	rts


lbC00D394	movem.l	D2-D7/A2/A3/A5/A6,-(SP)
	movea.l	a0,A5
	movea.l	a1,a2
	move.l	d0,d2
	move.l	d1,d3
	bsr	lbC0094C2
	moveq	#0,D7
	moveq	#0,D4
	moveq	#0,D5
	bra	\33

\1	moveq	#$25,D1
	cmp.b	D1,D4
	bne	\30
	moveq	#0,D0
	move.b	(A3),D0
;	moveq	#$25,D1
	sub.l	D1,D0
	beq	\28
	moveq	#$1C,D1
	sub.l	D1,D0
	beq	\10
	subq.l	#3,D0
	beq.b	\2
	moveq	#10,D1
	sub.l	D1,D0
	beq	\11
	subq.l	#2,D0
	beq	\23
	subq.l	#3,D0
	beq.b	\2
	subq.l	#3,D0
	beq	\18
	moveq	#11,D1
	sub.l	D1,D0
	beq	\10
	subq.l	#3,D0
	beq.b	\2
	moveq	#10,D1
	sub.l	D1,D0
	beq	\11
	subq.l	#2,D0
	beq	\23
	subq.l	#3,D0
	beq.b	\2
	subq.l	#3,D0
	beq	\18
	bra	\29

\2	move.l	A5,a1
	move.l	A2,a0
	bsr	lbC0071B4
	move.l	D0,D6
	lea	($184,A5,d7.l),A3
	clr.b	(A3)
	moveq	#1,D5
	moveq	#$78,D1
	sub.l	D7,D1
	exg	d0,d1
	move.l	A3,a1
	move.l	A2,a0
	bsr	_NameFromLock
	move.b	d3,D0
	moveq	#$64,D1
	cmp.b	D1,D0
	beq.b	\3
	moveq	#$44,D1
	cmp.b	D1,D0
	bne.b	\7
\3	move.l	A3,D1
	movea.l	(4,A2),A6
	jsr	(-$366,A6)
	movea.l	D0,A1
	movea.l	D0,A0
	tst.b	(A0)
	beq.b	\6
	bra.b	\5

\4	move.b	(A1)+,($184,A5,d7.l)
	addq.l	#1,D7
\5	tst.b	(A1)
	bne.b	\4
	bra.b	\9

\6	move.l	A1,D0
	sub.l	A3,D0
	bra.b	\8

\7	movea.l	A3,a0
	bsr	_strlen
\8	add.l	D0,D7
\9	move.l	D6,a1
	movea.l	a2,a0
	bsr	_UnLock
	bra	\29

\10	move.l	A5,a1
	move.l	A2,a0
	bsr	lbC0071B4
	move.l	D0,D6
	beq	\29
	moveq	#1,D5
	moveq	#$78,D1
	sub.l	D7,D1
	moveq	#18,d0
	move.l	d1,-(SP)
	pea	($184,A5,d7.l)
	move.l	d6,-(SP)
	movea.l	a2,a0
	bsr	_DoSyncDos
	lea	(12,SP),SP
	add.l	D0,D7
	move.l	D6,a1
	move.l	A2,a0
	bsr	_UnLock
	bra	\29

\11	moveq	#$70,D0
	cmp.l	D0,D7
	bge	\29
	movea.l	(_exec,A4),A6
	sys	Forbid
	move.l	($ec,a5),d6
	beq.b	\13
	movea.l	d6,a0
	move.l	(pr_TaskNum,a0),D6
\13	sys	Forbid
	tst.l	D6
	beq.b	\17
	lea	($184,A5,d7.l),a0
	move.l	A0,-(SP)
	move.l	d6,D0
	link.w	A5,#-12
	movea.l	SP,A1
	bge.b	\15
	move.b	#$2D,(A0)+
	neg.l	D0
\15	moveq	#10,D1
	bsr	lbC00DC48
	addi.w	#$30,D1
	move.b	D1,(A1)+
	tst.l	D0
	bne.b	\15
\16	move.b	-(A1),(A0)+
	cmpa.l	A1,SP
	bne.b	\16
	clr.b	(A0)
	move.l	A0,D0
	unlk	A5
	sub.l	(SP)+,D0

	add.l	D0,D7
	bra	\29

\17	move.b	#$2D,($184,A5,d7.l)
	addq.l	#1,D7
	bra	\29

\18	move.l	A5,a1
	move.l	A2,a0
	bsr	lbC0071B4
	move.l	D0,D6
	moveq	#1,D5
	moveq	#$78,D1
	sub.l	D7,D1
	exg	d0,d1
	lea	($184,A5,d7.l),a1
	move.l	A2,a0
	bsr	_NameFromLock
	tst.w	D0
	beq.b	\22
	bra.b	\20

\19	addq.l	#1,D7
\20	moveq	#$3A,D0
	cmp.b	($184,A5,D7.L),D0
	beq.b	\21
	moveq	#$76,D0
	cmp.l	D0,D7
	blt.b	\19
\21	addq.l	#1,D7
	clr.b	($184,A5,D7.L)
\22	move.l	D6,a1
	move.l	A2,a0
	bsr	_UnLock
	bra.b	\29

\23	movea.l	(_exec,A4),A6
	sys	Forbid
	move.l	($ec,a5),d0
	beq.b	\26
	movea.l	d0,a0
	movea.l	(LN_NAME,a0),A1
	bra.b	\25

\24	move.b	(A1)+,($184,a5,d7.l)
	addq.l	#1,D7
\25	tst.b	(A1)
	beq.b	\27
	moveq	#$77,D0
	cmp.l	D0,D7
	blt.b	\24
	bra.b	\27

\26	move.b	#$2D,($184,a5,d7.l)
	addq.l	#1,D7
\27	movea.l	(_exec,A4),A6
	sys	Permit
	bra.b	\29

\28	move.b	#$25,($184,A5,d7.l)
	addq.l	#1,D7
\29	moveq	#0,D4
	bra.b	\32

\30	move.b	(A3),D0
;	moveq	#$25,D1
	cmp.b	D1,D0
	beq.b	\31
	move.b	d0,($184,a5,d7.l)
	addq.l	#1,D7
	move.b	D0,D4
	bra.b	\32

\31	move.b	D1,D4
\32	addq.l	#1,d3
\33	movea.l	d3,A3
	tst.b	(A3)
	beq.b	\34
	moveq	#$77,D0
	cmp.l	D0,D7
	blt	\1
\34	tst.w	d2
	beq.b	\35
	tst.w	D5
	bne.b	\35
	btst	#6,($161,A5)
	beq.b	\35
	moveq	#$73,D0
	cmp.l	D0,D7
	bge.b	\35
	move.l	A5,a1
	move.l	A2,a0
	bsr	lbC0071B4
	move.l	D0,D6
	moveq	#$20,D1
	lea	($184,A5,d7.l),a1
	move.b	D1,(A1)+
	move.b	D1,(A1)+
	moveq	#$76,D1
	sub.l	D7,D1
	exg	d0,d1
	move.l	A2,a0
	bsr	_NameFromLock
	move.l	D6,a1
	move.l	A2,a0
	bsr	_UnLock
	bra.b	\36

\35	clr.b	($184,A5,D7.L)
\36	move.l	($EC,A5),D0
	beq.b	\37
	movea.l	D0,A0
	move.l	(pr_CurrentDir,A0),($E4,A5)
\37	move.w	D5,D0
	movem.l	(SP)+,D2-D7/A2/A3/A5/A6
	rts

lbC00D666	move.l	A6,-(SP)
	movea.l	(8,a5),A6
	IFND	rom
	cmpi.w	#39,(20,A6)
	bcs.b	\2x
	ENDC
	clr.l	-(SP)
	move.l	D0,-(SP)
	move.l	#WA_PointerDelay,-(SP)
	move.l	D0,-(SP)
	move.l	#WA_BusyPointer,-(SP)
	movea.l	SP,A1
	sys	SetWindowPointerA
	lea	(20,SP),SP
\2x	movea.l	(SP)+,A6
	rts

lbC00D6B4	suba.w	#20,SP
	movem.l	A2/A3/A5/A6,-(SP)
	tst.b	($100,A3)
	beq.b	\2
	moveq	#EasyStruct_SIZEOF,D0
	lea	(16,SP),A2
	move.l	D0,(A2)+
	clr.l	(A2)+
	lea	(mess,pc),a1
	bsr	_getstr
	move.l	a1,(A2)+
	lea	(swarn,pc),a1
	bsr	_getstr
	move.l	a1,(A2)+
	lea	(sgadg,pc),a1
	move.l	a1,(A2)
	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC00170E
	move.l	A3,-(SP)
	lea	(Cancel.MSG,pc),a1
	bsr	_getstr
	pea	(a1)
	lea	(gJump,pc),a1
	bsr	_getstr
	pea	(a1)
	movea.l	($78,A3),A0
	lea	(28,SP),A1
	movea.l	(8,A5),A6
	suba.l	A2,A2
	move.l	sp,A3
	sys	EasyRequestArgs
	addq.l	#8,sp
	movea.l	(SP)+,A3
	tst.l	D0
	beq.b	\1
	clr.b	($100,A3)
\1	move.l	A3,a0
	move.l	A5,a1
	bsr	lbC001746
\2	tst.b	($100,A3)
	beq.b	\3
	moveq	#0,D0
	bra.b	\4

\3	moveq	#1,D0
\4	movem.l	(SP)+,A2/A3/A5/A6
	adda.w	#20,SP
	rts

lbC00D784	suba.w	#$38,SP
	movem.l	A2/A3/A5/A6,-(SP)
;	movea.l	($4C,SP),A5
	clr.l	($44,SP)
	moveq	#1,d0
	suba.l	a1,a1
	lea	($1A-8,SP),a0
	bsr	lbC009826
	tst.w	D0
	beq	\9
	movea.l	(8,A5),A6
	sys	LockPubScreenList
	tst.l	D0
	beq.b	\4
	movea.l	D0,A0
	movea.l	(A0),A3
	bra.b	\3

\1	movea.l	(psn_Screen,A3),A0
	movea.l	($50,SP),A1
	cmpa.l	($88,A1),A0
	beq.b	\2
	movea.l	($30,SP),A2
	movea.l	(LN_NAME,A3),a0
	bsr	_strlen
	move.l	D0,-(SP)
	move.l	(10,A3),-(SP)
	pea	($1A,SP)
	move.l	A5,-(SP)
	jsr	(A2)
	lea	(16,SP),SP
\2	movea.l	(A3),A3
\3	tst.l	(A3)
	bne.b	\1
	movea.l	(8,A5),A6
	sys	UnlockPubScreenList
\4	movea.l	($50,SP),A3
	move.l	($20,SP),D0
	moveq	#1,D1
	cmp.l	D1,D0
	bne.b	\5
	movea.l	($12,SP),A2
	bra.b	\7

\5	cmp.l	D1,D0
	ble.b	\6
	lea	(SelectPublics.MSG,pc),a1
	bsr	_getstr
	pea	(a1)
	clr.l	-(SP)
	pea	($1A,SP)
	move.l	A3,-(SP)
	move.l	A5,-(SP)
	bsr	lbC007932
	movea.l	D0,A2
	bra.b	\7

\6	suba.l	A2,A2
\7	move.l	A2,D0
	beq.b	\8
;	move.l	A3,-(SP)
;	move.l	A5,-(SP)
	bsr	lbC00D6B4
	tst.w	D0
	beq.b	\8
	adda.w	#$12,A2
	movea.l	A2,A0
	movea.l	(8,A5),A6
	jsr	(-$1FE,A6)
	move.l	D0,($44,SP)
	beq.b	\8
	movea.l	A2,A0
	movea.l	($54,SP),A1
	move.w	#138,d0	
\c	move.b	(a0)+,(a1)+
	dbeq	d0,\c
\8	movea.l	($40,SP),A0
	pea	($12,SP)
	move.l	A5,-(SP)
	jsr	(A0)
	addq.l	#8,SP
\9	move.l	($44,SP),D0
	movem.l	(SP)+,A2/A3/A5/A6
	adda.w	#$38,SP
	rtd	#12

lbC00DBB0	movem.l	D2/D3/A2,-(SP)
	movea.l	A0,A2
	moveq	#0,D1
	move.l	D1,D0
	move.l	D1,D3
	cmpi.b	#$2B,(A0)
	beq.b	lbC00DBCA
	cmpi.b	#$2D,(A0)
	bne.b	lbC00DBCC
	moveq	#1,D3
lbC00DBCA	addq.w	#1,A0
lbC00DBCC	move.b	(A0)+,D0
	subi.b	#$30,D0
	blt.b	lbC00DBEE
	cmpi.b	#9,D0
	bgt.b	lbC00DBEE
	move.l	D1,D2
	asl.l	#2,D1
	add.l	D2,D1
	add.l	D1,D1
	tst.b	D3
	bne.b	lbC00DBEA
	add.l	D0,D1
	bra.b	lbC00DBCC

lbC00DBEA	sub.l	D0,D1
	bra.b	lbC00DBCC

lbC00DBEE	move.l	D1,(A1)
	move.l	A0,D0
	sub.l	A2,D0
	subq.l	#1,D0
	movem.l	(SP)+,D2/D3/A2
	rts

lbC00DC48	move.l	D2,-(SP)
	swap	D1
	move.w	D1,D2
	bne	lbC00DC72
	swap	D0
	swap	D1
	swap	D2
	move.w	D0,D2
	beq	\1
	divu.w	D1,D2
	move.w	D2,D0
\1	swap	D0
	move.w	D0,D2
	divu.w	D1,D2
	move.w	D2,D0
	swap	D2
	move.w	D2,D1
	move.l	(SP)+,D2
	rts

lbC00DC72	move.l	D3,-(SP)
	moveq	#$10,D3
	cmpi.w	#$80,D1
	bcc	lbC00DC82
	rol.l	#8,D1
	subq.w	#8,D3
lbC00DC82	cmpi.w	#$800,D1
	bcc	lbC00DC8E
	rol.l	#4,D1
	subq.w	#4,D3
lbC00DC8E	cmpi.w	#$2000,D1
	bcc	lbC00DC9A
	rol.l	#2,D1
	subq.w	#2,D3
lbC00DC9A	tst.w	D1
	bmi	lbC00DCA4
	rol.l	#1,D1
	subq.w	#1,D3
lbC00DCA4	move.w	D0,D2
	lsr.l	D3,D0
	swap	D2
	clr.w	D2
	lsr.l	D3,D2
	swap	D3
	divu.w	D1,D0
	move.w	D0,D3
	move.w	D2,D0
	move.w	D3,D2
	swap	D1
	mulu.w	D1,D2
	sub.l	D2,D0
	bcc	lbC00DCC6
	subq.w	#1,D3
	add.l	D1,D0
lbC00DCC6	moveq	#0,D1
	move.w	D3,D1
	swap	D3
	rol.l	D3,D0
	swap	D0
	exg	D0,D1
	move.l	(SP)+,D3
	move.l	(SP)+,D2
	rts

_strlen	move.l	A0,D0
.1	tst.b	(A0)+
	bne.b	.1
	subq.l	#1,A0
	suba.l	D0,A0
	move.l	A0,D0
	rts

lbC00DD34	move.l	A0,D1
	bra.b	\2

\1	move.b	(A1)+,(A0)+
	beq.b	\4
\2	subq.l	#1,D0
	bcc.b	\1
	bra.b	\5

\3	clr.b	(A0)+
\4	subq.l	#1,D0
	bcc.b	\3
\5	move.l	D1,D0
	rts

_RawDoFmt	movem.l	a2/a3/a6,-(sp)
	movea.l	a1,a3
	lea	(16,sp),a1
	lea	(\pp,pc),a2
	movea.l	(_exec,a4),a6
	sys	RawDoFmt
	movea.l	a3,a0
.c	tst.b	(a0)+
	bne	.c
	subq.l	#1,a0
	move.l	a0,d0
	sub.l	a3,d0
	movem.l	(sp)+,a2/a3/a6
	rtd	#4

\pp	move.b	d0,(a3)+
	rts

	ifeq	1

lbC00DD78	movea.l	A0,A1
	addq.l	#1,(A1)+
	movea.l	(A1),A0
	addq.l	#1,(A1)+
	move.b	D0,(A0)
	rts

	movea.l	(4,SP),A0
	move.l	A0,-(SP)
	clr.l	-(SP)
	movea.l	sp,a1
	pea	(20,SP)
	move.l	(20,SP),-(SP)
	lea	(lbC00DD78,PC),A0
	bsr	lbC00E218
	addq.l	#8,SP
	move.l	(SP)+,D0
	movea.l	(sp)+,a0
	clr.b	(A0)
	rts

lbC00DDB2	suba.w	#$48,SP
	movem.l	D2/D4-D7/A2/A3/A5,-(SP)
	movea.l	($70,SP),A5
	move.l	A0,($28,SP)
	clr.w	($5B,SP)
	clr.w	($46,SP)
;	clr.b	($47,SP)
	clr.b	($64,SP)
	move.b	#$20,($5A,SP)
	clr.l	($4C,SP)
	moveq	#-1,D0
	move.l	D0,($60,SP)
;	clr.b	($5C,SP)
	moveq	#0,D7
	moveq	#0,D6
	lea	($32,SP),A2
	movea.l	A1,A3
	bra.b	lbC00DE5E

lbC00DDF2	movea.l	($28,SP),A0
	moveq	#0,D0
	move.b	(A0),D0
	moveq	#$20,D1
	sub.l	D1,D0
	blt.b	lbC00DE66
	cmpi.l	#$11,D0
	bge.b	lbC00DE66
	add.w	D0,D0
	move.w	(lbW00DE12,PC,D0.W),D0
	jmp	(lbW00DE14,PC,D0.W)

lbW00DE12	dc.w	lbC00DE44-lbW00DE14
lbW00DE14	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE4C-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE3C-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE34-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE66-lbW00DE14
	dc.w	lbC00DE54-lbW00DE14

lbC00DE34	move.b	#1,($5B,SP)
	bra.b	lbC00DE5A

lbC00DE3C	move.b	#1,($46,SP)
	bra.b	lbC00DE5A

lbC00DE44	move.b	#1,($47,SP)
	bra.b	lbC00DE5A

lbC00DE4C	move.b	#1,($64,SP)
	bra.b	lbC00DE5A

lbC00DE54	move.b	#$30,($5A,SP)
lbC00DE5A	addq.l	#1,($28,SP)
lbC00DE5E	movea.l	($28,SP),A0
	tst.b	(A0)
	bne.b	lbC00DDF2
lbC00DE66	moveq	#1,D0
	cmp.b	($5B,SP),D0
	bne.b	lbC00DE74
	move.b	#$20,($5A,SP)
lbC00DE74	moveq	#$2A,D1
	movea.l	($28,SP),A0
	cmp.b	(A0),D1
	bne.b	lbC00DE98
	movea.l	(A3),A1
	addq.l	#4,(A3)
	move.l	(A1),D1
	move.l	D1,($4C,SP)
	bge.b	lbC00DE92
	neg.l	($4C,SP)
	move.b	D0,($5B,SP)
lbC00DE92	addq.l	#1,($28,SP)
	bra.b	lbC00DEA4

lbC00DE98	lea	($4C,SP),A1
	bsr	lbC00DBB0
	add.l	D0,($28,SP)
lbC00DEA4	movea.l	($28,SP),A0
	move.b	(A0),D0
	moveq	#$2E,D1
	cmp.b	D1,D0
	bne.b	lbC00DEF2
	addq.l	#1,($28,SP)
	moveq	#$2A,D0
	movea.l	($28,SP),A0
	cmp.b	(A0),D0
	bne.b	lbC00DED6
	movea.l	(A3),A0
	addq.l	#4,(A3)
	move.l	(A0),D0
	move.l	D0,($60,SP)
	bge.b	lbC00DED0
	moveq	#-1,D0
	move.l	D0,($60,SP)
lbC00DED0	addq.l	#1,($28,SP)
	bra.b	lbC00DEEC

lbC00DED6	lea	($60,SP),A1
	bsr	lbC00DBB0
	move.l	D0,D5
	bne.b	lbC00DEE8
	clr.l	($60,SP)
	bra.b	lbC00DEEC

lbC00DEE8	add.l	D5,($28,SP)
lbC00DEEC	move.b	#$20,($5A,SP)
lbC00DEF2	movea.l	($28,SP),A0
	moveq	#0,D0
	move.b	(A0),D0
	moveq	#$4C,D1
	sub.l	D1,D0
	beq.b	lbC00DF14
	moveq	#$1C,D1
	sub.l	D1,D0
	beq.b	lbC00DF0C
	subq.l	#4,D0
	beq.b	lbC00DF14
	bra.b	lbC00DF1E

lbC00DF0C	move.b	#2,($5C,SP)
	bra.b	lbC00DF1A

lbC00DF14	move.b	#1,($5C,SP)
lbC00DF1A	addq.l	#1,($28,SP)
lbC00DF1E	movea.l	($28,SP),A0
	addq.l	#1,A0
	movea.l	($28,SP),A1
	move.b	(A1),D0
	moveq	#0,D1
	move.b	D0,D1
	move.b	D0,($20,SP)
	move.l	A0,($22,SP)
	moveq	#$50,D0
	sub.l	D0,D1
	blt	lbC00E146
	cmpi.l	#$29,D1
	bge	lbC00E146
	add.w	D1,D1
	move.w	(lbW00DF52,PC,D1.W),D1
	jmp	(lbW00DF54,PC,D1.W)

lbW00DF52	dc.w	lbC00E06C-lbW00DF54
lbW00DF54	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E07E-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E134-lbW00DF54
	dc.w	lbC00DFC6-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00DFC6-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00DFA4-lbW00DF54
	dc.w	lbC00E07E-lbW00DF54
	dc.w	lbC00E06C-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E100-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E07E-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E146-lbW00DF54
	dc.w	lbC00E07E-lbW00DF54

lbC00DFA4	moveq	#2,D0
	cmp.b	($5C,SP),D0
	bne.b	lbC00DFBA
	movea.l	(A3),A0
	addq.l	#4,(A3)
	movea.l	(A0),A1
	move.l	(A5),D0
	move.w	D0,(A1)
	bra	lbC00E1F6

lbC00DFBA	movea.l	(A3),A0
	addq.l	#4,(A3)
	movea.l	(A0),A1
	move.l	(A5),(A1)
	bra	lbC00E1F6

lbC00DFC6	movea.l	(A3),A0
	addq.l	#4,(A3)
	move.l	(A0),D4
	bpl.b	lbC00DFD2
	moveq	#1,D7
	neg.l	D4
lbC00DFD2	tst.l	D7
	beq.b	lbC00DFE2
	lea	($33,SP),A2
	move.b	#$2D,($32,SP)
	bra.b	lbC00E004

lbC00DFE2	tst.b	($46,SP)
	beq.b	lbC00DFF4
	lea	($33,SP),A2
	move.b	#$2B,($32,SP)
	bra.b	lbC00E004

lbC00DFF4	tst.b	($47,SP)
	beq.b	lbC00E006
	lea	($33,SP),A2
	move.b	#$20,($32,SP)
lbC00E004	moveq	#1,D6
lbC00E006	move.l	D4,D0
	movea.l	A2,A0
lbC00DAF0	link.w	A5,#-12
	movea.l	SP,A1
lbC00DAF6	moveq	#10,D1
	bsr	lbC00DC48
	addi.w	#$30,D1
	move.b	D1,(A1)+
	tst.l	D0
	bne.b	lbC00DAF6
	move.l	A1,D0
lbC00DB08	move.b	-(A1),(A0)+
	cmpa.l	A1,SP
	bne.b	lbC00DB08
	clr.b	(A0)
	sub.l	SP,D0
	unlk	A5

lbC00E00E	move.l	D0,D5
lbC00E010	move.l	($60,SP),D0
	bne.b	lbC00E01C
	tst.l	D4
	beq	lbC00E1F6
lbC00E01C	move.l	($60,SP),D0
	bpl.b	lbC00E028
	moveq	#1,D1
	move.l	D1,($60,SP)
lbC00E028	move.l	($60,SP),D4
	sub.l	D5,D4
	ble.b	lbC00E04C
	movea.l	A2,A0
	adda.l	D4,A0
	move.l	D5,D0
	movea.l	A2,A1
lbC00DD58	move.l	A0,D1
	tst.l	D0
	ble.b	lbC00DD74
	cmpa.l	A1,A0
	bcs.b	lbC00DD6E
	adda.l	D0,A1
	adda.l	D0,A0
lbC00DD66	move.b	-(A1),-(A0)
	subq.l	#1,D0
	bne.b	lbC00DD66
	bra.b	lbC00DD74

lbC00DD6E	move.b	(A1)+,(A0)+
	subq.l	#1,D0
	bne.b	lbC00DD6E
lbC00DD74	move.l	D1,D0

	moveq	#$30,D1
	movea.l	A2,A0
	bra.b	lbC00E044

lbC00E042	move.b	D1,(A0)+
lbC00E044	subq.l	#1,D4
	bcc.b	lbC00E042
	move.l	($60,SP),D5
lbC00E04C	add.l	D5,D6
	subq.l	#1,D5
	bne	lbC00E14C
	move.b	(A2),D5
	cmp.b	($5A,SP),D5
	bne	lbC00E14C
	tst.l	($60,SP)
	bne	lbC00E14C
	clr.b	(A2)
	bra	lbC00E14C

lbC00E06C	move.l	($60,SP),D0
	bpl.b	lbC00E078
	moveq	#8,D0
	move.l	D0,($60,SP)
lbC00E078	move.b	#1,($5C,SP)
lbC00E07E	moveq	#2,D0
	cmp.b	($5C,SP),D0
	bne.b	lbC00E092
	movea.l	(A3),A0
	addq.l	#4,(A3)
	move.l	(A0),D0
	moveq	#0,D4
	move.w	D0,D4
	bra.b	lbC00E098

lbC00E092	movea.l	(A3),A0
	addq.l	#4,(A3)
	move.l	(A0),D4
lbC00E098	move.b	($20,SP),D0
	moveq	#$75,D1
	cmp.b	D1,D0
	beq	lbC00E006
	moveq	#$6F,D1
	cmp.b	D1,D0
	bne.b	lbC00E0C8
	tst.b	($64,SP)
	beq.b	lbC00E0BC
	lea	($33,SP),A2
	move.b	#$30,($32,SP)
	moveq	#1,D6
lbC00E0BC	move.l	D4,D0
	movea.l	A2,A0
lbC00DB16	link.w	A5,#-12
	movea.l	SP,A1
lbC00DB1C	move.l	D0,D1
	andi.w	#7,D1
	addi.w	#$30,D1
	move.b	D1,(A1)+
	lsr.l	#3,D0
	bne.b	lbC00DB1C
	move.l	A1,D0
lbC00DB2E	move.b	-(A1),(A0)+
	cmpa.l	A1,SP
	bne.b	lbC00DB2E
	clr.b	(A0)
	sub.l	SP,D0
	unlk	A5
	bra	lbC00E00E

lbC00E0C8	tst.b	($64,SP)
	beq.b	lbC00E0E0
	move.b	#$30,($32,SP)
	lea	($34,SP),A2
	move.b	#$78,($33,SP)
	moveq	#2,D6
lbC00E0E0	move.l	D4,D0
	movea.l	A2,A0
lbC00DB54	subq.l	#8,SP
	movea.l	SP,A1
lbC00DB58	move.w	D0,D1
	andi.w	#15,D1
	move.b	(abcdef.MSG,PC,D1.W),(A1)+
	lsr.l	#4,D0
	bne.b	lbC00DB58
	move.l	A1,D0
lbC00DB68	move.b	-(A1),(A0)+
	cmpa.l	A1,SP
	bne.b	lbC00DB68
	clr.b	(A0)
	sub.l	A1,D0
	addq.l	#8,SP

	move.l	D0,D5
	btst	#5,($20,SP)
	bne	lbC00E010
	lea	($32,SP),A1
lbC00DBFC	move.l	A3,-(SP)
	movea.l	A1,A3
	lea	(lbB00F25D,pc),A0
	moveq	#0,D1
	moveq	#$20,D0
	bra.b	lbC00DC24

lbC00DC04	bmi	\1
	btst	#1,(A0,D1.L)
	beq.b	\1
	sub.b	D0,D1
\1	move.b	D1,(A3)+
lbC00DC24	move.b	(A3),d1
	bne.b	lbC00DC04
	move.l	A1,D0
	movea.l	(SP)+,A3
	bra	lbC00E010

lbC00E100	movea.l	(A3),A0
	addq.l	#4,(A3)
	movea.l	(A0),A3
	move.l	A3,D0
	bne.b	lbC00E114
	moveq	#-1,D0
	lea	(NULLPOINTER.MSG,PC),A3
	move.l	D0,($60,SP)
lbC00E114	movea.l	A3,A0
lbC00E116	tst.b	(A0)+
	bne.b	lbC00E116
	subq.l	#1,A0
	suba.l	A3,A0
	move.l	A0,D5
	move.l	($60,SP),D0
	bmi.b	lbC00E12A
	cmp.l	D5,D0
	ble.b	lbC00E12E
lbC00E12A	move.l	D5,($60,SP)
lbC00E12E	move.l	($60,SP),D6
	bra.b	lbC00E150

lbC00E134	movea.l	(A3),A0
	moveq	#1,D6
	addq.l	#4,(A3)
	move.l	(A0),D0
	move.b	D0,($32,SP)
	clr.b	($33,SP)
	bra.b	lbC00E14C

lbC00E146	moveq	#0,D0
	bra	lbC00E1FA

lbC00E14C	lea	($32,SP),A3
lbC00E150	move.l	($4C,SP),D0
	cmp.l	D6,D0
	bge.b	lbC00E160
	moveq	#0,D1
	move.l	D1,($4C,SP)
	bra.b	lbC00E164

lbC00E160	sub.l	D6,($4C,SP)
lbC00E164	movea.l	($6C,SP),A2
	tst.b	($5B,SP)
	beq.b	lbC00E18C
	bra.b	lbC00E178

lbC00E170	movea.l	A5,A0
	moveq	#0,D0
	move.b	(A3)+,D0
	jsr	(A2)
lbC00E178	subq.l	#1,D6
	bge.b	lbC00E170
	bra.b	lbC00E184

lbC00E17E	movea.l	A5,A0
	moveq	#$20,D0
	jsr	(A2)
lbC00E184	subq.l	#1,($4C,SP)
	bge.b	lbC00E17E
	bra.b	lbC00E1F6

lbC00E18C	tst.l	D7
	bne.b	lbC00E19C
	tst.b	($47,SP)
	bne.b	lbC00E19C
	tst.b	($46,SP)
	beq.b	lbC00E1E2
lbC00E19C	move.b	(A3),D0
	moveq	#$20,D1
	cmp.b	D1,D0
	beq.b	lbC00E1B0
	moveq	#$2B,D2
	cmp.b	D2,D0
	beq.b	lbC00E1B0
	moveq	#$2D,D2
	cmp.b	D2,D0
	bne.b	lbC00E1E2
lbC00E1B0	tst.l	D6
	bmi.b	lbC00E1E2
	cmp.b	($5A,SP),D1
	bne.b	lbC00E1CC
	bra.b	lbC00E1C6

lbC00E1BC	movea.l	A5,A0
	moveq	#0,D0
	move.b	($5A,SP),D0
	jsr	(A2)
lbC00E1C6	subq.l	#1,($4C,SP)
	bge.b	lbC00E1BC
lbC00E1CC	movea.l	A5,A0
	moveq	#0,D0
	move.b	(A3)+,D0
	jsr	(A2)
	subq.l	#1,D6
	bra.b	lbC00E1E2

lbC00E1D8	movea.l	A5,A0
	moveq	#0,D0
	move.b	($5A,SP),D0
	jsr	(A2)
lbC00E1E2	subq.l	#1,($4C,SP)
	bge.b	lbC00E1D8
	bra.b	lbC00E1F2

lbC00E1EA	movea.l	A5,A0
	moveq	#0,D0
	move.b	(A3)+,D0
	jsr	(A2)
lbC00E1F2	subq.l	#1,D6
	bge.b	lbC00E1EA
lbC00E1F6	move.l	($22,SP),D0
lbC00E1FA	movem.l	(SP)+,D2/D4-D7/A2/A3/A5
	adda.w	#$48,SP
	rts

lbC00E218	subq.l	#4,SP
	movem.l	D7/A3/A5,-(SP)
	move.l	($18,SP),(12,SP)
	movea.l	A1,A3
	movea.l	A0,A5
	bra.b	\4

\1	moveq	#$25,D0
	cmp.b	D0,D7
	bne.b	\3
	movea.l	($14,SP),A0
	cmp.b	(A0),D0
	bne.b	\2
	addq.l	#1,($14,SP)
	bra.b	\3

\2	move.l	A3,-(SP)
	move.l	A5,-(SP)
	lea	($14,SP),A1
	bsr	lbC00DDB2
	addq.l	#8,SP
	tst.l	D0
	beq.b	\3
	move.l	D0,($14,SP)
	bra.b	\4

\3	movea.l	A3,A0
	moveq	#0,D0
	move.b	D7,D0
	jsr	(A5)
\4	movea.l	($14,SP),A0
	move.b	(A0)+,D7
	move.l	A0,($14,SP)
	tst.b	D7
	bne.b	\1
	movem.l	(SP)+,D7/A3/A5
	addq.l	#4,SP
	rts
	endc

	IFND	rom

lbC00E2A4	cmpi.w	#39,(20,A6)
	bcs.b	\1
	sysj	CreatePool

\1	suba.l	A0,A0
	cmp.l	D2,D1
	bcs.b	\2
	move.l	D0,-(SP)
	moveq	#7,D0
	add.l	D0,D1
	not.b	D0
	and.b	D0,D1
	move.l	D1,-(SP)
	moveq	#MEMF_ANY,D1
	moveq	#$18,D0
	sys	AllocMem
	move.l	(SP)+,D1
	movea.l	(SP)+,A0
	tst.l	D0
	beq.b	lbC00E2EA
	exg	D0,A0
	move.l	A0,(8,A0)
	addq.l	#4,A0
	clr.l	(A0)
	move.l	A0,-(A0)
	lea	(12,A0),A1
	move.l	D0,(A1)+
	move.l	D1,(A1)+
	move.l	D2,(A1)+
\2	move.l	A0,D0
lbC00E2EA	rts

lbC00E2EC	cmpi.w	#39,(20,A6)
	bcs.b	\1
	sysj	DeletePool

\1	move.l	A0,D0
	beq.b	lbC00E2EA
	movem.l	D2/A2,-(SP)
	movea.l	(A0),A2
\2	move.l	(A2),D2
	beq.b	\3
	movea.l	A2,A1
	movea.l	D2,A2
	move.l	-(A1),D0
	sys	FreeMem
	bra.b	\2

\3	movea.l	A2,A1
	subq.l	#4,A1
	movem.l	(SP)+,D2/A2
	moveq	#$18,D0
	sysj	FreeMem

lbC009364	cmpi.w	#39,(20,A6)
	bcs.b	\1
	sysj	AllocPooled

\1	move.l	D0,D1
	beq.b	lbC00E38A
	move.l	A0,D0
	beq.b	lbC00E38A
	movem.l	D2/D3/A2/A3,-(SP)
	move.l	D1,D2
	movea.l	A0,A2
	cmp.l	($14,A2),D2
	bcc.b	lbC00E38C
lbC00E342	move.l	(A2),D3
lbC00E344	movea.l	D3,A3
	move.l	(A3),D3
	beq.b	lbC00E3BC
	movea.l	A3,A0
	tst.l	(8,A0)
	beq.b	lbC00E3BC
	move.l	D2,D0
	jsr	(-$BA,A6)
	tst.l	D0
	beq.b	lbC00E344
	movea.l	D0,A0
	move.l	(12,A2),D0
	btst	#$10,D0
	beq.b	lbC00E384
	movea.l	A0,A1
	move.l	D2,D1
	addq.l	#7,D1
	lsr.l	#3,D1
	subq.l	#1,D1
	move.w	D1,D0
	swap	D1
	moveq	#0,D3
lbC00E378	move.l	D3,(A1)+
	move.l	D3,(A1)+
	dbra	D0,lbC00E378
	dbra	D1,lbC00E378
lbC00E384	move.l	A0,D0
lbC00E386	movem.l	(SP)+,D2/D3/A2/A3
lbC00E38A	rts

lbC00E38C	move.l	D2,D0
	addq.l	#8,D0
	addq.l	#4,D0
	move.l	(12,A2),D1
	bsr	lbC00E42C
	tst.l	D0
	beq.b	lbC00E386
	movea.l	D0,A1
	movea.l	A2,A0
	addq.l	#4,A0
	move.l	(4,A0),D0
	move.l	A1,(4,A0)
	exg	D0,A0
	movem.l	D0/A0,(A1)
	move.l	A1,(A0)
	addq.l	#8,A1
	clr.l	(A1)+
	move.l	A1,D0
	bra.b	lbC00E386

lbC00E3BC	move.l	(12,A2),D1
	move.l	($10,A2),D0
	addi.l	#$24,D0
	bsr.b	lbC00E42C
	tst.l	D0
	beq.b	lbC00E386
	movea.l	D0,A3
	movea.l	D0,A1
	movea.l	A2,A0
	move.l	(A0),D0
	move.l	A1,(A0)
	movem.l	D0/A0,(A1)
	movea.l	D0,A0
	move.l	A1,(4,A0)
	moveq	#10,D0
	move.b	D0,(8,A3)
	move.b	D0,(9,A3)
	lea	(Pool.MSG,PC),A0
	move.l	A0,(10,A3)
	move.l	(12,A2),D1
	move.w	D1,(14,A3)
	lea	($24,A3),A0
	moveq	#7,D1
	not.l	D1
	move.l	A0,D0
	and.l	D0,D1
	movea.l	D1,A0
	move.l	($10,A2),D0
	move.l	A0,($10,A3)
	move.l	A0,($14,A3)
	move.l	D0,($1C,A3)
	clr.l	(A0)
	move.l	D0,(4,A0)
	adda.l	D0,A0
	move.l	A0,($18,A3)
	bra	lbC00E342

lbC00E42C	addq.l	#4,D0
	move.l	D0,-(SP)
	sys	AllocMem
	tst.l	D0
	beq.b	lbC00E43E
	movea.l	D0,A0
	move.l	(SP),(A0)+
	move.l	A0,D0
lbC00E43E	addq.l	#4,SP
	rts

lbC009368	cmpi.w	#39,(20,A6)
	bcs.b	lbC00E44E
	sys	FreePooled
	rts

lbC00E44E	move.l	A0,D1
	beq.b	lbC00E4B0
	move.l	A1,D1
	beq.b	lbC00E4B0
	cmp.l	($14,A0),D0
	bcc.b	lbC00E4B2
	move.l	A3,-(SP)
	move.l	(A0),D1
lbC00E460	movea.l	D1,A3
	move.l	(A3),D1
	beq.b	lbC00E4CA
	tst.l	(8,A3)
	beq.b	lbC00E4CA
	cmpa.l	($14,A3),A1
	bcs.b	lbC00E460
	cmpa.l	($18,A3),A1
	bcc.b	lbC00E460
	movea.l	A3,A0
	jsr	(-$C0,A6)
	movea.l	(4,A3),A1
	move.l	(4,A1),D1
	beq.b	lbC00E49E
	movea.l	D1,A0
	move.l	A3,(A0)
	move.l	A0,(4,A3)
	move.l	A3,(4,A1)
	movea.l	(A3),A0
	move.l	A0,(A1)
	move.l	A1,(4,A0)
	move.l	A1,(A3)
lbC00E49E	movea.l	A3,A1
	movea.l	(SP)+,A3
	move.l	($20,A1),D0
	add.l	($14,A1),D0
	sub.l	($18,A1),D0
	beq.b	lbC00E4B6
lbC00E4B0	rts

lbC00E4B2	subq.l	#4,A1
	subq.l	#8,A1
lbC00E4B6	move.l	A1,D0
	movea.l	(A1)+,A0
	movea.l	(A1),A1
	move.l	A0,(A1)
	move.l	A1,(4,A0)
	movea.l	D0,A1
	move.l	-(A1),D0
	sysj	FreeMem

lbC00E4CA	movea.l	(SP)+,A3
	move.l	D7,-(SP)
	move.l	#$100000F,D7
	sys	Alert
	move.l	(SP)+,D7
	rts

	ENDC

lbL00E53C	dc.l	0	;normal icon image
	dc.l	$800
	dc.l	$8000000
	dc.l	$210800
	dc.l	$8000000
	dc.l	$210800
	dc.l	$7FFFFFFF
	dc.l	$FFFFF800
	dc.l	$20000000
	dc.l	$800
	dc.l	$20000000
	dc.l	$800
	dc.l	$20000000
	dc.l	$800
	dc.l	$20000000
	dc.l	$800
	dc.l	$20000040
	dc.l	$800
	dc.l	$200000A0
	dc.l	$800
	dc.l	$200000E0
	dc.l	$800
	dc.l	$20000110
	dc.l	$800
	dc.l	$2000E1B0
	dc.l	$E0000800
	dc.l	$2081F1B1
	dc.l	$F0200800
	dc.l	$20B0FB1B
	dc.l	$E1A00800
	dc.l	$206B3F5F
	dc.l	$9AC00800
	dc.l	$203DC554
	dc.l	$77800800
	dc.l	$201EBF1F
	dc.l	$AF000800
	dc.l	$200FB6ED
	dc.l	$BE000800
	dc.l	$20074AEA
	dc.l	$5C000800
	dc.l	$2007A71C
	dc.l	$BC000800
	dc.l	$20010802
	dc.l	$10000800
	dc.l	$2003F249
	dc.l	$F8000800
	dc.l	$2007FFFF
	dc.l	$FC000800
	dc.l	$200A8802
	dc.l	$2A002800
	dc.l	$200C2802
	dc.l	$86005800
	dc.l	$200A8000
	dc.l	$2E007800
	dc.l	$2007FFFF
	dc.l	$FC000800
	dc.l	$20000000
	dc.l	$5800
	dc.l	$20000000
	dc.l	$2800
	dc.l	$FFFFFFFF
	dc.l	$FFFFF800
	dc.l	$FFFFFFFF
	dc.l	$FFFFF000
	dc.l	$84000000
	dc.l	$108000
	dc.l	$84000000
	dc.l	$108000
	dc.l	$80000000
	dc.l	0
	dc.l	$80000000
	dc.l	$8000
	dc.l	$80000040
	dc.l	$8000
	dc.l	$800000E0
	dc.l	$8000
	dc.l	$80000040
	dc.l	$8000
	dc.l	$80000040
	dc.l	$8000
	dc.l	$800000E0
	dc.l	$8000
	dc.l	$800080E0
	dc.l	$20008000
	dc.l	$8101C1F0
	dc.l	$70108000
	dc.l	$8380A0E0
	dc.l	$A0388000
	dc.l	$81F150E1
	dc.l	$51F08000
	dc.l	$807FA9F2
	dc.l	$BFC08000
	dc.l	$807FD5F5
	dc.l	$7FC08000
	dc.l	$802FFBFB
	dc.l	$FE808000
	dc.l	$8017E7FC
	dc.l	$FC008000
	dc.l	$800DDFFF
	dc.l	$74008000
	dc.l	$8003FFFF
	dc.l	$F8008000
	dc.l	$8002FFFF
	dc.l	$E8008000
	dc.l	$8003FFFF
	dc.l	$F8008000
	dc.l	$80016DF6
	dc.l	$D0008000
	dc.l	$800091F8
	dc.l	$8000F000
	dc.l	$8007FFFF
	dc.l	$FC008000
	dc.l	$8007FFFF
	dc.l	$FC008000
	dc.l	$8007FFFF
	dc.l	$D0008000
	dc.l	$80009BBB
	dc.l	$2000F000
	dc.l	$80000000
	dc.l	$8000
	dc.l	$80000000
	dc.l	$8000
	dc.l	0
	dc.l	0

* alternate icon image (font: monaco/9)

lbL00E72C	dc.l	0
	dc.l	$800
	dc.l	$7BFFFFFF
	dc.l	$FFEF7800
	dc.l	$7BFFFFFF
	dc.l	$FFEF7800
	dc.l	$7FFFFFFF
	dc.l	$FFFFF800
	dc.l	$60065C00
	dc.l	$7800
	dc.l	$6607BE00
	dc.l	$7800
	dc.l	$61468C80
	dc.l	$7800
	dc.l	$667DEFA0
	dc.l	$7800
	dc.l	$605B07F0
	dc.l	$7800
	dc.l	$638BCE30
	dc.l	$7800
	dc.l	$61A73C20
	dc.l	$7800
	dc.l	$60D58AE0
	dc.l	$7800
	dc.l	$60EF1AC0
	dc.l	$7800
	dc.l	$7FAC2D80
	dc.l	$7800
	dc.l	$7FE04300
	dc.l	$7800
	dc.l	$7EB98600
	dc.l	$7800
	dc.l	$71B18400
	dc.l	$7800
	dc.l	$75870800
	dc.l	$7800
	dc.l	$7E561000
	dc.l	$7800
	dc.l	$7A5C2000
	dc.l	$7800
	dc.l	$7FBE4000
	dc.l	$7800
	dc.l	$7FF08018
	dc.l	$3F007800
	dc.l	$66E56338
	dc.l	$3007800
	dc.l	$60C26318
	dc.l	$3000800
	dc.l	$60CE3618
	dc.l	$6007800
	dc.l	$609C3618
	dc.l	$C007800
	dc.l	$60D81C18
	dc.l	$C007800
	dc.l	$60501C19
	dc.l	$8C000800
	dc.l	$60000000
	dc.l	$7800
	dc.l	$60000000
	dc.l	$7800
	dc.l	$FFFFFFFF
	dc.l	$FFFFF800
	dc.l	$FFFFFFFF
	dc.l	$FFFFF000
	dc.l	$F7FFFFFF
	dc.l	$FFDEF000
	dc.l	$F7FFFFFF
	dc.l	$FFDEF000
	dc.l	$80000000
	dc.l	0
	dc.l	$D807F000
	dc.l	$F000
	dc.l	$D600FC00
	dc.l	$F000
	dc.l	$C3C7F200
	dc.l	$F000
	dc.l	$C7C63E40
	dc.l	$F000
	dc.l	$C3F6FAE0
	dc.l	$F000
	dc.l	$C3FFFDC2
	dc.l	$F000
	dc.l	$C0FFEFC1
	dc.l	$F000
	dc.l	$C0FFF748
	dc.l	$F000
	dc.l	$C07FEF44
	dc.l	$2000F000
	dc.l	$DFFFFE01
	dc.l	$F000
	dc.l	$C67FFF20
	dc.l	$F000
	dc.l	$C17FF800
	dc.l	$400F000
	dc.l	$DFFFFC00
	dc.l	$F000
	dc.l	$DF79F800
	dc.l	$F000
	dc.l	$DFF9E000
	dc.l	$F000
	dc.l	$DFE3E000
	dc.l	$F000
	dc.l	$DAEFC000
	dc.l	$F000
	dc.l	$CAAF0000
	dc.l	$F000
	dc.l	$C1FF0000
	dc.l	$F000
	dc.l	$C03C0000
	dc.l	$F000
	dc.l	$C0780000
	dc.l	$D000
	dc.l	$C0740000
	dc.l	$A000
	dc.l	$C0200000
	dc.l	$8000
	dc.l	$C0000000
	dc.l	$F000
	dc.l	$C0000000
	dc.l	$A000
	dc.l	$C0000000
	dc.l	$D000
	dc.l	0
	dc.l	0

	dc.b	'$VER: '
KingCONhandle.MSG	dc.b	'KingCON-handler (68020+) '

	IFND	test
	$VER
	ELSE
	dc.b	'TEST'
	ENDC

	dc.b	' ('
	IFND	test
	fdate
	ELSE
	mydate
	ENDC

	dc.b	')',10,13,0

RAMRADVD0SD0.MSG	dc.b	'~RAM RAD VD0 SD0',0
d.MSG	dc.b	'%ld',0
ascii.MSG	dc.b	'1;',0
ascii.MSG0	dc.b	'2;',0
ascii.MSG1	dc.b	'3;',0
ascii.MSG2	dc.b	'4;',0
ascii.MSG3	dc.b	'7;',0
ascii.MSG4	dc.b	'8;',0
ddm.MSG	dc.b	'%ld;%ldm',0

***************************** GADGETS ****************************************
	dc.b	25
OK.MSG	dc.b	'OK',0
	dc.b	26
Cancel.MSG	dc.b	'Cancel',0
	dc.b	27
gIconify	dc.b	'Iconify',0
	dc.b	28
gMinimize	dc.b	'Minimize',0
	dc.b	29
gJump	dc.b	'Jump',0

***************************** MENUS ******************************************
	dc.b	50
Console.MSG	dc.b	'Console',0

	dc.b	51,'Z'
Clearwindow.msg	dc.b	'Clear window',0

	dc.b	52
Reset.MSG	dc.b	'Reset',0

	dc.b	53,'J'
Jumpscroll.MSG	dc.b	'Jump scroll',0

	dc.b	54,'I'
Iconify.MSG	dc.b	'Iconify',0

	dc.b	55,'I'
Minimize.MSG	dc.b	'Minimize',0

	dc.b	56,'N'
Normalize.MSG	dc.b	'Normalize',0

	dc.b	57,'A'
Maximize.MSG	dc.b	'Maximize',0

	dc.b	58,'S'
Nextscreen.MSG	dc.b	'Next screen',0

	dc.b	59
Gotoscreen.MSG	dc.b	'Goto screen...',0

	dc.b	60,'H'
Halt.MSG	dc.b	'Halt',0

	dc.b	61,'R'
Resume.MSG	dc.b	'Resume',0

	dc.b	62,'K'
_keep	dc.b	'Keep closed',0

	dc.b	63,'?'
About.MSG	dc.b	'About...',0

	dc.b	64,'Q'
Close.MSG	dc.b	'Close',0

	dc.b	65
Complete.MSG	dc.b	'Complete',0

	dc.b	66,'F'
Filename.MSG	dc.b	'Filename',0

	dc.b	67,'M'
Command.MSG	dc.b	'Command',0

	dc.b	68,'D'
Device.MSG	dc.b	'Device',0

	dc.b	69
Enablecache.MSG	dc.b	'Enable cache',0

	dc.b	70
Resetcache.MSG	dc.b	'Reset cache',0

	dc.b	71
Purgecache.MSG	dc.b	'Purge cache',0

	dc.b	72,'.'
Showinfo.MSG	dc.b	'Show .info',0

	dc.b	73
Review.MSG	dc.b	'Review',0

	dc.b	74,'W'
Enabled.MSG	dc.b	'Enabled',0

	dc.b	75,'B'
Clearbuffer.MSG	dc.b	'Clear buffer',0

	dc.b	76
Saveplaintext.MSG	dc.b	'Save plain text as...',0

	dc.b	77
Savewithstyle.MSG	dc.b	'Save with styles as...',0

	dc.b	78
History.MSG	dc.b	'History',0

	dc.b	79
Enabled.MSG2	dc.b	'Enabled',0

	dc.b	80
Clearbuffer.MSG2	dc.b	'Clear buffer'

***************************** OTHER STRINGS **********************************
	dc.b	0
about	dc.b	'About KingCON',0

	dc.b	1
fixes	dc.b	'Fixes',0

	dc.b	2
status	dc.b	'The handler and related files',10
	dc.b	'are freely distributable',0

	dc.b	3
mess	dc.b	'KingCON message',0

	dc.b	4
iwarn	dc.b	'Iconifying the window now',10
	dc.b	'COULD be dangerous.',10,10
	dc.b	'Continue anyway?',0

	dc.b	5
swarn	dc.b	'Jumping to another screen now',10
	dc.b	'COULD be dangerous',10,10
	dc.b	'Continue anyway?',0

	dc.b	6
SelectPublics.MSG	dc.b	'Select Public screen',0


_about	dc.b	'KingCON (68020) '

	IFND	test
	$VER
	ELSE
	dc.b	'TEST - '
	mydate
	ENDC

_cr	dc.b	10,10
	dc.b	'Copyright © 1993,1994 David Larsson',10
	dc.b	'%s © 2000-2005 Mikolaj Calusinski',10,10
	dc.b	'%s',0

igadg	dc.b	'%s|'
sgadg	dc.b	'%s|%s',0

lbL00F1B0	dc.l	GA_ID
	dc.l	$80000001
	dc.l	0
lbL00F1C0	dc.l	GA_ID
	dc.l	$80000002
	dc.l	0

lbL00F234	dc.l	0
	dc.l	$35001F
	dc.w	2
lbL00F23E	dc.l	lbL00E53C
	dc.l	$3000000
	dc.w	0
lbL00F248	dc.l	0
	dc.l	$35001F
	dc.w	2
lbL00F252	dc.l	lbL00E72C
	dc.l	$3000000
	dc.w	0

lbB00F25D	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$28
	dc.b	$28
	dc.b	$28
	dc.b	$28
	dc.b	$28
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$20
	dc.b	$48
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$84
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$81
	dc.b	$81
	dc.b	$81
	dc.b	$81
	dc.b	$81
	dc.b	$81
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	1
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$82
	dc.b	$82
	dc.b	$82
	dc.b	$82
	dc.b	$82
	dc.b	$82
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	2
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$10
	dc.b	$20
	even
_end	end

----------------------------------------------------------------------------
Flags:
----------------------------------------------------------------------------
$160 [L]

$163 [B]
00 {00} - [S]: AUTO\AUTOICONIFY		[C]: NOAUTO
01 {01} - [S]: CLOSE\AUTOICONIFY	[C]: NOCLOSE
02 {02} - [S]: BACKDROP			[C]: NOBACKDROP
03 {03} - [S]: NOBORDER			[C]: BORDER
04 {04} - [S]: NODRAG			[C]: DRAG
05 {05} - [S]: NOSIZE			[C]: SIZE
06 {06} - [S]: SIMPLE			[C]: SMART
07 {07} - [S]: WAIT			[C]: NOWAIT

$162 [B]
08 {00} - [S]: MINI			[C]: MAXI
09 {01} - [S]: RAW mode			[C]: CON mode
10 {02} - [S]: NOREVIEW\PLAIN		[C]: REVIEW
11 {03} - [S]: (menu: disable review)
12 {04} - [S]: NOFNC\PLAIN		[C]: FNC
13 {05} - [S]: NOMENUS\PLAIN		[C]: MENUS
14 {06} - [S]: (menu: show .info)
15 {07} - [S]: (menu: disable history)

$161 [B]
16 {00} - [S]: NOSTYLES			[C]: STYLES
17 {01} - [S]: INACTIVE			[C]: ACTIVE
18 {02} - [S]: JUMP			[C]: NOJUMP
19 {03} - [S]:
20 {04} - [S]: NOGADS\PLAIN		[C]: GADS
21 {05} - [S]: ASYNC			[C]: SYNC
22 {06} - [S]: SHOWDIR			[C]: NOSHOWDIR
23 {07} - [S]: AUTOICONIFY		[C]: NOAUTO

$160 [B]
24 {00} - [S]: NOICONIFY		[C]: ICONIFY
25 {01} - [S]:
26 {02} - [S]: KEEPCLOSED		[C]: NOKEEPCLOSED
27 {03} - [S]: POPSCREEN		[C]: NOPOPSCREEN
28 {04} - [S]: 
29 {05} - [S]: FASTUPDATE		[C]: NICEUPDATE
30 {06} - [S]: MENUFY			[C]: NOMENUFY
31 {07} - [S]: 

$164 [L]

$167 [B]
00 {00} - [S]: 				[C]: NODIRCACHE
01 {01} - [S]: (menu: enable cache)	[C]: NODIRCACHE
02 {02} - [S]: 
03 {03} - [S]: 
04 {04} - [S]: NOSHORTCUTS		[C]: SHORTCUTS
05 {05} - [S]: 
06 {06} - [S]: 
07 {07} - [S]: 
