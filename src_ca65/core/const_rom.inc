; =====================================================================
; x16clib :: core/const_rom.inc -- ROM banks and their $C000 entry points
; =====================================================================
; Pure symbol file. Safe to .include any number of times.
;
; The audio and graphics APIs are NOT in the $FFxx KERNAL table. They
; live at $C000+ inside their own ROM bank and must be reached with
; jsrfar (or rom_call_fast). See core/macros.inc.
;
; Transcribed from x16-rom-r49/inc/banks.inc, audio.inc, graphics.inc.
; =====================================================================

.ifndef X16_CONST_ROM_INC
X16_CONST_ROM_INC = 1

BANK_KERNAL  = $00
BANK_KEYBD   = $01
BANK_CBDOS   = $02
BANK_FAT32   = $03
BANK_BASIC   = $04
BANK_MONITOR = $05
BANK_CHARSET = $06
BANK_DIAG    = $07
BANK_GRAPH   = $08
BANK_DEMO    = $09
BANK_AUDIO   = $0A
BANK_UTIL    = $0B
BANK_BANNEX  = $0C
BANK_X16EDIT = $0D          ; occupies two banks
BANK_BASLOAD = $0F

; ---------------------------------------------------------------------
; BANK_AUDIO entry points (x16-rom-r49/inc/audio.inc).
;
; ym_* / psg_* keep the ROM driver's volume and pan shadows coherent.
; Writing YM_REG/YM_DATA directly does not -- that is the AUDIOYM.TXT
; distinction between FMPOKE (via ROM) and YM! (raw).
; ---------------------------------------------------------------------
rom_bas_fmfreq          = $C000
rom_bas_fmnote          = $C003
rom_bas_fmplaystring    = $C006
rom_bas_fmvib           = $C009
rom_bas_playstringvoice = $C00C
rom_bas_psgfreq         = $C00F
rom_bas_psgnote         = $C012
rom_bas_psgwav          = $C015
rom_bas_psgplaystring   = $C018
rom_notecon_bas2fm      = $C01B
rom_notecon_bas2midi    = $C01E
rom_notecon_bas2psg     = $C021
rom_notecon_fm2bas      = $C024
rom_notecon_fm2midi     = $C027
rom_notecon_fm2psg      = $C02A
rom_notecon_freq2bas    = $C02D
rom_notecon_freq2fm     = $C030
rom_notecon_freq2midi   = $C033
rom_notecon_freq2psg    = $C036
rom_notecon_midi2bas    = $C039
rom_notecon_midi2fm     = $C03C
rom_notecon_midi2psg    = $C03F
rom_notecon_psg2bas     = $C042
rom_notecon_psg2fm      = $C045
rom_notecon_psg2midi    = $C048
rom_psg_init            = $C04B
rom_psg_playfreq        = $C04E
rom_psg_read            = $C051
rom_psg_setatten        = $C054
rom_psg_setfreq         = $C057
rom_psg_setpan          = $C05A
rom_psg_setvol          = $C05D
rom_psg_write           = $C060
rom_ym_init             = $C063
rom_ym_loaddefpatches   = $C066
rom_ym_loadpatch        = $C069
rom_ym_loadpatchlfn     = $C06C
rom_ym_playdrum         = $C06F
rom_ym_playnote         = $C072
rom_ym_setatten         = $C075
rom_ym_setdrum          = $C078
rom_ym_setnote          = $C07B
rom_ym_setpan           = $C07E
rom_ym_read             = $C081
rom_ym_release          = $C084
rom_ym_trigger          = $C087
rom_ym_write            = $C08A
rom_bas_fmchordstring   = $C08D
rom_bas_psgchordstring  = $C090
rom_psg_getatten        = $C093
rom_psg_getpan          = $C096
rom_ym_getatten         = $C099
rom_ym_getpan           = $C09C
rom_audio_init          = $C09F
rom_psg_write_fast      = $C0A2
rom_ym_get_chip_type    = $C0A5

; ---------------------------------------------------------------------
; BANK_BASIC floating-point jump table.
;
; The ROM ships a C128/C65-compatible FP library. Its jump table sits at
; $FE00 inside BANK_BASIC (cfg/basic-x16.cfgtpl: FPJMP start = $FE00)
; and is a stable ABI -- unlike the implementation addresses in
; basic.sym, which move between ROM revisions.
;
; 52 entries. The six after fp_poly are compiled out (`.if 0` in
; math/jumptab.s) and read back as $AA fill. Do not call them.
;
; Everything operates on FAC, the floating accumulator in zero page.
; Pointer arguments are A = low byte, Y = high byte.
;
; CAUTION: fp_fsub and fp_fdiv are the reverse of what the comments in
; jumptab.s claim. Each does `jsr conupk` (ARG = mem) and then falls into
; the ARG-first form, so what you actually get is
;       fp_fsub:  FAC = mem - FAC          (NOT FAC - mem)
;       fp_fdiv:  FAC = mem / FAC
;       fp_fsubt: FAC = ARG - FAC
;       fp_fdivt: FAC = ARG / FAC
; util/float.s wraps these back into the intuitive direction.
; ---------------------------------------------------------------------
fp_ayint  = $FE00       ; facmo:faclo = (s16)FAC, high byte first
fp_givayf = $FE03       ; FAC = (s16) A:Y        (A = high, Y = low)
fp_fout   = $FE06       ; FAC -> ASCIIZ at FP_FBUFFR; returns A/Y = ptr
fp_val    = $FE09       ; FAC = value of the string at X:Y, length A
fp_getadr = $FE0C       ; A:Y = (u16)FAC         (A = high, Y = low)
fp_floatc = $FE0F
fp_fsub   = $FE12       ; FAC = mem(A,Y) - FAC
fp_fsubt  = $FE15       ; FAC = ARG - FAC
fp_fadd   = $FE18       ; FAC = FAC + mem(A,Y)
fp_faddt  = $FE1B       ; FAC = FAC + ARG
fp_fmult  = $FE1E       ; FAC = FAC * mem(A,Y)
fp_fmultt = $FE21       ; FAC = FAC * ARG
fp_fdiv   = $FE24       ; FAC = mem(A,Y) / FAC
fp_fdivt  = $FE27       ; FAC = ARG / FAC
fp_log    = $FE2A       ; FAC = ln(FAC)
fp_int    = $FE2D       ; FAC = int(FAC)
fp_sqr    = $FE30       ; FAC = sqrt(FAC)
fp_negop  = $FE33       ; FAC = -FAC  (the real unary minus)
fp_fpwr   = $FE36       ; FAC = mem(A,Y) ^ FAC
fp_fpwrt  = $FE39       ; FAC = ARG ^ FAC
fp_exp    = $FE3C       ; FAC = e ^ FAC
fp_cos    = $FE3F       ; destroys ARG
fp_sin    = $FE42       ; destroys ARG
fp_tan    = $FE45       ; destroys ARG
fp_atn    = $FE48       ; destroys ARG
fp_round  = $FE4B
fp_abs    = $FE4E       ; FAC = |FAC|
fp_sign   = $FE51       ; A = sgn(FAC): $FF, 0 or 1
fp_fcomp  = $FE54       ; A = compare FAC with mem(A,Y): $FF, 0 or 1
fp_rnd    = $FE57
fp_conupk = $FE5A       ; ARG = mem(A,Y)
fp_movfm  = $FE60       ; FAC = mem(A,Y)
fp_movmf  = $FE66       ; mem(X,Y) = round(FAC)  (X = low, Y = high)
fp_movfa  = $FE69       ; FAC = ARG
fp_movaf  = $FE6C       ; ARG = round(FAC)
fp_faddh  = $FE6F       ; FAC += 0.5
fp_zerofc = $FE72       ; FAC = 0
fp_normal = $FE75
fp_negfac = $FE78       ; CAUTION: not a negate. Internal helper of the
                        ; add/subtract path: two's-complements the FAC
                        ; mantissa in place, denormalising a normal FAC.
                        ; Use fp_negop for -FAC.
fp_mul10  = $FE7B       ; FAC *= 10
fp_div10  = $FE7E       ; FAC /= 10
fp_movef  = $FE81       ; ARG = FAC
fp_sgn    = $FE84       ; FAC = sgn(FAC)
fp_float  = $FE87       ; FAC = (s8)A -- SIGNED: 200 comes out -56.
                        ; For an unsigned byte go through fp_givayf
                        ; with a zero high byte (util/float.s does).
fp_floats = $FE8A       ; FAC = (s16) facho:facho+1
fp_qint   = $FE8D       ; facho..faclo = (u32)FAC, most significant first
fp_finlog = $FE90       ; FAC += (s8)A
fp_foutc  = $FE93
fp_polyx  = $FE96
fp_poly   = $FE99

; ---------------------------------------------------------------------
; The floating accumulator and argument, in BASIC's zero page.
;
; A float packed in memory is 5 bytes; unpacked in FAC it is 6, with the
; sign broken out into its own byte. These live at $C3-$D1, above cc65's
; ZEROPAGE window ($22-$7F), so there is no conflict. Safe to disturb
; from a SYSed program, because BASIC is dormant while it runs.
; ---------------------------------------------------------------------
FP_FAC    = $C3
FP_FACEXP = $C3
FP_FACHO  = $C4         ; mantissa, most significant byte
FP_FACMOH = $C5
FP_FACMO  = $C6
FP_FACLO  = $C7         ; mantissa, least significant byte
FP_FACSGN = $C8
FP_ARG    = $CA
FP_ARGEXP = $CA
FP_ARGSGN = $CF
FP_FACOV  = $D1
FP_FBUFFR = $0100       ; fp_fout writes its ASCIIZ result here

FP_SIZE = 5             ; bytes of a packed float in memory

; ---------------------------------------------------------------------
; BANK_GRAPH entry points (x16-rom-r49/inc/graphics.inc).
; Most of these are also reachable through the $FFxx stubs in
; core/const_kernal.inc, which is the preferred route.
; ---------------------------------------------------------------------
gr_GRAPH_clear                = $C000
gr_GRAPH_draw_image           = $C003
gr_GRAPH_draw_line            = $C006
gr_GRAPH_draw_oval            = $C009
gr_GRAPH_draw_rect            = $C00C
gr_GRAPH_init                 = $C00F
gr_GRAPH_move_rect            = $C012
gr_GRAPH_set_colors           = $C015
gr_GRAPH_set_window           = $C018
gr_GRAPH_get_char_size        = $C01B
gr_GRAPH_put_char             = $C01E
gr_GRAPH_set_font             = $C021
gr_font_init                  = $C024
gr_console_init               = $C027
gr_console_put_char           = $C02A
gr_console_get_char           = $C02D
gr_console_put_image          = $C030
gr_console_set_paging_message = $C033
gr_set_window_fullscreen      = $C036
gr_FB_init                    = $C039
gr_FB_get_info                = $C03C
gr_FB_set_palette             = $C03F
gr_default_palette            = $C063

.endif
