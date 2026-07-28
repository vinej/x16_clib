; =====================================================================
; x16clib :: ui/filepick.s -- a file browser on a panel
; =====================================================================
; A directory panel with a mouse and a keyboard: scrolling, descent into
; folders, and one question answered -- which file? The caller does the
; rest. It is the same browser in every program that opens it, which is
; the point: one set of keys, one look, one copy.
;
;       x16_fp_filter("*.bmx");
;       if (x16_fp_open() == X16_FPK_PICK) {
;           x16_fp_path(buf, sizeof buf);
;       }
;       x16_fp_close();
;
; THE FILTER is a list of patterns separated by ';':
;
;       "*.prg"             programs
;       "*.bmx;*.png"       either kind of picture
;       "*.*"               every file, whatever it is called
;
; Directories are always listed whatever the filter says, or there would
; be no way to reach the file you wanted. Matching folds case: a drive
; answers in ASCII and clearing bit 5 lands either case on the other.
;
; x16_fp_primary() sets a SECOND pattern for callers that list everything
; but can only act on some of it -- a launcher lists "*.*" with a primary
; of "*.prg", and anything else is marked [dat] and can be handed to a
; program rather than run.
;
; THE ACCESSORS COPY. The upstream module can be relocated into a RAM
; bank, and a banked module cannot lend a pointer into its bank: by the
; time the caller reads it, the bank is no longer mapped and the pointer
; names whatever is in the window instead. So x16_fp_path/_name/_dir take
; a destination and a size and copy the answer out. The C port keeps that
; contract -- it is the API every other toolchain port follows.
;
; THE ENTRY CACHE is 64 entries of 40 bytes in VRAM -- x16_fp_cache()
; says where, and $12000 is the default, clear of the text map at $1B000.
; VRAM rather than a RAM bank, and not by preference: a banked module
; cannot page a bank into the window it is executing from.
;
; SAVE-UNDER: a launcher repaints itself when the panel closes and does
; not care what was underneath. A spreadsheet does. x16_fp_saveunder()
; keeps the covered characters and colours elsewhere in VRAM and puts
; them back -- 5,712 bytes at 80 columns, $14000 by default.
;
; THE EDIT KEYS (n new folder, e rename, d delete, c copy, v paste) are
; compiled in unconditionally: ld65 links this module only into programs
; that call it, so nothing pays for the browser it does not open. The
; upstream X16_USE_FILEPICK_EDIT gate exists because ACME has no linker.
;
; ---------------------------------------------------------------------
; ca65 -t cx16 TRANSLATES CHARACTER LITERALS TO PETSCII. ACME did not.
;
; The KERNAL keyboard answers in PETSCII, where an unshifted letter is
; $41-$5A -- the codes ASCII uses for CAPITALS -- and a shifted one is
; $C1-$DA. Every letter the key loop compares against, every byte that
; goes to the drive, and every name byte compared against one coming
; back is therefore written as its explicit value here, exactly the
; bytes the upstream ACME source assembled. See storage/dos.s.
;
; Display-only strings (headings, tags, prompts) reach screen_blit's
; PETSCII fold, so they have to BE PETSCII. ca65 gets there by
; translating the literal; the llvm-mos assembler has no charmap and
; would emit ASCII, which folds to the wrong glyphs, so the bytes are
; spelled out here with the text beside them. Lower case is ASCII minus
; $20, upper case is ASCII plus $80 -- verified by assembling the same
; literal with ca65 -t cx16 and reading the object back.
; ---------------------------------------------------------------------
; DEPENDENCIES. dos (chdir/mkdir/rmdir/delete/rename) and screen
; (get_mode/charset) are imported from their modules. The directory
; walker, the text-map blitters, the mouse-bounds config, key_wait and
; the streamed-copy openers have no module in this tree yet, so they are
; carried here as file-local copies of the upstream routines; when
; dir/fileio/screen-blit modules are ported, these collapse into imports.
; =====================================================================

        .include        "macros.inc"
        .include        "x16zp.inc"


; Cross-module: the drive commands live in storage/dos.s...
; (import dropped: dos_chdir, dos_mkdir, dos_rmdir)
; (import dropped: dos_delete, dos_rename)
; ...the mode query and the charset in video/screen.s...
; (import dropped: screen_get_mode, screen_charset)
; ...and the mouse in input/input.s.
; (import dropped: mouse_get, mouse_hide)

        .globl  x16_fp_cache
        .globl  x16_fp_filter
        .globl  x16_fp_primary
        .globl  x16_fp_style
        .globl  x16_fp_heading
        .globl  x16_fp_footing
        .globl  x16_fp_saveunder
        .globl  x16_fp_charset
        .globl  x16_fp_start_dir
        .globl  x16_fp_open
        .globl  x16_fp_resume
        .globl  x16_fp_close
        .globl  x16_fp_redraw
        .globl  x16_fp_path
        .globl  x16_fp_name
        .globl  x16_fp_dir
        .globl  x16_fp_is_primary
        .globl  x16_fp_match
        .globl  x16_fp_panel_top
        .globl  x16_fp_panel_left
        .globl  x16_fp_panel_width
        .globl  x16_fp_panel_rows

; ---------------------------------------------------------------------
; The answers x16_fp_open() comes back with (mirrored in filepick.h).
; ---------------------------------------------------------------------
FPK_NONE   = 0                   ; cancelled: ESC, Run/Stop, or the x box
FPK_PICK   = 1                   ; a file was chosen: x16_fp_path has it
FPK_ALT    = 2                   ; the second gesture: right click, or 'a'
FPK_HERE   = 3                   ; 'h': this DIRECTORY, not a file in it

FPK_ESIZE  = 40                  ; one cache entry: type, then the name
FPK_ENAME  = 1
FPK_MAXENT = 64
FPK_PTOP   = 3                   ; the panel's first row
FPK_DBLCLK = 30                  ; jiffies: half a second
FPK_ACURSOR = $67                ; yellow on blue: the caret, inverse of
                                 ; the field it sits in
FPK_AEDIT  = $76                 ; blue on yellow: the one place the panel
                                 ; is asking rather than showing, and it
                                 ; has to be unmistakable. Deliberately
                                 ; not the caller's palette -- a prompt
                                 ; that blends in is a prompt nobody
                                 ; answers.

; The explicit bytes (see the charmap note above). The letters are what
; the KERNAL sends for an UNSHIFTED key, which is also what the drive
; speaks; a `cmp #'h'` under -t cx16 would compare against a byte the
; keyboard never sends.
CH_SPACE  = $20
CH_QUOTE  = $22
CH_DOLLAR = $24
CH_STAR   = $2A
CH_COMMA  = $2C
CH_DOT    = $2E
CH_SLASH  = $2F
CH_SEMI   = $3B
CH_A      = $41
CH_C      = $43
CH_D      = $44
CH_E      = $45
CH_H      = $48
CH_N      = $4E
CH_P      = $50
CH_R      = $52
CH_S      = $53
CH_U      = $55
CH_V      = $56
CH_W      = $57
CH_Y      = $59

KEY_STOP  = $03                  ; Run/Stop
KEY_ENTER = $0D
KEY_HOME  = $13
KEY_BKSP  = $14
KEY_ESC   = $1B
KEY_DOWN  = $11
KEY_UP    = $91

; What the private directory walker reports (upstream storage/dir.asm).
DIR_LFN = 3                     ; logical file: clear of fs_load's 1 and
                                ; of the command channel's 15

DIR_TYPE_NONE = 0               ; no name on the line: "BLOCKS FREE."
DIR_TYPE_PRG  = 1
DIR_TYPE_SEQ  = 2
DIR_TYPE_USR  = 3
DIR_TYPE_REL  = 4
DIR_TYPE_DIR  = 5
DIR_TYPE_HOST = 6               ; the header line naming the volume

        .section .text,"ax",@progbits

; =====================================================================
; C entry points -- configuration
; =====================================================================

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_cache(unsigned long vaddr)
;   Where the 2,560-byte entry cache lives in VRAM. Bit 16 picks the
;   VRAM bank; the default is $12000.
; ---------------------------------------------------------------------
x16_fp_cache:
        sta     fp_vram
        stx     fp_vram+1
        lda     mos8(__rc2)             ; bits 16-23; cc65 kept these in sreg
        and     #$01
        sta     fp_vramh
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_filter(const char *patterns)
;   A ';' list of "*.ext" patterns, NUL-terminated. NULL means "*.*".
;   The string is not copied: it must stay valid while the panel is up.
; ---------------------------------------------------------------------
x16_fp_filter:
        lda     mos8(__rc2)             ; a lone pointer never lands
        sta     fp_filt                 ; in A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     fp_filt+1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_primary(const char *patterns)
;   Which of the listed files the caller can act on itself. Anything
;   listed that does NOT match is marked [dat] in the panel, and
;   x16_fp_is_primary() reports which kind was chosen. NULL means "the
;   same as the filter".
; ---------------------------------------------------------------------
x16_fp_primary:
        lda     mos8(__rc2)             ; a lone pointer never lands
        sta     fp_prim                 ; in A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     fp_prim+1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_style(unsigned char panel, unsigned char bar,
;                                unsigned char sel)
;   Colour bytes (foreground | background << 4).
; ---------------------------------------------------------------------
x16_fp_style:
        pha                             ; A and X carry arguments that
        phx                             ; the loads below clobber
        lda     mos8(__rc2)
        sta     fp_asel                ; sel (rightmost arg, in A)
        plx
        pla
        sta     fp_apanel
        stx     fp_abar
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_heading(const char *text)
; void __fastcall__ x16_fp_footing(const char *text)
;   The text in front of the path on the header row, and the reminder
;   along the bottom. NULL restores the defaults.
; ---------------------------------------------------------------------
x16_fp_heading:
        lda     mos8(__rc2)             ; a lone pointer never lands
        sta     fp_head                 ; in A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     fp_head+1
        rts

x16_fp_footing:
        lda     mos8(__rc2)             ; a lone pointer never lands
        sta     fp_foot                 ; in A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     fp_foot+1
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_saveunder(unsigned char on, unsigned long vaddr)
;   Keep what the panel covers, and put it back on close. The copy lives
;   in VRAM too (the text map IS VRAM): 5,712 bytes at 80 columns,
;   $14000 by default. The address is stored either way; `on` decides.
; ---------------------------------------------------------------------
x16_fp_saveunder:
        pha                             ; on -- it arrives in A, and the
                                        ; loads below clobber it
        stx     fp_under                ; the long starts in X, because `on`
        lda     mos8(__rc2)             ; took A: bits 0-7, 8-15, then 16-23
        sta     fp_under+1
        lda     mos8(__rc3)
        and     #$01
        sta     fp_underh
        pla
        sta     fp_undon
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_charset(unsigned char charset)
;   The charset the panel is drawn in (3 = PET upper/lower, the
;   default). 255 leaves whatever the caller had: there is no way to ask
;   the KERNAL which charset is loaded, so the browser cannot put back
;   what it does not know.
; ---------------------------------------------------------------------
x16_fp_charset:
        sta     fp_chset
        rts

; ---------------------------------------------------------------------
; void __fastcall__ x16_fp_start_dir(const char *path)
;   Where the browser opens. NULL means "/".
; ---------------------------------------------------------------------
x16_fp_start_dir:
        lda     mos8(__rc2)             ; a lone pointer never lands
        sta     fp_startat                 ; in A/X -- it takes an __rc pair
        lda     mos8(__rc3)
        sta     fp_startat+1
        rts

; =====================================================================
; C entry points -- the session
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char x16_fp_open(void)
;   Put the panel up on the starting directory and run it. Returns
;   X16_FPK_NONE / _PICK / _ALT / _HERE.
;
; X16_FPK_HERE is for a caller that wants a PLACE rather than a file.
; The drive is left standing in that directory whatever the answer, so a
; bare filename written afterwards lands there and x16_fp_dir() names
; it. Without it ESC has to double as "use this one", and then there is
; no way left to mean "cancel".
; ---------------------------------------------------------------------
x16_fp_open:
        jsr     fp_open
        ldx     #0                      ; high byte, for int-promoting callers
        rts

; ---------------------------------------------------------------------
; unsigned char x16_fp_resume(void)
;   The same panel again, same directory, same selection: for a caller
;   that acted on an X16_FPK_ALT and wants the browser back.
; ---------------------------------------------------------------------
x16_fp_resume:
        jsr     fp_resume
        ldx     #0
        rts

; ---------------------------------------------------------------------
; void x16_fp_close(void)
;   Put back what the panel covered and hide the pointer. The DRIVE is
;   left in the directory that was being browsed: a caller that needs to
;   be somewhere else should say so with x16_dos_chdir().
; ---------------------------------------------------------------------
x16_fp_close:
fp_close:
        jsr     filepick_restore_under
        jmp     mouse_hide

; ---------------------------------------------------------------------
; void x16_fp_redraw(void)
;   Paint the panel again, after a caller has drawn over it.
; ---------------------------------------------------------------------
x16_fp_redraw:
        jmp     filepick_draw

; =====================================================================
; C entry points -- what the caller reads back
; =====================================================================

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fp_path(char *dest, unsigned char size)
; unsigned char __fastcall__ x16_fp_name(char *dest, unsigned char size)
; unsigned char __fastcall__ x16_fp_dir (char *dest, unsigned char size)
;   The absolute path of the chosen entry / just its name / the
;   directory being browsed, COPIED into the caller's buffer, always
;   NUL-terminated. Returns how many characters were copied, terminator
;   aside. `size` counts the terminator, so a size of 0 copies nothing.
; ---------------------------------------------------------------------
x16_fp_path:
        jsr     copy_marshal
        jsr     fp_copy_path
        ldx     #0
        rts

x16_fp_name:
        jsr     copy_marshal
        jsr     fp_copy_name
        ldx     #0
        rts

x16_fp_dir:
        jsr     copy_marshal
        jsr     fp_copy_dir
        ldx     #0
        rts

; in:  A = size, one pointer on the C stack
; out: X16_P0/P1 = dest, X16_P2 = size
copy_marshal:
        sta     mos8(X16_P2)            ; size
        lda     mos8(__rc2)             ; dest
        sta     mos8(X16_P0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        rts

; ---------------------------------------------------------------------
; unsigned char x16_fp_is_primary(void)
;   Is the chosen entry one the caller can act on? 1 when it matches the
;   primary pattern (falling back to the filter, then to "*.*").
; ---------------------------------------------------------------------
x16_fp_is_primary:
        jsr     fp_is_primary           ; carry set = primary
        lda     #0
        rol     a
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char __fastcall__ x16_fp_match(const char *name,
;                                         const char *patterns)
;   Does a name match a ';' list of patterns? The same matcher the panel
;   filters with, exposed because a caller often wants to ask it about a
;   name of its own. A NULL pattern list matches everything, which is
;   what an unset filter means.
; ---------------------------------------------------------------------
x16_fp_match:
        lda     mos8(__rc2)             ; name
        sta     mos8(X16_P0)
        lda     mos8(__rc3)
        sta     mos8(X16_P1)
        lda     mos8(__rc4)             ; patterns
        sta     mos8(X16_P2)
        lda     mos8(__rc5)
        sta     mos8(X16_P3)
        jsr     fp_match                ; carry set = match
        lda     #0
        rol     a
        ldx     #0
        rts

; ---------------------------------------------------------------------
; unsigned char x16_fp_panel_top(void)   ...left / width / rows
;   The panel's geometry, for a caller drawing inside it. Valid once
;   x16_fp_open() has run: the panel is sized to the screen it finds
;   (80x60 or 40x30).
; ---------------------------------------------------------------------
x16_fp_panel_top:
        lda     #FPK_PTOP
        ldx     #0
        rts

x16_fp_panel_left:
        lda     fp_left
        ldx     #0
        rts

x16_fp_panel_width:
        lda     fp_wide
        ldx     #0
        rts

x16_fp_panel_rows:
        lda     fp_rows
        ldx     #0
        rts

; =====================================================================
; Internal routines -- what the caller reads back
; =====================================================================

; fp_path -- out: X/Y = the absolute path of the chosen entry.
; INTERNAL ONLY in this port: the public accessors copy instead (see the
; header -- a banked module cannot lend a pointer into its bank).
fp_path:
        ldx     #<fp_full
        ldy     #>fp_full
        rts

; fp_name -- out: X/Y = a pointer into the path, past the last '/'
fp_name:
        lda     #<fp_full
        sta     fp_ptr
        lda     #>fp_full
        sta     fp_ptr+1
        ldy     #0
filepick_nm_scan:
        lda     fp_full,y
        beq     filepick_nm_done
        cmp     #CH_SLASH
        bne     filepick_nm_next
        ; the character after this slash starts the name
        tya
        sec
        adc     #<fp_full               ; sec: +1 as well, for the slash itself
        sta     fp_ptr
        lda     #>fp_full
        adc     #0
        sta     fp_ptr+1
filepick_nm_next:
        iny
        bne     filepick_nm_scan
filepick_nm_done:
        ldx     fp_ptr
        ldy     fp_ptr+1
        rts

; fp_dir -- out: X/Y = the directory being browsed, which is where the
; drive is left standing
fp_dir:
        ldx     #<fp_curdir
        ldy     #>fp_curdir
        rts

; ---------------------------------------------------------------------
; fp_copy_path / fp_copy_name / fp_copy_dir
;   in:  X16_P0/P1 = destination, X16_P2 = its size (the NUL included)
;   out: A = how many characters were copied, terminator aside
; ---------------------------------------------------------------------
fp_copy_path:
        lda     #<fp_full
        sta     fp_src
        lda     #>fp_full
        sta     fp_src+1
        bra     filepick_copy_out

fp_copy_name:
        jsr     fp_name
        stx     fp_src
        sty     fp_src+1
        bra     filepick_copy_out

fp_copy_dir:
        lda     #<fp_curdir
        sta     fp_src
        lda     #>fp_curdir
        sta     fp_src+1
filepick_copy_out:
        lda     mos8(X16_P0)
        sta     fp_dst
        lda     mos8(X16_P1)
        sta     fp_dst+1
        lda     mos8(X16_P2)
        beq     filepick_co_none
        dec     a                       ; leave room for the terminator
        jsr     filepick_put_str
        tya                             ; filepick_put_str leaves Y = the length
        rts
filepick_co_none:
        lda     #0
        rts

; ---------------------------------------------------------------------
; fp_is_primary -- is the chosen entry one the caller can act on?
;   out: carry set when it matches the primary pattern
; ---------------------------------------------------------------------
fp_is_primary:
        jsr     fp_name
        stx     mos8(X16_P0)
        sty     mos8(X16_P1)
        jsr     filepick_primpat
        sta     mos8(X16_P2)
        stx     mos8(X16_P3)
        jmp     fp_match

; =====================================================================
; matching
; =====================================================================

; ---------------------------------------------------------------------
; fp_match -- does a name match a ';' list of patterns?
;   in:  X16_P0/P1 = the name, X16_P2/P3 = the pattern list
;   out: carry set when it matches
; ---------------------------------------------------------------------
fp_match:
        lda     mos8(X16_P2)
        ora     mos8(X16_P3)
        bne     filepick_m_have
        sec                             ; no pattern: everything matches
        rts
filepick_m_have:
        lda     mos8(X16_P2)
        sta     fp_pat
        lda     mos8(X16_P3)
        sta     fp_pat+1
filepick_m_loop:
        lda     fp_pat
        sta     mos8(X16_T0)
        lda     fp_pat+1
        sta     mos8(X16_T1)
        ldy     #0
        lda     (X16_T0),y
        beq     filepick_m_no           ; end of the list, nothing matched
        jsr     filepick_match_one
        bcs     filepick_m_yes
        ; step past this pattern to the one after the ';'
filepick_m_skip:
        lda     fp_pat
        sta     mos8(X16_T0)
        lda     fp_pat+1
        sta     mos8(X16_T1)
        ldy     #0
        lda     (X16_T0),y
        beq     filepick_m_no
        cmp     #CH_SEMI
        beq     filepick_m_next
        inc     fp_pat
        bne     filepick_m_skip
        inc     fp_pat+1
        bra     filepick_m_skip
filepick_m_next:
        inc     fp_pat
        bne     filepick_m_loop
        inc     fp_pat+1
        bra     filepick_m_loop
filepick_m_yes:
        sec
        rts
filepick_m_no:
        clc
        rts

; One pattern, at fp_pat, against the name in X16_P0/P1.
;   out: carry set when it matches
filepick_match_one:
        lda     fp_pat
        sta     mos8(X16_T0)
        lda     fp_pat+1
        sta     mos8(X16_T1)
        ldy     #0
        lda     (X16_T0),y
        cmp     #CH_STAR
        beq     filepick_mo_star
        clc                             ; only "*..." patterns are understood
        rts
filepick_mo_star:
        ldy     #1
        lda     (X16_T0),y
        bne     filepick_hop1           ; "*"
        jmp     filepick_mo_all
filepick_hop1:
        cmp     #CH_SEMI
        bne     filepick_hop2           ; "*;..."
        jmp     filepick_mo_all
filepick_hop2:
        cmp     #CH_DOT
        beq     filepick_hop3
        jmp     filepick_mo_bad
filepick_hop3:
        ldy     #2
        lda     (X16_T0),y
        cmp     #CH_STAR
        bne     filepick_hop4           ; "*.*"
        jmp     filepick_mo_all
filepick_hop4:
        ; "*.ext": measure the extension, up to the next ';'
        lda     fp_pat
        clc
        adc     #2
        sta     fp_src
        lda     fp_pat+1
        adc     #0
        sta     fp_src+1
        ldy     #0
filepick_mo_extlen:
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        lda     (X16_T0),y
        beq     filepick_mo_gotext
        cmp     #CH_SEMI
        beq     filepick_mo_gotext
        iny
        bne     filepick_mo_extlen
filepick_mo_gotext:
        cpy     #0
        beq     filepick_mo_bad         ; "*." on its own is not a pattern
        sty     fp_cnt                  ; the extension's length

        ; the name's length
        lda     mos8(X16_P0)
        sta     mos8(X16_T0)
        lda     mos8(X16_P1)
        sta     mos8(X16_T1)
        ldy     #0
filepick_mo_namelen:
        lda     (X16_T0),y
        beq     filepick_mo_gotname
        iny
        bne     filepick_mo_namelen
filepick_mo_gotname:
        cpy     fp_cnt                  ; a name has to be longer than ".ext"
        bcc     filepick_mo_bad
        beq     filepick_mo_bad
        tya
        sec
        sbc     fp_cnt                  ; where the tail starts
        sta     fp_tmp
        ; the character before the tail must be the dot
        tay
        dey
        lda     (X16_T0),y
        cmp     #CH_DOT
        bne     filepick_mo_bad
        ; compare, folding case
        ldy     #0
filepick_mo_cmp:
        cpy     fp_cnt
        beq     filepick_mo_all
        lda     mos8(X16_P0)
        sta     mos8(X16_T0)
        lda     mos8(X16_P1)
        sta     mos8(X16_T1)
        tya
        clc
        adc     fp_tmp
        tax                             ; index of this tail character
        txa
        tay
        lda     (X16_T0),y
        jsr     filepick_fold
        sta     fp_tmp2
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        txa
        sec
        sbc     fp_tmp
        tay
        lda     (X16_T0),y
        jsr     filepick_fold
        cmp     fp_tmp2
        bne     filepick_mo_bad
        iny
        bne     filepick_mo_cmp
filepick_mo_all:
        sec
        rts
filepick_mo_bad:
        clc
        rts

; A -> the same letter with bit 5 clear, whichever case it arrived in
filepick_fold:
        cmp     #$41
        bcc     filepick_fd_out
        cmp     #$5B
        bcc     filepick_fd_do
        cmp     #$61
        bcc     filepick_fd_out
        cmp     #$7B
        bcs     filepick_fd_out
filepick_fd_do:
        and     #$DF
filepick_fd_out:
        rts

; -> A/X = the primary pattern, falling back to the filter, then to "*.*"
filepick_primpat:
        lda     fp_prim
        ora     fp_prim+1
        beq     filepick_pp_filt
        lda     fp_prim
        ldx     fp_prim+1
        rts
filepick_pp_filt:
        lda     fp_filt
        ora     fp_filt+1
        beq     filepick_pp_all
        lda     fp_filt
        ldx     fp_filt+1
        rts
filepick_pp_all:
        lda     #<filepick_alldef
        ldx     #>filepick_alldef
        rts

; -> A/X = the filter, or "*.*"
filepick_filtpat:
        lda     fp_filt
        ora     fp_filt+1
        beq     filepick_fp_all
        lda     fp_filt
        ldx     fp_filt+1
        rts
filepick_fp_all:
        lda     #<filepick_alldef
        ldx     #>filepick_alldef
        rts

; =====================================================================
; small helpers
; =====================================================================

; X16_P0/P1 = string -> Y = its length, terminator aside
filepick_zlen:
        lda     mos8(X16_P0)
        sta     mos8(X16_T0)
        lda     mos8(X16_P1)
        sta     mos8(X16_T1)
        ldy     #0
filepick_zl_loop:
        lda     (X16_T0),y
        beq     filepick_zl_done
        iny
        bne     filepick_zl_loop
filepick_zl_done:
        rts

; fp_src -> fp_dst, at most A characters, always terminated
filepick_put_str:
        sta     fp_cnt
        ldy     #0
filepick_ps_loop:
        cpy     fp_cnt
        beq     filepick_ps_end
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        lda     (X16_T0),y
        beq     filepick_ps_end
        pha
        lda     fp_dst
        sta     mos8(X16_T0)
        lda     fp_dst+1
        sta     mos8(X16_T1)
        pla
        sta     (X16_T0),y
        iny
        bne     filepick_ps_loop
filepick_ps_end:
        lda     fp_dst
        sta     mos8(X16_T0)
        lda     fp_dst+1
        sta     mos8(X16_T1)
        lda     #0
        sta     (X16_T0),y
        rts

; A = entry index: point VERA port 0 at that entry in the cache.
;
; The cache is in VRAM rather than in a RAM bank, and that is not a
; detail: the upstream module can be banked, and a banked filepick runs
; from the $A000 window itself, so paging a bank in to reach its own
; data would page its own code away. VRAM is reachable from anywhere.
filepick_ent:
        sta     fp_tmp
        stz     fp_ptr
        stz     fp_ptr+1
        lda     fp_tmp                  ; index * 40 = index*32 + index*8
        sta     fp_ptr
        asl     fp_ptr                  ; *2
        rol     fp_ptr+1
        asl     fp_ptr                  ; *4
        rol     fp_ptr+1
        asl     fp_ptr                  ; *8
        rol     fp_ptr+1
        lda     fp_ptr
        sta     fp_tmp2                 ; keep index*8
        lda     fp_ptr+1
        sta     fp_cnt
        asl     fp_ptr                  ; *16
        rol     fp_ptr+1
        asl     fp_ptr                  ; *32
        rol     fp_ptr+1
        clc
        lda     fp_ptr
        adc     fp_tmp2
        sta     fp_ptr
        lda     fp_ptr+1
        adc     fp_cnt
        sta     fp_ptr+1
        clc                             ; + the cache's own address
        lda     fp_ptr
        adc     fp_vram
        sta     fp_ptr
        lda     fp_ptr+1
        adc     fp_vram+1
        sta     fp_ptr+1
        ; fall through: point port 0 at fp_ptr, stepping by one
filepick_point0:
        vera_addrsel 0
        lda     fp_ptr
        sta     VERA_ADDR_L
        lda     fp_ptr+1
        sta     VERA_ADDR_M
        lda     fp_vramh
        and     #$01
        ora     #$10                    ; increment 1
        sta     VERA_ADDR_H
        rts

; A = entry index: point port 0 at that entry's NAME
filepick_ent_name:
        jsr     filepick_ent
        clc
        lda     fp_ptr
        adc     #FPK_ENAME
        sta     fp_ptr
        lda     fp_ptr+1
        adc     #0
        sta     fp_ptr+1
        jmp     filepick_point0

; A = entry index -> A = its type, port 0 left just past it
filepick_ent_type:
        jsr     filepick_ent
        lda     VERA_DATA0
        rts

; A = entry index: copy its name out of VRAM into fp_nm, so the rest of
; the code can treat it as an ordinary string.
filepick_ent_fetch:
        jsr     filepick_ent_name
        ldy     #0
filepick_ef_loop:
        lda     VERA_DATA0
        sta     fp_nm,y
        beq     filepick_ef_done
        iny
        cpy     #FPK_ESIZE-2
        bne     filepick_ef_loop
        lda     #0
        sta     fp_nm,y
filepick_ef_done:
        rts

; =====================================================================
; the listing
; =====================================================================

; Read the current directory into the cache: directories first, then
; whatever the primary pattern matches, then the rest. Three passes over
; the listing rather than a sort.
filepick_read:
        stz     fp_nent
        stz     fp_pass
filepick_rd_pass:
        stz     mos8(X16_P0)            ; dir_open with no name: "$"
        stz     mos8(X16_P1)
        stz     mos8(X16_P2)
        lda     #8
        sta     mos8(X16_P3)
        jsr     dir_open
        bcc     filepick_hop5
        jmp     filepick_rd_done
filepick_hop5:
filepick_rd_next:
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        lda     #40
        sta     mos8(X16_P2)
        jsr     dir_next
        bcs     filepick_hop6
        jmp     filepick_rd_close
filepick_hop6:
        jsr     dir_type
        sta     fp_tmp                  ; the type the drive reported
        ; Not files. The header line is a path on an emulator's host
        ; filesystem (HOST) and the volume label on a real card (NONE,
        ; with raw directory bytes in the name), and the "BLOCKS FREE."
        ; trailer is NONE as well: listing either put rubbish in the
        ; panel.
        cmp     #DIR_TYPE_NONE
        beq     filepick_rd_next
        cmp     #DIR_TYPE_HOST
        beq     filepick_rd_next
        lda     fp_nent
        cmp     #FPK_MAXENT
        bcs     filepick_rd_next        ; the cache is full
        ; which pass wants this one?
        lda     fp_tmp
        cmp     #DIR_TYPE_DIR
        bne     filepick_rd_file
        lda     fp_pass
        bne     filepick_rd_next        ; directories belong to pass 0
        lda     fp_nm                   ; "." leads nowhere
        cmp     #CH_DOT
        bne     filepick_rd_keep_dir
        lda     fp_nm+1
        beq     filepick_rd_next
filepick_rd_keep_dir:
        lda     #DIR_TYPE_DIR
        sta     fp_kind
        bra     filepick_rd_store
filepick_rd_file:
        lda     fp_pass
        beq     filepick_rd_next        ; files are passes 1 and 2
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jsr     filepick_filtpat
        sta     mos8(X16_P2)
        stx     mos8(X16_P3)
        jsr     fp_match
        bcc     filepick_rd_next        ; not ours to show at all
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jsr     filepick_primpat
        sta     mos8(X16_P2)
        stx     mos8(X16_P3)
        jsr     fp_match
        ; The carry says primary -- and the cmp below would destroy it,
        ; so put it somewhere that survives asking which pass this is.
        ; Without this the pass test read its own comparison's carry,
        ; every file came out primary, and nothing was ever marked [dat].
        lda     #0
        rol     a                       ; 1 = primary, 0 = data
        sta     fp_cnt
        lda     fp_pass
        cmp     #1
        bne     filepick_rd_datapass
        lda     fp_cnt                  ; pass 1 keeps the primaries
        bne     filepick_rd_isprim
        jmp     filepick_rd_next
filepick_rd_isprim:
        lda     #DIR_TYPE_PRG
        sta     fp_kind
        bra     filepick_rd_store
filepick_rd_datapass:
        lda     fp_cnt                  ; pass 2 keeps everything else
        beq     filepick_rd_isdata
        jmp     filepick_rd_next
filepick_rd_isdata:
        lda     #DIR_TYPE_SEQ
        sta     fp_kind
filepick_rd_store:
        lda     fp_nent
        jsr     filepick_ent            ; port 0 at the entry, stepping by one
        lda     fp_kind
        sta     VERA_DATA0              ; the type
        ldy     #0                      ; ...then the name, terminator included
filepick_rd_name:
        lda     fp_nm,y
        sta     VERA_DATA0
        beq     filepick_rd_named
        iny
        cpy     #FPK_ESIZE-2
        bne     filepick_rd_name
        lda     #0
        sta     VERA_DATA0
filepick_rd_named:
        inc     fp_nent
        jmp     filepick_rd_next
filepick_rd_close:
        jsr     dir_close
        inc     fp_pass
        lda     fp_pass
        cmp     #3
        bcs     filepick_hop8
        jmp     filepick_rd_pass
filepick_hop8:
filepick_rd_done:
        rts

; fp_curdir + "/" + the name at X16_P0/P1 -> fp_full
filepick_make_path:
        lda     mos8(X16_P0)
        sta     fp_src
        lda     mos8(X16_P1)
        sta     fp_src+1
        ldy     #0
filepick_mp_dir:
        lda     fp_curdir,y
        beq     filepick_mp_slash
        sta     fp_full,y
        iny
        cpy     #40
        bne     filepick_mp_dir
filepick_mp_slash:
        cpy     #0
        beq     filepick_mp_name
        dey
        lda     fp_full,y
        iny
        cmp     #CH_SLASH
        beq     filepick_mp_name
        lda     #CH_SLASH
        sta     fp_full,y
        iny
filepick_mp_name:
        sty     fp_tmp                  ; where the name goes
        ldx     #0
filepick_mp_copy:
        txa
        tay
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        lda     (X16_T0),y
        beq     filepick_mp_end
        ldy     fp_tmp
        sta     fp_full,y
        inc     fp_tmp
        lda     fp_tmp
        cmp     #63
        bcs     filepick_mp_end
        inx
        bne     filepick_mp_copy
filepick_mp_end:
        ldy     fp_tmp
        lda     #0
        sta     fp_full,y
        rts

; Where we are, kept by hand: ".." trims the last component, anything
; else appends one. The drive is not asked, because it answers with a
; volume label on a card and a path on an emulator.
filepick_descend:
        lda     mos8(X16_P0)
        sta     fp_src
        lda     mos8(X16_P1)
        sta     fp_src+1
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        ldy     #0
        lda     (X16_T0),y
        cmp     #CH_DOT
        bne     filepick_ds_append
        iny
        lda     (X16_T0),y
        cmp     #CH_DOT
        bne     filepick_ds_append
        iny
        lda     (X16_T0),y
        bne     filepick_ds_append
        ; ".." -- back up over the last component
        ldy     #0
filepick_ds_len:
        lda     fp_curdir,y
        beq     filepick_ds_gotlen
        iny
        bne     filepick_ds_len
filepick_ds_gotlen:
        cpy     #2
        bcc     filepick_ds_root
filepick_ds_back:
        dey
        beq     filepick_ds_root
        lda     fp_curdir,y
        cmp     #CH_SLASH
        bne     filepick_ds_back
        cpy     #0
        bne     filepick_ds_cut
filepick_ds_root:
        lda     #CH_SLASH
        sta     fp_curdir
        lda     #0
        sta     fp_curdir+1
        rts
filepick_ds_cut:
        lda     #0
        sta     fp_curdir,y
        rts
filepick_ds_append:
        ldy     #0
filepick_ds_alen:
        lda     fp_curdir,y
        beq     filepick_ds_agot
        iny
        bne     filepick_ds_alen
filepick_ds_agot:
        cpy     #0
        beq     filepick_ds_acopy
        dey
        lda     fp_curdir,y
        iny
        cmp     #CH_SLASH
        beq     filepick_ds_acopy
        lda     #CH_SLASH
        sta     fp_curdir,y
        iny
filepick_ds_acopy:
        sty     fp_tmp
        ldx     #0
filepick_ds_aloop:
        txa
        tay
        lda     fp_src
        sta     mos8(X16_T0)
        lda     fp_src+1
        sta     mos8(X16_T1)
        lda     (X16_T0),y
        beq     filepick_ds_aend
        ldy     fp_tmp
        sta     fp_curdir,y
        inc     fp_tmp
        lda     fp_tmp
        cmp     #63
        bcs     filepick_ds_aend
        inx
        bne     filepick_ds_aloop
filepick_ds_aend:
        ldy     fp_tmp
        lda     #0
        sta     fp_curdir,y
        rts

; =====================================================================
; the panel
; =====================================================================
filepick_layout:
        jsr     screen_get_mode
        cmp     #0
        bne     filepick_ly_small
        lda     #80
        sta     fp_scrw
        lda     #60
        sta     fp_scrh
        lda     #40
        sta     fp_rows
        lda     #6
        sta     fp_left
        lda     #68
        sta     fp_wide
        rts
filepick_ly_small:
        lda     #40
        sta     fp_scrw
        lda     #30
        sta     fp_scrh
        lda     #22
        sta     fp_rows
        lda     #1
        sta     fp_left
        lda     #38
        sta     fp_wide
        rts

; A = row, X = colour: fill one row of the panel
filepick_prow:
        pha
        phx
        tax                             ; screen_addr wants X = row, Y = column
        ldy     fp_left
        jsr     screen_addr
        plx                             ; colour
        lda     fp_wide
        ldy     #CH_SPACE
        jsr     screen_blitfill
        pla
        rts

filepick_draw:
        ; ---- the header row ------------------------------------------
        lda     #FPK_PTOP
        ldx     fp_abar
        jsr     filepick_prow
        lda     fp_head
        ora     fp_head+1
        bne     filepick_dw_head
        lda     #<filepick_headdef
        sta     mos8(X16_P0)
        lda     #>filepick_headdef
        sta     mos8(X16_P1)
        bra     filepick_dw_headgo
filepick_dw_head:
        lda     fp_head
        sta     mos8(X16_P0)
        lda     fp_head+1
        sta     mos8(X16_P1)
filepick_dw_headgo:
        ldx     #FPK_PTOP
        ldy     fp_left
        iny
        jsr     screen_addr
        jsr     filepick_zlen
        cpy     #0
        beq     filepick_dw_path
        tya
        ldx     fp_abar
        jsr     screen_blit
filepick_dw_path:
        lda     #<fp_curdir
        sta     mos8(X16_P0)
        lda     #>fp_curdir
        sta     mos8(X16_P1)
        jsr     filepick_zlen
        tya
        ; a deep path must not run off the bar
        sta     fp_tmp
        lda     fp_wide
        sec
        sbc     #14
        cmp     fp_tmp
        bcs     filepick_dw_pathlen
        sta     fp_tmp
filepick_dw_pathlen:
        lda     fp_tmp
        beq     filepick_dw_close
        ldx     fp_abar
        jsr     screen_blit
filepick_dw_close:
        ldx     #FPK_PTOP
        lda     fp_left
        clc
        adc     fp_wide
        sec
        sbc     #3
        tay
        jsr     screen_addr
        lda     #<filepick_closebox
        sta     mos8(X16_P0)
        lda     #>filepick_closebox
        sta     mos8(X16_P1)
        lda     #3
        ldx     #$F2                    ; red on light grey: click to close
        jsr     screen_blit

        ; ---- the rows -------------------------------------------------
        stz     fp_row
filepick_dw_row:
        lda     fp_row
        cmp     fp_rows
        bcc     filepick_hop9
        jmp     filepick_dw_foot
filepick_hop9:
        clc
        adc     fp_top
        sta     fp_idx
        ldx     fp_apanel
        cmp     fp_sel
        bne     filepick_dw_attr
        ldx     fp_asel
filepick_dw_attr:
        stx     fp_attr
        lda     fp_row
        clc
        adc     #FPK_PTOP+1
        ldx     fp_attr
        jsr     filepick_prow
        lda     fp_idx
        cmp     fp_nent
        bcs     filepick_dw_next
        ; Read the entry out of VRAM FIRST. The cache and the screen are
        ; both reached through VERA port 0, and screen_addr points it at
        ; the screen -- fetching a name after that wrote the row into the
        ; cache instead of onto the display, and left the panel blank.
        lda     fp_idx
        jsr     filepick_ent_type
        sta     fp_kind                 ; filepick_ent uses fp_tmp2 itself
        lda     fp_idx
        jsr     filepick_ent_fetch      ; the name, into fp_nm
        lda     fp_row
        clc
        adc     #FPK_PTOP+1
        tax
        lda     fp_left
        clc
        adc     #2
        tay
        jsr     screen_addr
        lda     fp_kind
        cmp     #DIR_TYPE_DIR
        bne     filepick_dw_notdir
        lda     #<filepick_dirtag
        ldx     #>filepick_dirtag
        bra     filepick_dw_tag
filepick_dw_notdir:
        cmp     #DIR_TYPE_SEQ
        bne     filepick_dw_blanktag
        lda     #<filepick_dattag
        ldx     #>filepick_dattag
        bra     filepick_dw_tag
filepick_dw_blanktag:
        lda     #<filepick_blanktag
        ldx     #>filepick_blanktag
filepick_dw_tag:
        sta     mos8(X16_P0)
        stx     mos8(X16_P1)
        lda     #6
        ldx     fp_attr
        jsr     screen_blit
        ; the name, clamped: a row that runs over wraps around the screen
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jsr     filepick_zlen
        tya
        sta     fp_tmp
        lda     fp_wide
        sec
        sbc     #10
        cmp     fp_tmp
        bcs     filepick_dw_namelen
        sta     fp_tmp
filepick_dw_namelen:
        lda     fp_tmp
        beq     filepick_dw_next
        ldx     fp_attr
        jsr     screen_blit
filepick_dw_next:
        inc     fp_row
        jmp     filepick_dw_row

        ; ---- the footer ------------------------------------------------
filepick_dw_foot:
        lda     fp_rows
        clc
        adc     #FPK_PTOP+1
        ldx     fp_abar
        jsr     filepick_prow
        lda     fp_foot
        ora     fp_foot+1
        bne     filepick_dw_footset
        lda     #<filepick_footdef
        sta     mos8(X16_P0)
        lda     #>filepick_footdef
        sta     mos8(X16_P1)
        bra     filepick_dw_footgo
filepick_dw_footset:
        lda     fp_foot
        sta     mos8(X16_P0)
        lda     fp_foot+1
        sta     mos8(X16_P1)
filepick_dw_footgo:
        lda     fp_rows
        clc
        adc     #FPK_PTOP+1
        tax
        lda     fp_left
        clc
        adc     #1
        tay
        jsr     screen_addr
        jsr     filepick_zlen
        cpy     #0
        beq     filepick_dw_end
        tya
        sta     fp_tmp
        lda     fp_wide
        sec
        sbc     #2
        cmp     fp_tmp
        bcs     filepick_dw_footlen
        sta     fp_tmp
filepick_dw_footlen:
        lda     fp_tmp
        ldx     fp_abar
        jsr     screen_blit
filepick_dw_end:
        rts

; A = the key: move the selection
filepick_move:
        cmp     #KEY_UP
        bne     filepick_mv_down
        lda     fp_sel
        beq     filepick_mv_clamp
        dec     fp_sel
        bra     filepick_mv_clamp
filepick_mv_down:
        cmp     #KEY_DOWN
        bne     filepick_mv_home
        lda     fp_sel
        clc
        adc     #1
        cmp     fp_nent
        bcs     filepick_mv_clamp
        inc     fp_sel
        bra     filepick_mv_clamp
filepick_mv_home:
        cmp     #KEY_HOME
        bne     filepick_mv_clamp
        stz     fp_sel
filepick_mv_clamp:
        lda     fp_sel                  ; scrolled off the top?
        cmp     fp_top
        bcs     filepick_mv_bottom
        sta     fp_top
filepick_mv_bottom:
        lda     fp_top                  ; ...or off the bottom?
        clc
        adc     fp_rows
        cmp     fp_sel
        beq     filepick_mv_scroll
        bcs     filepick_mv_out
filepick_mv_scroll:
        lda     fp_sel
        sec
        sbc     fp_rows
        clc
        adc     #1
        sta     fp_top
filepick_mv_out:
        rts

; =====================================================================
; save-under
;
; The text map IS VRAM, so keeping a copy of it somewhere else in VRAM
; costs nothing but the space: port 0 walks the screen, port 1 walks the
; scratch, and the bytes go across one at a time. Two bytes per cell,
; (rows + 2) rows of the panel's width: 5,712 bytes at 80 columns.
; =====================================================================

; A = row: point port 1 at that row's copy in the scratch area
filepick_under_addr:
        sta     fp_tmp
        stz     fp_dst
        stz     fp_dst+1
        lda     fp_tmp
        beq     filepick_ua_have
        ldx     fp_tmp
filepick_ua_loop:
        clc
        lda     fp_dst
        adc     fp_wide
        sta     fp_dst
        lda     fp_dst+1
        adc     #0
        sta     fp_dst+1
        dex
        bne     filepick_ua_loop
filepick_ua_have:
        asl     fp_dst                  ; two bytes per cell
        rol     fp_dst+1
        clc
        lda     fp_dst
        adc     fp_under
        sta     fp_dst
        lda     fp_dst+1
        adc     fp_under+1
        sta     fp_dst+1
        vera_addrsel 1
        lda     fp_dst
        sta     VERA_ADDR_L
        lda     fp_dst+1
        sta     VERA_ADDR_M
        lda     fp_underh
        and     #$01
        ora     #$10                    ; increment 1
        sta     VERA_ADDR_H
        vera_addrsel 0                  ; back to port 0 for the caller
        rts

filepick_save_under:
        lda     fp_undon
        bne     filepick_su_go1
        rts
filepick_su_go1:
        stz     fp_row
filepick_su_row:
        lda     fp_row
        cmp     fp_rows
        bcc     filepick_su_go
        beq     filepick_su_go
        sec                             ; rows + 2: the header and the footer
        sbc     fp_rows
        cmp     #2
        bcc     filepick_su_go
        lda     #1
        sta     fp_saved
        rts
filepick_su_go:
        lda     fp_row
        clc
        adc     #FPK_PTOP
        tax
        ldy     fp_left
        jsr     screen_addr             ; port 0 at the screen row
        lda     fp_row
        jsr     filepick_under_addr     ; port 1 at its copy
        lda     fp_wide
        asl     a                       ; two bytes per cell
        sta     fp_cnt
filepick_su_cell:
        lda     VERA_DATA0
        sta     VERA_DATA1
        dec     fp_cnt
        bne     filepick_su_cell
        inc     fp_row
        bra     filepick_su_row

filepick_restore_under:
        lda     fp_undon
        bne     filepick_ru_go1
        rts
filepick_ru_go1:
        lda     fp_saved
        bne     filepick_ru_go2
        rts
filepick_ru_go2:
        stz     fp_row
filepick_ru_row:
        lda     fp_row
        cmp     fp_rows
        bcc     filepick_ru_go
        beq     filepick_ru_go
        sec
        sbc     fp_rows
        cmp     #2
        bcc     filepick_ru_go
        stz     fp_saved
        rts
filepick_ru_go:
        lda     fp_row
        clc
        adc     #FPK_PTOP
        tax
        ldy     fp_left
        jsr     screen_addr
        lda     fp_row
        jsr     filepick_under_addr
        lda     fp_wide
        asl     a
        sta     fp_cnt
filepick_ru_cell:
        lda     VERA_DATA1
        sta     VERA_DATA0
        dec     fp_cnt
        bne     filepick_ru_cell
        inc     fp_row
        bra     filepick_ru_row

; =====================================================================
; opening, closing, and the loop between
; =====================================================================

; fp_open -- put the panel up on the starting directory
;   out: A = FPK_NONE / FPK_PICK / FPK_ALT / FPK_HERE
fp_open:
        jsr     filepick_layout
        stz     fp_saved
        lda     fp_startat
        ora     fp_startat+1
        bne     filepick_op_start
        lda     #<filepick_root
        sta     mos8(X16_P0)
        lda     #>filepick_root
        sta     mos8(X16_P1)
        bra     filepick_op_setdir
filepick_op_start:
        lda     fp_startat
        sta     mos8(X16_P0)
        lda     fp_startat+1
        sta     mos8(X16_P1)
filepick_op_setdir:
        lda     mos8(X16_P0)
        sta     fp_src
        lda     mos8(X16_P1)
        sta     fp_src+1
        lda     #<fp_curdir
        sta     fp_dst
        lda     #>fp_curdir
        sta     fp_dst+1
        lda     #63
        jsr     filepick_put_str
        lda     fp_src                  ; and take the drive there
        sta     mos8(X16_P0)
        lda     fp_src+1
        sta     mos8(X16_P1)
        jsr     filepick_zlen           ; Y = length
        lda     mos8(X16_P0)            ; dos_chdir wants A/X = name, Y = length
        ldx     mos8(X16_P1)
        jsr     dos_chdir
        stz     fp_sel
        stz     fp_top
        lda     #255
        sta     fp_lastidx
        lda     fp_chset
        cmp     #255
        beq     filepick_op_nochar
        jsr     screen_charset
filepick_op_nochar:
        jsr     filepick_save_under
        jsr     filepick_read
        lda     #1                      ; the pointer, with the panel's bounds
        ldx     fp_scrw
        ldy     fp_scrh
        jsr     MOUSE_CONFIG
        lda     #1                      ; the click that opened us may still be held
        sta     fp_down
        jmp     filepick_loop

; fp_resume -- the same panel again, same directory, same selection
fp_resume:
        lda     #1
        ldx     fp_scrw
        ldy     fp_scrh
        jsr     MOUSE_CONFIG
        lda     #1
        sta     fp_down
        jmp     filepick_loop

filepick_loop:
        jsr     filepick_draw
filepick_lp_input:
        stz     fp_key
        stz     fp_act
filepick_lp_poll:
        jsr     GETIN
        sta     fp_key
        beq     filepick_hop10
        ; The KERNAL answers in PETSCII, where an unshifted letter is
        ; $41-$5A -- the codes ASCII uses for CAPITALS -- and a shifted
        ; one is $C1-$DA. Fold the shifted range down so both cases of a
        ; letter command land on the same compare.
        cmp     #$C1
        bcc     filepick_kf_done
        cmp     #$DB
        bcs     filepick_kf_done
        sec
        sbc     #$80
        sta     fp_key
filepick_kf_done:
        jmp     filepick_lp_act
filepick_hop10:
        jsr     mouse_get
        and     #3                      ; left (1) and right (2)
        sta     fp_tmp
        bne     filepick_lp_press
        stz     fp_down                 ; released
        bra     filepick_lp_poll
filepick_lp_press:
        lda     fp_down
        bne     filepick_lp_poll        ; still the same press
        lda     #1
        sta     fp_down
        ; which cell is under the pointer?
        lda     mos8(X16_P2)            ; y, in pixels
        lsr     mos8(X16_P3)
        ror     a
        lsr     mos8(X16_P3)
        ror     a
        lsr     mos8(X16_P3)
        ror     a
        sta     fp_row                  ; the text row
        lda     mos8(X16_P0)            ; x
        lsr     mos8(X16_P1)
        ror     a
        lsr     mos8(X16_P1)
        ror     a
        lsr     mos8(X16_P1)
        ror     a
        sta     fp_tmp2                 ; the text column
        ; the x box on the header row closes, like ESC
        lda     fp_row
        cmp     #FPK_PTOP
        bne     filepick_lp_rows
        lda     fp_left
        clc
        adc     fp_wide
        sec
        sbc     #3
        cmp     fp_tmp2
        bcs     filepick_lp_poll
        lda     #KEY_ESC
        sta     fp_key
        jmp     filepick_lp_act
filepick_lp_rows:
        lda     fp_row
        cmp     #FPK_PTOP+1
        bcc     filepick_lp_poll
        sec
        sbc     #FPK_PTOP+1
        sta     fp_row                  ; the line within the list
        cmp     fp_rows
        bcs     filepick_lp_poll
        clc
        adc     fp_top
        cmp     fp_nent
        bcc     filepick_fk1677
        jmp     filepick_lp_poll
filepick_fk1677:
        sta     fp_idx
        sta     fp_sel
        lda     fp_tmp
        and     #2
        beq     filepick_lp_left
        ; RIGHT button: the ALT gesture
        lda     #3
        sta     fp_act
        lda     #255
        sta     fp_lastidx
        bra     filepick_lp_act
filepick_lp_left:
        jsr     RDTIM                   ; A/X = the low 16 bits of the jiffy clock
        sta     fp_tmp
        stx     fp_tmp2
        lda     fp_idx
        cmp     fp_lastidx
        bne     filepick_lp_single
        sec                             ; how long since the last click here?
        lda     fp_tmp
        sbc     fp_lastck
        sta     fp_cnt
        lda     fp_tmp2
        sbc     fp_lastck+1
        bne     filepick_lp_single      ; more than 255 jiffies ago
        lda     fp_cnt
        cmp     #FPK_DBLCLK
        bcs     filepick_lp_single
        lda     #1                      ; double click
        sta     fp_act
        lda     #255
        sta     fp_lastidx
        bra     filepick_lp_act
filepick_lp_single:
        lda     fp_tmp
        sta     fp_lastck
        lda     fp_tmp2
        sta     fp_lastck+1
        lda     fp_idx
        sta     fp_lastidx
        lda     #2                      ; select only
        sta     fp_act
filepick_lp_act:
        lda     fp_act
        cmp     #2
        bne     filepick_hop11          ; selection moved: redraw and carry on
        jmp     filepick_loop
filepick_hop11:
        cmp     #3
        bne     filepick_lp_key
        ; the ALT gesture, which only makes sense on a file
        lda     fp_sel
        jsr     filepick_ent_type
        cmp     #DIR_TYPE_DIR
        beq     filepick_lp_again
        jsr     filepick_path_of_sel
        lda     #FPK_ALT
        rts
filepick_lp_again:
        lda     #1
        sta     fp_down
        jmp     filepick_loop
filepick_lp_key:
        lda     fp_act
        cmp     #1
        bne     filepick_lp_haskey
        lda     #KEY_ENTER              ; a double click is Enter
        sta     fp_key
filepick_lp_haskey:
        lda     fp_key
        cmp     #CH_H                   ; "the folder I am looking at"
        bne     filepick_lp_nothere
        lda     #FPK_HERE
        rts
filepick_lp_nothere:
        lda     fp_key
        cmp     #CH_N
        bne     filepick_lp_note1
        jmp     filepick_ed_newdir
filepick_lp_note1:
        cmp     #CH_E                   ; not 'r': that already runs/picks
        bne     filepick_lp_note2
        jmp     filepick_ed_rename
filepick_lp_note2:
        cmp     #CH_D
        bne     filepick_lp_note3
        jmp     filepick_ed_delete
filepick_lp_note3:
        cmp     #CH_C
        bne     filepick_lp_note4
        jmp     filepick_ed_copy
filepick_lp_note4:
        cmp     #CH_V
        bne     filepick_lp_note5
        jmp     filepick_ed_paste
filepick_lp_note5:
        lda     fp_key
        cmp     #KEY_ESC
        bne     filepick_hop12
        jmp     filepick_lp_none
filepick_hop12:
        cmp     #KEY_STOP
        bne     filepick_hop13
        jmp     filepick_lp_none
filepick_hop13:
        cmp     #KEY_UP
        bne     filepick_hop14
        jmp     filepick_lp_move
filepick_hop14:
        cmp     #KEY_DOWN
        bne     filepick_hop15
        jmp     filepick_lp_move
filepick_hop15:
        cmp     #KEY_HOME
        beq     filepick_lp_move
        lda     fp_nent
        bne     filepick_hop16          ; nothing to act on
        jmp     filepick_lp_input
filepick_hop16:
        lda     fp_sel
        jsr     filepick_ent_type
        cmp     #DIR_TYPE_DIR
        bne     filepick_lp_file
        lda     fp_key
        cmp     #KEY_ENTER
        beq     filepick_hop17
        jmp     filepick_lp_input
filepick_hop17:
        ; descend: the drive first, then our own idea of where we are
        lda     fp_sel
        jsr     filepick_ent_fetch
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jsr     filepick_zlen           ; Y = length
        lda     #<fp_nm                 ; A/X = name, Y = length
        ldx     #>fp_nm
        jsr     dos_chdir
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jsr     filepick_descend
        stz     fp_sel
        stz     fp_top
        jsr     filepick_read
        jmp     filepick_loop
filepick_lp_file:
        lda     fp_key
        cmp     #KEY_ENTER
        beq     filepick_lp_pick
        cmp     #CH_R
        beq     filepick_lp_pick
        cmp     #CH_A
        beq     filepick_hop18
        jmp     filepick_lp_input
filepick_hop18:
        jsr     filepick_path_of_sel
        lda     #FPK_ALT
        rts
filepick_lp_pick:
        jsr     filepick_path_of_sel
        lda     #FPK_PICK
        rts
filepick_lp_move:
        lda     fp_key
        jsr     filepick_move
        jmp     filepick_loop
filepick_lp_none:
        lda     #FPK_NONE
        rts

; =====================================================================
; managing what is in the panel, rather than only choosing from it
;
;   n  make a folder        c  remember a file  (copy)
;   e  rename               v  write it here    (paste)
;   d  delete
;
; Every one of these ends by re-reading the directory, so the panel is
; never showing something the drive no longer has.
; =====================================================================

; Edit fp_nm in place on the panel's first row. X16_P0/P1 = the label.
;   out: carry set when Enter was pressed with something in the field
;
; Drawn blue on yellow, which nothing else in the panel uses: a field
; you type into that looks like the rows you do not is a field nobody
; sees. Inverting it was not enough -- the selected row is inverted too.
filepick_ed_prompt:
        lda     mos8(X16_P0)
        sta     fp_src
        lda     mos8(X16_P1)
        sta     fp_src+1
filepick_ep_draw:
        lda     #FPK_PTOP+1
        ldx     #FPK_AEDIT
        jsr     filepick_prow
        ldx     #FPK_PTOP+1
        ldy     fp_left
        iny
        jsr     screen_addr
        lda     fp_src
        sta     mos8(X16_P0)
        lda     fp_src+1
        sta     mos8(X16_P1)
        jsr     filepick_zlen
        tya
        ldx     #FPK_AEDIT
        jsr     screen_blit
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        lda     fp_elen
        beq     filepick_ep_cursor
        ldx     #FPK_AEDIT
        jsr     screen_blit
filepick_ep_cursor:
        ; A solid block in the opposite colours, not an underscore in the
        ; same ones: the caret has to be findable at a glance, and a thin
        ; character on a coloured field is not.
        lda     #1
        ldx     #FPK_ACURSOR
        ldy     #CH_SPACE
        jsr     screen_blitfill
        jsr     key_wait
        cmp     #KEY_ENTER
        beq     filepick_ep_enter
        cmp     #KEY_ESC
        beq     filepick_ep_cancel
        cmp     #KEY_STOP
        beq     filepick_ep_cancel
        cmp     #KEY_BKSP
        bne     filepick_ep_char
        lda     fp_elen
        beq     filepick_ep_draw
        dec     fp_elen
        ldy     fp_elen
        lda     #0
        sta     fp_nm,y
        bra     filepick_ep_draw
filepick_ep_char:
        cmp     #CH_SPACE
        bcc     filepick_ep_draw
        cmp     #$80
        bcs     filepick_ep_draw
        ldy     fp_elen
        cpy     #30
        bcs     filepick_ep_draw
        sta     fp_nm,y
        inc     fp_elen
        ldy     fp_elen
        lda     #0
        sta     fp_nm,y
        jmp     filepick_ep_draw
filepick_ep_enter:
        lda     fp_elen
        beq     filepick_ep_cancel
        sec
        rts
filepick_ep_cancel:
        clc
        rts

; X16_P0/P1 = question -> carry set on y
filepick_ed_confirm:
        lda     #FPK_PTOP+1
        ldx     #FPK_AEDIT
        jsr     filepick_prow
        ldx     #FPK_PTOP+1
        ldy     fp_left
        iny
        jsr     screen_addr
        jsr     filepick_zlen
        tya
        ldx     #FPK_AEDIT
        jsr     screen_blit
        jsr     key_wait
        and     #$DF                    ; either case
        cmp     #CH_Y
        beq     filepick_ec_yes
        clc
        rts
filepick_ec_yes:
        sec
        rts

; n -- make a folder here
filepick_ed_newdir:
        stz     fp_nm
        stz     fp_elen
        lda     #<filepick_s_newdir
        sta     mos8(X16_P0)
        lda     #>filepick_s_newdir
        sta     mos8(X16_P1)
        jsr     filepick_ed_prompt
        bcs     filepick_far1977
        jmp     filepick_ed_done
filepick_far1977:
        lda     #<fp_nm
        ldx     #>fp_nm
        ldy     fp_elen
        jsr     dos_mkdir
        jmp     filepick_ed_reread

; e -- rename the selected entry
filepick_ed_rename:
        lda     fp_nent
        bne     filepick_far1987
        jmp     filepick_ed_done
filepick_far1987:
        lda     fp_sel
        jsr     filepick_ent_fetch      ; the old name, into fp_nm
        lda     #<fp_nm
        sta     fp_src
        lda     #>fp_nm
        sta     fp_src+1
        lda     #<fp_clip
        sta     fp_dst
        lda     #>fp_clip
        sta     fp_dst+1
        lda     #38
        jsr     filepick_put_str        ; keep it: the prompt edits fp_nm
        sty     fp_clipok               ; ...and its length, borrowed for a moment
        ldy     #0
filepick_er_len:
        lda     fp_nm,y
        beq     filepick_er_gotlen
        iny
        bne     filepick_er_len
filepick_er_gotlen:
        sty     fp_elen
        lda     #<filepick_s_rename
        sta     mos8(X16_P0)
        lda     #>filepick_s_rename
        sta     mos8(X16_P1)
        jsr     filepick_ed_prompt
        bcc     filepick_ed_clipreset
        lda     #<fp_clip               ; old name
        sta     mos8(X16_P0)
        lda     #>fp_clip
        sta     mos8(X16_P1)
        lda     fp_clipok
        sta     mos8(X16_P2)
        lda     #<fp_nm                 ; new name
        ldx     #>fp_nm
        ldy     fp_elen
        jsr     dos_rename
filepick_ed_clipreset:
        stz     fp_clipok               ; it was only borrowed
        jmp     filepick_ed_reread

; d -- delete the selected entry, folder or file
filepick_ed_delete:
        lda     fp_nent
        beq     filepick_ed_done
        lda     fp_sel
        jsr     filepick_ent_type
        sta     fp_kind
        lda     fp_sel
        jsr     filepick_ent_fetch
        ldy     #0
filepick_dl_len:
        lda     fp_nm,y
        beq     filepick_dl_gotlen
        iny
        bne     filepick_dl_len
filepick_dl_gotlen:
        sty     fp_elen
        lda     #<filepick_s_delete
        sta     mos8(X16_P0)
        lda     #>filepick_s_delete
        sta     mos8(X16_P1)
        jsr     filepick_ed_confirm
        bcc     filepick_ed_done
        lda     fp_kind
        cmp     #DIR_TYPE_DIR
        beq     filepick_dl_dir
        lda     #<fp_nm
        ldx     #>fp_nm
        ldy     fp_elen
        jsr     dos_delete
        jmp     filepick_ed_reread
filepick_dl_dir:
        lda     #<fp_nm
        ldx     #>fp_nm
        ldy     fp_elen
        jsr     dos_rmdir
        jmp     filepick_ed_reread

; c -- remember the selected file
filepick_ed_copy:
        lda     fp_nent
        beq     filepick_ed_done
        lda     fp_sel
        jsr     filepick_ent_type
        cmp     #DIR_TYPE_DIR
        beq     filepick_ed_done        ; folders are not copied, only their files
        jsr     filepick_path_of_sel    ; fp_full = the absolute path
        lda     #<fp_full
        sta     fp_src
        lda     #>fp_full
        sta     fp_src+1
        lda     #<fp_clip
        sta     fp_dst
        lda     #>fp_clip
        sta     fp_dst+1
        lda     #62
        jsr     filepick_put_str
        lda     #1
        sta     fp_clipok
filepick_ed_done:
        jmp     filepick_loop

; v -- write the remembered file into the folder on show
filepick_ed_paste:
        lda     fp_clipok
        beq     filepick_ed_done
        ; the destination name is the source's leaf, plus ",s,w" so the
        ; drive writes a sequential file rather than looking for a program
        lda     #<fp_clip
        sta     fp_src
        lda     #>fp_clip
        sta     fp_src+1
        ldy     #0
        ldx     #0
filepick_pa_leaf:
        lda     fp_clip,y
        beq     filepick_pa_gotleaf
        cmp     #CH_SLASH
        bne     filepick_pa_next
        iny
        tya
        tax                             ; x = where the leaf starts
        dey
filepick_pa_next:
        iny
        bne     filepick_pa_leaf
filepick_pa_gotleaf:
        txa
        tay
        ldx     #0
filepick_pa_copy:
        lda     fp_clip,y
        beq     filepick_pa_suffix
        sta     fp_nm,x
        inx
        iny
        bne     filepick_pa_copy
filepick_pa_suffix:
        ldy     #0
filepick_pa_swr:
        lda     filepick_s_swr,y
        beq     filepick_pa_named
        sta     fp_nm,x
        inx
        iny
        bne     filepick_pa_swr
filepick_pa_named:
        stx     fp_elen
        ; source: the absolute path, read on logical file 4
        lda     #<fp_clip
        sta     mos8(X16_P0)
        lda     #>fp_clip
        sta     mos8(X16_P1)
        ldy     #0
filepick_pa_slen:
        lda     fp_clip,y
        beq     filepick_pa_gotslen
        iny
        bne     filepick_pa_slen
filepick_pa_gotslen:
        sty     mos8(X16_P2)
        lda     #4
        sta     mos8(X16_P3)
        lda     #8
        sta     mos8(X16_P4)
        lda     #2
        sta     mos8(X16_P5)
        jsr     fio_open_read
        bcs     filepick_pa_failsrc
        ; destination on logical file 5, in whatever directory we are in
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        lda     fp_elen
        sta     mos8(X16_P2)
        lda     #5
        sta     mos8(X16_P3)
        lda     #8
        sta     mos8(X16_P4)
        lda     #2
        sta     mos8(X16_P5)
        jsr     fio_open_write
        bcs     filepick_pa_faildst
filepick_pa_block:
        ldx     #4                      ; read a block
        jsr     CHKIN
        ldy     #0
filepick_pa_read:
        jsr     CHRIN
        sta     fp_buf,y
        iny
        beq     filepick_pa_full        ; 256 bytes
        jsr     READST
        beq     filepick_pa_read
        sty     fp_cnt                  ; short block: the last one
        lda     #1
        sta     fp_tmp
        bra     filepick_pa_write
filepick_pa_full:
        sty     fp_cnt                  ; 0 means 256
        stz     fp_tmp
filepick_pa_write:
        ldx     #5
        jsr     CHKOUT
        ldy     #0
filepick_pa_out:
        lda     fp_buf,y
        jsr     CHROUT
        iny
        cpy     fp_cnt
        bne     filepick_pa_out
        lda     fp_tmp
        beq     filepick_pa_block
        ; done
        jsr     CLRCHN
        lda     #5
        jsr     CLOSE
        lda     #4
        jsr     CLOSE
        bra     filepick_ed_reread
filepick_pa_faildst:
        jsr     CLRCHN
        lda     #5
        jsr     CLOSE
        lda     #4
        jsr     CLOSE
        bra     filepick_pa_report
filepick_pa_failsrc:
        jsr     CLRCHN
        lda     #4
        jsr     CLOSE
filepick_pa_report:
        ; Say so. A silent failure here looks exactly like a key that
        ; does nothing, which is how this one hid upstream.
        lda     #<filepick_s_copyfail
        sta     mos8(X16_P0)
        lda     #>filepick_s_copyfail
        sta     mos8(X16_P1)
        jsr     filepick_ed_confirm     ; draws it and waits for a key
        jmp     filepick_loop

filepick_ed_reread:
        jsr     filepick_read
        stz     fp_sel
        stz     fp_top
        jmp     filepick_loop

; the selected entry's name -> fp_full, as an absolute path
filepick_path_of_sel:
        lda     fp_sel
        jsr     filepick_ent_fetch
        lda     #<fp_nm
        sta     mos8(X16_P0)
        lda     #>fp_nm
        sta     mos8(X16_P1)
        jmp     filepick_make_path

; =====================================================================
; Private carries -- upstream modules this tree has not ported yet.
; Each is a file-local copy of the upstream routine, byte-for-byte in
; behaviour; when the module arrives, delete the copy and .import.
; =====================================================================

; ---------------------------------------------------------------------
; The directory walker (upstream storage/dir.asm). A drive hands its
; directory over as a BASIC program listing; these walk it so the rest
; of the code never sees the quotes and link bytes.
; ---------------------------------------------------------------------

; dir_open -- open a directory for reading
;   in:  X16_P0/P1 = path address, X16_P2 = path length
;        (a length of 0 asks for "$", the current directory)
;        X16_P3    = device (usually 8)
;   out: carry set if the directory could not be opened
dir_open:
        lda     mos8(X16_P2)
        bne     dir_named
        lda     #1                      ; no path given: just "$"
        ldx     #<dir_dollar
        ldy     #>dir_dollar
        bra     dir_setnam
dir_named:
        ldx     mos8(X16_P0)
        ldy     mos8(X16_P1)
dir_setnam:
        jsr     SETNAM
        lda     #DIR_LFN
        ldx     mos8(X16_P3)
        ldy     #0                      ; secondary 0: the directory, not a file
        jsr     SETLFS
        jsr     OPEN
        bcs     dir_openbad
        ldx     #DIR_LFN
        jsr     CHKIN
        bcs     dir_openbad
        jsr     dir_getb                ; the two load-address bytes, discarded
        bcs     dir_openbad
        jsr     dir_getb
        bcs     dir_openbad
        clc
        rts
dir_openbad:
        sec
        rts

; dir_next -- read the next entry
;   in:  X16_P0/P1 = a buffer for the name, X16_P2 = its size (2-255)
;   out: carry SET if an entry was read, CLEAR at the end of the listing
dir_next:
        stz     dir_ty                  ; DIR_TYPE_NONE until the line says more
        ldx     #DIR_LFN                ; the caller may have used the channel
        jsr     CHKIN                   ; in between, so re-select it every time
        bcs     dir_no

        jsr     dir_getb                ; link
        bcs     dir_no
        sta     mos8(X16_T0)
        jsr     dir_getb
        bcs     dir_no
        ora     mos8(X16_T0)
        beq     dir_no                  ; a zero link is the end of the listing

        jsr     dir_getb                ; the line number is the block count,
        bcs     dir_no                  ; which nothing here wants
        jsr     dir_getb
        bcs     dir_no

        stz     mos8(X16_T1)            ; name bytes stored so far
        stz     mos8(X16_T2)            ; 0 before the name, 1 inside, 2 after
dir_text:
        jsr     dir_getb
        bcs     dir_endline             ; the file ended: keep what we have
        cmp     #0
        beq     dir_endline             ; and $00 ends the line properly
        ldx     mos8(X16_T2)
        cpx     #1
        beq     dir_inname
        cpx     #2
        beq     dir_after
        cmp     #CH_QUOTE               ; before the name: find the quote
        bne     dir_text
        inc     mos8(X16_T2)
        bra     dir_text

dir_inname:
        cmp     #CH_QUOTE               ; the closing quote ends the name
        beq     dir_closed
        ldx     mos8(X16_T1)
        inx
        cpx     mos8(X16_P2)            ; room for this byte AND a terminator?
        bcs     dir_text                ; no: drop it, but keep parsing the type
        ldy     mos8(X16_T1)            ; CHRIN is free to clobber Y, so load it
        sta     (X16_P0),y              ; here rather than holding it across
        inc     mos8(X16_T1)
        bra     dir_text
dir_closed:
        lda     #2
        sta     mos8(X16_T2)
        bra     dir_text

dir_after:
        cmp     #CH_SPACE               ; the first non-space after the name is
        beq     dir_text                ; the type
        ldx     dir_ty
        bne     dir_text                ; already classified this line
        jsr     dir_classify
        bra     dir_text

dir_endline:
        ldy     mos8(X16_T1)
        lda     #0
        sta     (X16_P0),y              ; NUL-terminate within the buffer
        sec                             ; an entry was read
        rts
dir_no:
        clc
        rts

; The first letter is enough: PRG, SEQ, USR, REL, DIR and HOST do not
; collide. A suffix like PRG< (locked) classifies the same way.
dir_classify:
        cmp     #CH_P
        beq     dir_t_prg
        cmp     #CH_S
        beq     dir_t_seq
        cmp     #CH_U
        beq     dir_t_usr
        cmp     #CH_R
        beq     dir_t_rel
        cmp     #CH_D
        beq     dir_t_dir
        cmp     #CH_H
        beq     dir_t_host
        rts
dir_t_prg:
        lda     #DIR_TYPE_PRG
        bra     dir_setty
dir_t_seq:
        lda     #DIR_TYPE_SEQ
        bra     dir_setty
dir_t_usr:
        lda     #DIR_TYPE_USR
        bra     dir_setty
dir_t_rel:
        lda     #DIR_TYPE_REL
        bra     dir_setty
dir_t_dir:
        lda     #DIR_TYPE_DIR
        bra     dir_setty
dir_t_host:
        lda     #DIR_TYPE_HOST
dir_setty:
        sta     dir_ty
        rts

; dir_type -- what the entry dir_next just read is
dir_type:
        lda     dir_ty
        rts

; dir_close -- finished with the directory
dir_close:
        jsr     CLRCHN
        lda     #DIR_LFN
        jmp     CLOSE

; one byte from the directory channel; carry set if the stream ended
dir_getb:
        jsr     CHRIN
        sta     mos8(X16_T3)
        jsr     READST
        cmp     #0
        bne     dir_getb_end
        lda     mos8(X16_T3)
        clc
        rts
dir_getb_end:
        sec
        rts

; ---------------------------------------------------------------------
; Direct text-map access (upstream video/screen.asm). CHROUT costs
; several hundred cycles a character; a full-screen panel cannot afford
; that, so these write VERA's tile map itself. They do not scroll, do
; not wrap, and do not move the CHROUT cursor.
; ---------------------------------------------------------------------

; screen_addr -- point VERA port 0 at a character cell
;   in:  X = row, Y = column
;
; Reads L1_MAPBASE and L1_CONFIG, so it follows whatever screen mode is
; live rather than assuming the 80x60 default. Leaves ADDRSEL = 0 and
; the increment set to 1.
screen_addr:
        jsr     screen_addr_calc
        vera_addrsel 0
        lda     mos8(X16_T0)
        sta     VERA_ADDR_L
        lda     mos8(X16_T1)
        sta     VERA_ADDR_M
        lda     mos8(X16_T2)
        and     #$01                    ; bit 16 of the address
        ora     #$10                    ; increment 1
        sta     VERA_ADDR_H
        rts

; address of (X = row, Y = column) into X16_T0/T1/T2, port untouched
screen_addr_calc:
        sty     mos8(X16_T5)            ; column
        stx     mos8(X16_T6)            ; row

        lda     VERA_L1_MAPBASE         ; map base = MAPBASE << 9
        asl     a                       ; carry = bit 16
        sta     mos8(X16_T1)            ; mid
        lda     #0
        rol     a
        sta     mos8(X16_T2)            ; high
        stz     mos8(X16_T0)            ; low

        lda     VERA_L1_CONFIG          ; MAP_WIDTH: 0=32 1=64 2=128 3=256 tiles
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #3
        clc
        adc     #6                      ; bytes per row = 2 << (5 + width)
        tay

        lda     mos8(X16_T6)            ; row << Y
        sta     mos8(X16_T3)
        stz     mos8(X16_T4)
.Lscreen_addr_calc_shift:
        asl     mos8(X16_T3)
        rol     mos8(X16_T4)
        dey
        bne     .Lscreen_addr_calc_shift

        clc                             ; base += row * stride
        lda     mos8(X16_T0)
        adc     mos8(X16_T3)
        sta     mos8(X16_T0)
        lda     mos8(X16_T1)
        adc     mos8(X16_T4)
        sta     mos8(X16_T1)
        bcc     .Lscreen_addr_calc_nocarry1
        inc     mos8(X16_T2)
.Lscreen_addr_calc_nocarry1:
        lda     mos8(X16_T5)            ; base += column * 2
        asl     a
        tax
        lda     #0
        rol     a
        tay
        txa
        clc
        adc     mos8(X16_T0)
        sta     mos8(X16_T0)
        tya
        adc     mos8(X16_T1)
        sta     mos8(X16_T1)
        bcc     .Lscreen_addr_calc_nocarry2
        inc     mos8(X16_T2)
.Lscreen_addr_calc_nocarry2:
        rts

; screen_scode -- PETSCII to screen code (the standard CBM folding)
screen_scode:
        cmp     #$20
        bcc     .Lscreen_scode_plus80                 ; $00-$1F
        cmp     #$40
        bcc     .Lscreen_scode_same                   ; $20-$3F
        cmp     #$60
        bcc     .Lscreen_scode_minus40                ; $40-$5F
        cmp     #$80
        bcc     .Lscreen_scode_minus20                ; $60-$7F
        cmp     #$A0
        bcc     .Lscreen_scode_plus40                 ; $80-$9F
        cmp     #$C0
        bcc     .Lscreen_scode_minus40                ; $A0-$BF
        sec                             ; $C0-$FF
        sbc     #$80
.Lscreen_scode_same:
        rts
.Lscreen_scode_plus80:
        clc
        adc     #$80
        rts
.Lscreen_scode_minus40:
        sec
        sbc     #$40
        rts
.Lscreen_scode_minus20:
        sec
        sbc     #$20
        rts
.Lscreen_scode_plus40:
        clc
        adc     #$40
        rts

; screen_blit -- write a run of characters, all one colour
;   in:  X16_P0/P1 = source, A = count (1-255), X = colour byte
;
; Port 0 must already point at the first cell (screen_addr); it is left
; pointing just past the last one, so runs can be chained.
screen_blit:
        sta     mos8(X16_T7)            ; count
        stx     mos8(X16_T3)            ; colour
        ldy     #0
.Lscreen_blit_loop:
        lda     (X16_P0),y
        jsr     screen_scode
        sta     VERA_DATA0
        lda     mos8(X16_T3)
        sta     VERA_DATA0
        iny
        cpy     mos8(X16_T7)
        bne     .Lscreen_blit_loop
        rts

; screen_blitfill -- write a run of one repeated character
;   in:  A = count (1-255), X = colour byte, Y = character (PETSCII)
screen_blitfill:
        sta     mos8(X16_T7)            ; count
        stx     mos8(X16_T3)            ; colour
        tya
        jsr     screen_scode
        sta     mos8(X16_T4)            ; screen code, converted once
        ldy     #0
.Lscreen_blitfill_loop:
        lda     mos8(X16_T4)
        sta     VERA_DATA0
        lda     mos8(X16_T3)
        sta     VERA_DATA0
        iny
        cpy     mos8(X16_T7)
        bne     .Lscreen_blitfill_loop
        rts

; ---------------------------------------------------------------------
; key_wait (upstream input/input.asm) -- block until a key is pressed
; ---------------------------------------------------------------------
key_wait:
.Lkey_wait_loop:
        jsr     GETIN
        beq     .Lkey_wait_loop
        rts

; ---------------------------------------------------------------------
; Streamed-copy openers (upstream storage/fileio.asm), for paste.
;       X16_P0/P1 = filename, X16_P2 = its length,
;       X16_P3 = logical file, X16_P4 = device, X16_P5 = secondary
; ---------------------------------------------------------------------
fio_open_read:
        jsr     fio_open_named
        bcs     .Lfio_open_read_done
        ldx     mos8(X16_P3)
        jmp     CHKIN
.Lfio_open_read_done:
        rts

fio_open_write:
        jsr     fio_open_named
        bcs     .Lfio_open_write_done
        ldx     mos8(X16_P3)
        jmp     CHKOUT
.Lfio_open_write_done:
        rts

fio_open_named:
        lda     mos8(X16_P2)
        ldx     mos8(X16_P0)
        ldy     mos8(X16_P1)
        jsr     SETNAM
        lda     mos8(X16_P3)
        ldx     mos8(X16_P4)
        ldy     mos8(X16_P5)
        jsr     SETLFS
        jmp     OPEN

; =====================================================================
; data
; =====================================================================

        .section .rodata,"a",@progbits

filepick_root:
        .byte   CH_SLASH, 0
filepick_headdef:
        .byte   $46,$49,$4C,$45,$53,$20,$49,$4E,$20,0   ; "files in "
filepick_alldef:
        .byte   CH_STAR, CH_DOT, CH_STAR, 0     ; "*.*"
filepick_footdef:
        .byte   $44,$4F,$55,$42,$4C,$45,$20,$43,$4C,$49,$43,$4B,$20,$4F,$50,$45,$4E,$53,$20,$20,$20,$45,$53,$43,$20,$43,$4C,$4F,$53,$45,$53,0   ; "double click opens   esc closes"
filepick_dirtag:
        .byte   $5B,$44,$49,$52,$5D,$20,0   ; "[dir] "
filepick_dattag:
        .byte   $5B,$44,$41,$54,$5D,$20,0   ; "[dat] "
filepick_blanktag:
        .byte   $20,$20,$20,$20,$20,$20,0   ; "      "
filepick_closebox:
        .byte   $20,$58,$20,0   ; " x "
dir_dollar:
        .byte   CH_DOLLAR                       ; "$"

filepick_s_newdir:
        .byte   $4E,$45,$57,$20,$46,$4F,$4C,$44,$45,$52,$3A,$20,0   ; "new folder: "
filepick_s_rename:
        .byte   $52,$45,$4E,$41,$4D,$45,$20,$54,$4F,$3A,$20,0   ; "rename to: "
filepick_s_delete:
        .byte   $44,$45,$4C,$45,$54,$45,$3F,$20,$59,$2F,$4E,$3A,$20,0   ; "delete? y/n: "
filepick_s_copyfail:
        .byte   $43,$4F,$50,$59,$20,$46,$41,$49,$4C,$45,$44,$20,$2D,$2D,$20,$50,$52,$45,$53,$53,$20,$41,$20,$4B,$45,$59,0   ; "copy failed -- press a key"
filepick_s_swr:
        ; ",S,W" as explicit bytes: under -t cx16 the literal would
        ; translate 'S'/'W' to $D3/$D7, which is not what a drive reads
        ; as "sequential, write".
        .byte   CH_COMMA, CH_S, CH_COMMA, CH_W, 0

; The configuration has nonzero defaults, so it lives in DATA, not BSS.
        .section .data,"aw",@progbits

fp_vram:    .word $2000         ; the listing: VRAM, not banked RAM
fp_vramh:   .byte $01           ; ...$12000 by default, clear of the text map
fp_filt:    .word 0             ; 0 means "*.*"
fp_prim:    .word 0             ; 0 means "the same as the filter"
fp_head:    .word 0             ; 0 means "files in "
fp_foot:    .word 0
fp_apanel:  .byte $F6           ; blue on light grey
fp_abar:    .byte $F6
fp_asel:    .byte $6F           ; inverted
fp_under:   .word $4000         ; the save-under, also VRAM: $14000
fp_underh:  .byte $01
fp_undon:   .byte 0             ; 0 = keep nothing
fp_chset:   .byte 3             ; PET upper/lower; 255 leaves it alone
fp_startat: .word 0             ; 0 means "/"
fp_rows:    .byte 40
fp_left:    .byte 6
fp_wide:    .byte 68
fp_scrw:    .byte 80
fp_scrh:    .byte 60

        .section .bss,"aw",@nobits

fp_curdir:  .zero  64
fp_full:    .zero  64
fp_nm:      .zero  40
fp_nent:    .zero  1
fp_sel:     .zero  1
fp_top:     .zero  1
fp_down:    .zero  1
fp_lastck:  .zero  2
fp_lastidx: .zero  1
fp_saved:   .zero  1
fp_pass:    .zero  1
fp_act:     .zero  1
fp_key:     .zero  1
fp_row:     .zero  1
fp_idx:     .zero  1
fp_attr:    .zero  1
fp_cnt:     .zero  1
fp_tmp:     .zero  1
fp_tmp2:    .zero  1
fp_kind:    .zero  1              ; an entry's type, which filepick_ent must not eat
fp_src:     .zero  2              ; scratch pointers, kept out of the ZP
fp_dst:     .zero  2              ; block so a library call cannot eat them
fp_pat:     .zero  2
fp_ptr:     .zero  2

fp_clip:    .zero  64             ; the file 'c' remembered, absolute
fp_clipok:  .zero  1
fp_buf:     .zero  256            ; what a copy moves at a time
fp_elen:    .zero  1              ; length of the text being edited

dir_ty:     .zero  1              ; the private directory walker's state
