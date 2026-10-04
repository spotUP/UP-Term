/* hl_langs -- the languages hl knows, and how a file finds its language.
 * One table per language (hl_lex.h struct hl_lang): keyword lists are
 * blank-separated (keywords, types, builtins), hashed on first use.
 * Adding a language: a row here, its fixture in tests/test_hl.c. */
#include <string.h>
#include "hl_lex.h"

#define NONE { 0, 0 }

const hl_lang hl_langs[] = {
    { "c", "c cpp c++ cxx cc h hpp objc", "c h cc cpp cxx c++ hpp hxx hh h++ ino", "", "",
      { "auto break case const continue default do else enum extern for goto if inline register restrict "
        "return sizeof static struct switch typedef union volatile while alignas alignof and asm catch "
        "class constexpr const_cast consteval constinit co_await co_return co_yield decltype delete "
        "dynamic_cast explicit export friend mutable namespace new noexcept not operator or private "
        "protected public reinterpret_cast static_assert static_cast template throw try typeid typename ",
        "using virtual xor override final _Alignas _Alignof _Atomic _Generic _Noreturn _Static_assert "
        "_Thread_local",
        "void char short int long float double signed unsigned bool _Bool _Complex size_t ssize_t "
        "ptrdiff_t intptr_t uintptr_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t "
        "wchar_t char16_t char32_t FILE va_list BYTE UBYTE WORD UWORD LONG ULONG BOOL APTR BPTR BSTR "
        "STRPTR CONST_STRPTR TEXT VOID FLOAT DOUBLE SHORT USHORT IPTR UQUAD QUAD std string vector map",
        0,
        "NULL TRUE FALSE true false nullptr this stdin stdout stderr errno EOF",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "\"'", "", "", '\\', "", "",
      HLF_CPP | HLF_FUNC, HLM_CODE },

    { "asm", "asm 68k m68k vasm devpac s assembly assembler", "s asm a68 i 68k sx", "", "",
      { "abcd add adda addi addq addx and andi asl asr bcc bcs beq bge bgt bhi bhs ble blo bls blt bmi "
        "bne bpl bvc bvs bra bsr bchg bclr bset btst bfchg bfclr bfexts bfextu bfffo bfins bfset bftst "
        "bkpt callm cas cas2 chk chk2 clr cmp cmpa cmpi cmpm cmp2 dbcc dbcs dbeq dbf dbra dbge dbgt "
        "dbhi dble dbls dblt dbmi dbne dbpl dbt dbvc dbvs divs divsl divu divul eor eori exg ext extb "
        "illegal jmp jsr lea link lsl lsr move movea movec movem movep moveq moves move16 muls mulu nbcd ",
        "neg negx nop not or ori pack pea reset rol ror roxl roxr rtd rte rtm rtr rts sbcd scc scs seq sf "
        "sge sgt shi sle sls slt smi sne spl st stop sub suba subi subq subx svc svs swap tas trap trapv "
        "tst unlk unpk fabs fadd fbeq fbne fbgt fbge fblt fble fbra fcmp fdiv fint fintrz fmove fmovem "
        "fmul fneg fsqrt fsub ftst fsin fcos fetox flogn cinva cinvl cpusha cpushl pflusha pflush pmove",
        "align assert blk bss bss_c bss_f code code_c code_f cnop data data_c data_f dc dcb ds else "
        "elseif end endc endif endm endr equ equr even fail fpu idnt if ifb ifc ifd ifeq ifge ifgt ifle "
        "iflt ifnb ifnc ifnd ifne incbin incdir include list machine macro mc68000 mc68010 mc68020 "
        "mc68030 mc68040 mc68060 nolist offset org output printt printv public reg rept rs rsreset rsset "
        "section set so text xdef xref opt near far basereg endb rorg fo clrfo setfo mexit",
        0,
        "d0 d1 d2 d3 d4 d5 d6 d7 a0 a1 a2 a3 a4 a5 a6 a7 sp pc ccr sr usp ssp vbr cacr caar msp isp "
        "sfc dfc fp0 fp1 fp2 fp3 fp4 fp5 fp6 fp7 fpcr fpsr fpiar",
        0 },
      { ";", 0 }, NONE, NONE, "'\"", "", "'\"", 0, "._@", "",
      HLF_NOCASE | HLF_HEXDOLLAR | HLF_QQ, HLM_ASM },

    { "e", "e amigae", "e", "", "",
      { "AND BUT CASE CONST DEC DEF DEFAULT DO ELSE ELSEIF END ENDFOR ENDIF ENDLOOP ENDOBJECT ENDPROC "
        "ENDSELECT ENDWHILE ENUM EXCEPT EXIT FOR HANDLE IF INC IS JUMP LOOP MODULE NEW OBJECT OF OPT OR "
        "PROC RAISE REPEAT RETURN SELECT SET STEP SUPER THEN TO UNTIL WHILE EXPORT PRIVATE PUBLIC "
        "LIBRARY NOT",
        0,
        "LONG INT CHAR PTR ARRAY LIST STRING REAL",
        0,
        "TRUE FALSE NIL WriteF PrintF StringF StrCopy StrAdd StrCmp StrLen EstrLen String List New "
        "Dispose DisposeLink Val InStr UpperStr LowerStr TrimStr MidStr RightStr Mod Abs Max Min Bounds "
        "Even Odd Rnd CtrlC FileLength ReadStr Out Inp Mouse MouseX MouseY Plot Line Box TextF Colour "
        "SetStdRast OpenW CloseW OpenS CloseS Gadget WaitIMessage MsgCode stdout stdin conout arg "
        "wbmessage exception exceptioninfo Throw ReThrow",
        0 },
      { "->", 0 }, { "/*", 0 }, { "*/", 0 }, "'\"", "", "", '\\', "", "",
      HLF_NEST1 | HLF_HEXDOLLAR | HLF_FUNC, HLM_CODE },

    { "python", "python py python3 py3 gyp", "py pyw pyi", "sconstruct sconscript", "python python2 python3 pypy",
      { "and as assert async await break class continue def del elif else except finally for from "
        "global if import in is lambda nonlocal not or pass raise return try while with yield match case",
        0,
        "int float str bytes bool list dict set tuple object complex frozenset bytearray type",
        0,
        "None True False self cls print len range open enumerate zip map filter sorted reversed min max "
        "sum abs any all repr format input iter next hasattr getattr setattr isinstance issubclass super "
        "id hex ord chr round divmod vars dir globals locals __name__ __main__ __init__ __file__ "
        "Exception ValueError TypeError KeyError IndexError RuntimeError StopIteration OSError IOError "
        "NotImplementedError AttributeError ImportError",
        0 },
      { "#", 0 }, NONE, NONE, "'\"", "", "", '\\', "", "",
      HLF_TRIPLE | HLF_STRPREFIX | HLF_ATSIGN | HLF_FUNC, HLM_CODE },

    { "sh", "sh shell bash zsh ksh vsh shellscript console shell-session",
      "sh bash zsh ksh vsh command", "vshrc .vshrc .bashrc .bash_profile .profile .zshrc .kshrc configure pkgbuild",
      "sh bash zsh ksh dash vsh ash",
      { "if then else elif fi for while until do done case esac in function select time return break "
        "continue",
        0,
        "local export readonly declare typeset unset shift",
        0,
        "echo printf cd pwd read test set source alias unalias exit exec eval trap wait jobs fg bg kill "
        "true false which type command builtin let getopts umask ulimit hash stack",
        0 },
      { "#", 0 }, NONE, NONE, "'\"`", "'\"`", "'", '\\', "", "",
      HLF_DOLLAR | HLF_HASHWORD | HLF_HEREDOC | HLF_FUNC, HLM_CODE },

    { "amigados", "amigados dos ados amigashell script",
      "dos script", "startup-sequence user-startup shell-startup cli-startup network-startup", "",
      { "if else endif skip lab quit failat execute ask wait run",
        0,
        "warn error fail exists eq gt ge not val all quiet clone force noreq",
        0,
        "echo set setenv unset unsetenv getenv copy delete rename makedir makelink list dir type version "
        "resident path prompt endcli endshell newshell newcli mount protect search sort status break "
        "avail info date join why fault requestchoice requestfile lock relabel which assign cd setdate "
        "setclock setfont setkeyboard addbuffers changetaskpri conclip ed edit eval filenote iconx "
        "installer loadwb loadresource stack time",
        0 },
      { ";", 0 }, NONE, NONE, "\"", "", "", '*', "", "",
      HLF_NOCASE | HLF_AMIGADOS, HLM_CODE },

    { "arexx", "arexx rexx", "rexx rx", "", "rx rexx",
      { "address arg break by call do drop else end exit expose forever if interpret iterate leave nop "
        "numeric options otherwise parse pragma procedure pull push queue return say select shell signal "
        "then to trace until upper value var when while with",
        0,
        "",
        0,
        "abbrev abs addlib b2c bitand bitor bitxor c2b c2d c2x center centre close compare compress "
        "copies d2c d2x datatype date delay delstr delword digits eof exists export find form fuzz "
        "getclip import index insert lastpos left length lines max min open overlay pos random randu "
        "readch readln remlib reverse right seek setclip show sign sourceline space storage strip substr "
        "subword symbol time translate trim verify word wordindex wordlength words writech writeln x2c ",
        "x2d rc result sigl" },
      NONE, { "/*", 0 }, { "*/", 0 }, "'\"", "", "", 0, "", "",
      HLF_NOCASE | HLF_NEST1 | HLF_QQ | HLF_FUNC, HLM_CODE },

    { "lua", "lua", "lua", "", "lua luajit",
      { "and break do else elseif end for function goto if in local not or repeat return then until while",
        0,
        "",
        0,
        "nil true false self print pairs ipairs type tostring tonumber require pcall xpcall error assert "
        "setmetatable getmetatable rawget rawset select unpack next string table math io os coroutine "
        "debug _G _ENV",
        0 },
      { "--", 0 }, NONE, NONE, "'\"", "", "", '\\', "", "",
      HLF_LUALONG | HLF_FUNC, HLM_CODE },

    { "javascript", "javascript js typescript ts jsx tsx node mjs",
      "js mjs cjs jsx ts tsx mts cts", "", "node deno bun",
      { "break case catch class const continue debugger default delete do else export extends finally "
        "for from function if import in instanceof let new of return static super switch throw try "
        "typeof var void while with yield async await get set as implements interface package private "
        "protected public enum type namespace declare abstract readonly keyof infer is satisfies",
        0,
        "string number boolean any unknown never object symbol bigint Array Promise Map Set Record "
        "Partial Readonly Error Date RegExp",
        0,
        "true false null undefined this NaN Infinity console window document globalThis module require "
        "exports process JSON Math Object String Number Boolean Symbol",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "'\"`", "`", "", '\\', "$", "",
      HLF_REGEX | HLF_FUNC, HLM_CODE },

    { "json", "json jsonc json5", "json jsonc json5 geojson webmanifest", ".babelrc .eslintrc", "",
      { "",
        0,
        "",
        0,
        "true false null",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "\"", "", "", '\\', "", "",
      HLF_KEYSTR, HLM_CODE },

    { "yaml", "yaml yml", "yml yaml", ".clang-format", "",
      { "",
        0,
        "",
        0,
        "true false null yes no on off True False Null Yes No On Off TRUE FALSE NULL",
        0 },
      { "#", 0 }, NONE, NONE, "'\"", "'\"", "'", '\\', "-", ":",
      HLF_KEYLINE | HLF_YAML | HLF_HASHWORD | HLF_QQ, HLM_CODE },

    { "toml", "toml", "toml", "cargo.lock pipfile poetry.lock", "",
      { "",
        0,
        "",
        0,
        "true false inf nan",
        0 },
      { "#", 0 }, NONE, NONE, "\"'", "", "'", '\\', "-", "=",
      HLF_SECTION | HLF_KEYLINE | HLF_TRIPLE, HLM_CODE },

    { "ini", "ini conf cfg dosini properties up-term editorconfig gitconfig",
      "ini conf cfg prefs properties inf desktop service", "up-term .gitconfig .editorconfig .npmrc .gitmodules", "",
      { "",
        0,
        "",
        0,
        "true false yes no on off",
        0 },
      { ";", "#" }, NONE, NONE, "\"", "", "", 0, "-.", "=:",
      HLF_SECTION | HLF_KEYLINE | HLF_HASHWORD, HLM_CODE },

    { "make", "make makefile mk", "mk mak make", "makefile gnumakefile smakefile dmakefile", "make",
      { "ifeq ifneq ifdef ifndef else endif include -include sinclude define endef export unexport "
        "override vpath private undefine",
        0,
        "",
        0,
        "",
        0 },
      { "#", 0 }, NONE, NONE, "\"'", "", "'", '\\', "-", "",
      HLF_DOLLAR | HLF_MAKE, HLM_CODE },

    { "markdown", "markdown md", "md markdown mdown mkd mdx", "", "",
      { "",
        0,
        "",
        0,
        "",
        0 }, NONE, NONE, NONE, "", "", "", 0, "", "", 0, HLM_MARKDOWN },

    { "html", "html xhtml htm vue svelte", "html htm xhtml shtml vue svelte", "", "",
      { "",
        0,
        "",
        0,
        "",
        0 }, NONE, NONE, NONE, "", "", "", 0, "", "", 0, HLM_MARKUP },

    { "xml", "xml svg plist xsl", "xml svg xsl xslt plist rss atom xsd wsdl csproj vcxproj", "", "",
      { "",
        0,
        "",
        0,
        "",
        0 }, NONE, NONE, NONE, "", "", "", 0, "", "", 0, HLM_MARKUP },

    { "diff", "diff patch udiff", "diff patch rej", "", "",
      { "",
        0,
        "",
        0,
        "",
        0 }, NONE, NONE, NONE, "", "", "", 0, "", "", 0, HLM_DIFF },

    { "css", "css scss less sass", "css scss sass less", "", "",
      { "",
        0,
        "",
        0,
        "inherit initial unset auto none transparent currentcolor",
        0 },
      NONE, { "/*", 0 }, { "*/", 0 }, "'\"", "", "", '\\', "-", "",
      HLF_CSS | HLF_ATSIGN, HLM_CODE },

    { "rust", "rust rs", "rs", "", "",
      { "as async await break const continue crate dyn else enum extern fn for if impl in let loop match "
        "mod move mut pub ref return static struct super trait type union unsafe use where while yield "
        "macro_rules",
        0,
        "i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option "
        "Result Box Rc Arc RefCell Cell HashMap HashSet BTreeMap Self",
        0,
        "true false self Some None Ok Err",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "\"'", "\"", "", '\\', "", "",
      HLF_NEST1 | HLF_CHARLIT | HLF_STRPREFIX | HLF_MACROBANG | HLF_FUNC | HLF_CPP, HLM_CODE },

    { "go", "go golang", "go", "", "",
      { "break case chan const continue default defer else fallthrough for func go goto if import "
        "interface map package range return select struct switch type var",
        0,
        "bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint "
        "uint8 uint16 uint32 uint64 uintptr any",
        0,
        "true false nil iota append cap close complex copy delete imag len make new panic print println "
        "real recover",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "\"'`", "`", "`", '\\', "", "",
      HLF_FUNC, HLM_CODE },

    { "java", "java", "java", "", "",
      { "abstract assert break case catch class const continue default do else enum extends final "
        "finally for goto if implements import instanceof interface native new package private protected "
        "public return static strictfp super switch synchronized throw throws transient try volatile "
        "while var record sealed permits yield",
        0,
        "boolean byte char double float int long short void String Object Integer Long Double Float "
        "Boolean Character List Map Set ArrayList HashMap",
        0,
        "true false null this System",
        0 },
      { "//", 0 }, { "/*", 0 }, { "*/", 0 }, "\"'", "", "", '\\', "", "",
      HLF_TRIPLE | HLF_ATSIGN | HLF_FUNC, HLM_CODE }
};

const int hl_nlangs = (int)(sizeof(hl_langs) / sizeof(hl_langs[0]));

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* w (n bytes, any case) is one of list's blank-separated words */
static int listed(const char *list, const char *w, long n)
{
    const char *p = list;
    if (!p || n <= 0)
        return 0;
    while (*p) {
        const char *s;
        long i;
        while (*p == ' ')
            p++;
        s = p;
        while (*p && *p != ' ')
            p++;
        if (p - s == n) {
            for (i = 0; i < n && lower((unsigned char)w[i]) == s[i]; i++)
                ;
            if (i == n)
                return 1;
        }
    }
    return 0;
}

const hl_lang *hl_find(const char *name, long len)
{
    int i;
    for (i = 0; i < hl_nlangs; i++)
        if (listed(hl_langs[i].name, name, len) || listed(hl_langs[i].names, name, len))
            return &hl_langs[i];
    for (i = 0; i < hl_nlangs; i++)
        if (listed(hl_langs[i].exts, name, len))
            return &hl_langs[i];
    return 0;
}

static int starts(const char *s, long n, const char *w)
{
    long k = (long)strlen(w), i;
    if (n < k)
        return 0;
    for (i = 0; i < k; i++)
        if (lower((unsigned char)s[i]) != w[i])
            return 0;
    return 1;
}

static const hl_lang *by_interp(const char *s, long n)
{
    long i = 2, w, e;
    int k;
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    w = i;
    while (i < n && s[i] != ' ' && s[i] != '\t') {
        if (s[i] == '/' || s[i] == ':')
            w = i + 1;
        i++;
    }
    if (i - w == 3 && !memcmp(s + w, "env", 3)) {
        while (i < n && (s[i] == ' ' || s[i] == '\t'))
            i++;
        if (i < n && s[i] == '-') /* env -S */
            while (i < n && s[i] != ' ')
                i++;
        while (i < n && (s[i] == ' ' || s[i] == '\t'))
            i++;
        w = i;
        while (i < n && s[i] != ' ' && s[i] != '\t')
            i++;
    }
    e = i;
    /* python3.11 -> python3 -> python */
    while (e > w) {
        for (k = 0; k < hl_nlangs; k++)
            if (listed(hl_langs[k].interp, s + w, e - w))
                return &hl_langs[k];
        if ((s[e - 1] >= '0' && s[e - 1] <= '9') || s[e - 1] == '.')
            e--;
        else
            break;
    }
    return 0;
}

const hl_lang *hl_detect(const char *filename, const char *first, long firstlen)
{
    int i;
    if (filename && *filename) {
        const char *base = filename, *p, *dot = 0;
        long bl;
        for (p = filename; *p; p++)
            if (*p == '/' || *p == ':')
                base = p + 1;
        bl = (long)strlen(base);
        for (i = 0; i < hl_nlangs; i++)
            if (listed(hl_langs[i].files, base, bl))
                return &hl_langs[i];
        if (starts(base, bl, "makefile") || starts(base, bl, "gnumakefile"))
            return hl_find("make", 4);
        for (p = base; *p; p++)
            if (*p == '.')
                dot = p;
        if (dot && dot[1]) {
            for (i = 0; i < hl_nlangs; i++)
                if (listed(hl_langs[i].exts, dot + 1, (long)strlen(dot + 1)))
                    return &hl_langs[i];
        }
    }
    if (first && firstlen >= 2) {
        if (first[0] == '#' && first[1] == '!')
            return by_interp(first, firstlen);
        if (starts(first, firstlen, "<?xml"))
            return hl_find("xml", 3);
        if (starts(first, firstlen, "<!doctype html") || starts(first, firstlen, "<html"))
            return hl_find("html", 4);
        if (starts(first, firstlen, ".key") || starts(first, firstlen, ".bra"))
            return hl_find("amigados", 8);
        if (starts(first, firstlen, "diff ") || starts(first, firstlen, "--- ") ||
            (starts(first, firstlen, "from ") && firstlen > 45))
            return hl_find("diff", 4);
    }
    return 0;
}
