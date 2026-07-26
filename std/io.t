// ---------------------------------------------------------
// io — reflective formatted output using type packs & reflection
// ---------------------------------------------------------
// Replaces C's printf/fprintf with type-safe, reflective alternatives.
// Each `%` in the format string consumes the next argument and prints it
// according to its TYPE using compile-time reflection.
//
// Automatically handles:
//   - Primitives: i32, u32, i64, u64, f32, f64, bool, u8, etc.
//   - Strings: u8*
//   - Custom Printing: Any type with a `fn __print()` method
//   - Pointers: P* (prints 0x... or null)
//   - Function Pointers: fn(Args...) R (prints <fn 0x...>)
//   - Arrays: Elem[N] (prints [x, y, z])
//   - Structs: struct { Any... } (prints StructName { field: val, ... })
//   - Enums: enum { Any... } (prints EnumName)
//   - Unions: union { Any... } (prints UnionName)
//
// Uses `%%` to emit a literal `%`.

extern fn putchar(i32 c) i32
extern fn printf(u8* fmt, ...) i32
extern fn fprintf(u8* stream, u8* fmt, ...) i32
extern fn fputc(i32 c, u8* stream) i32
extern fn fdopen(i32 fd, u8* mode) u8*
extern fn fgets(u8* buf, i32 size, u8* stream) u8*
extern fn fflush(u8* stream) i32
extern fn strtol(u8* s, u8** end, i32 base) i64
extern fn strtod(u8* s, u8** end) f64

// The three standard streams, opened on their well-known descriptors. libc's
// `stdout`/`stderr` are GLOBALS, not functions, so they cannot be reached
// through `extern fn`; fdopen on the descriptor gets the same stream without
// needing global-symbol imports.
// Cached: fdopen() creates a NEW buffered stream every call, so calling it per
// print gave each line its own buffer and they flushed in the wrong order --
// output came out reversed. One stream per descriptor, opened once.
u8* g_stdout = null
u8* g_stderr = null
u8* g_stdin  = null

pub fn stdout_stream() u8* {
    if g_stdout == null { g_stdout = fdopen(1, "w") }
    return g_stdout
}
pub fn stderr_stream() u8* {
    if g_stderr == null { g_stderr = fdopen(2, "w") }
    return g_stderr
}
pub fn stdin_stream() u8* {
    if g_stdin == null { g_stdin = fdopen(0, "r") }
    return g_stdin
}

// Helper to print struct fields sequentially via compile-time reflection
fn print_struct_fields[Orig, Walk, u32 N](u8* out, Orig* obj) void {
    match Walk {
        struct { H; Rest... } {
            if N > 0 { fprintf(out, ", ") }
            fprintf(out, "%s: ", nameof(Orig, N))
            u8* base = (u8*)obj
            u64 off = offsetof(Orig, N)
            H* field_ptr = (H*)(base + off)
            print_val[H](out, *field_ptr)
            print_struct_fields[Orig, Rest, N + 1](out, obj)
        }
        struct {} {}
    }
}

// ---------------------------------------------------------
// print_val — emit a single value by its type via reflection
// ---------------------------------------------------------

pub fn print_val[T](u8* out, T val) void {
    match T {
        // 1. Custom Print Hook: if type has `fn __print()`, let it format itself!
        impl { fn __print() } {
            val.__print()
        }

        // 2. C-Strings
        u8*  {
            if (u8*)val == null { fprintf(out, "null") }
            else { fprintf(out, "%s", val) }
        }

        // 3. Numeric Primitives & Booleans
        i32  { fprintf(out, "%d", val) }
        u32  { fprintf(out, "%u", val) }
        i64  { fprintf(out, "%lld", val) }
        u64  { fprintf(out, "%llu", val) }
        f64  { fprintf(out, "%g", val) }
        f32  { fprintf(out, "%g", (f64)val) }
        bool {
            if val { fprintf(out, "true") }
            else   { fprintf(out, "false") }
        }
        u8   { fprintf(out, "%u", (u32)val) }
        i8   { fprintf(out, "%d", (i32)val) }
        u16  { fprintf(out, "%u", (u32)val) }
        i16  { fprintf(out, "%d", (i32)val) }

        // 4. Function Pointers: fn(Args...) R
        fn(Args...) R {
            if (u64)val == 0 { fprintf(out, "<fn null>") }
            else { fprintf(out, "<fn 0x%llx>", (u64)val) }
        }

        // 5. Generic Pointer: P*
        P* {
            if (void*)val == null { fprintf(out, "null") }
            else { fprintf(out, "0x%llx", (u64)val) }
        }

        // 6. Generic Array: Elem[N]
        Elem[N] {
            fprintf(out, "[")
            for u32 i = 0 to N {
                if i > 0 { fprintf(out, ", ") }
                print_val[Elem](out, val[i])
            }
            fprintf(out, "]")
        }

        // 7. Generic Struct Shape: reflect over fields automatically
        struct { Any... } {
            fprintf(out, "%s { ", nameof(T))
            print_struct_fields[T, T, 0](out, &val)
            fprintf(out, " }")
        }

        // 8. Enums
        enum { Any... } {
            fprintf(out, "%s", nameof(T))
        }

        // 9. Unions
        union { Any... } {
            fprintf(out, "%s", nameof(T))
        }

        else {
            fprintf(out, "<??>")
        }
    }
}

// ---------------------------------------------------------
// Core: format-string walker with compile-time pack peeling
// ---------------------------------------------------------

fn print_impl[T](u8* out, u8* fmt, T args) void {
    i32 i = 0
    while fmt[i] != 0 {
        if fmt[i] == '%' {
            if fmt[i + 1] == '%' {
                fputc(37, out)
                i = i + 2
                continue
            }

            match T {
                struct { H; Rest... } {
                    print_val(out, args._0)
                    print_impl(out, fmt + i + 1, (Rest)args)
                    return
                }
                struct {} {
                    fprintf(out, "[MISSING]")
                    i = i + 1
                    continue
                }
            }
        } else {
            fputc((i32)fmt[i], out)
            i = i + 1
        }
    }

    match T {
        struct { H; Rest... } {
            fprintf(out, "[EXTRA]")
        }
        struct {} { }
    }
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------

pub fn print[T](u8* fmt, T... args) void {
    print_impl(stdout_stream(), fmt, args)
}

pub fn println[T](u8* fmt, T... args) void {
    u8* o = stdout_stream()
    print_impl(o, fmt, args)
    fputc(10, o)
}

// Diagnostics go to STDERR, which is what makes them survive a redirected
// stdout and interleave correctly with the program's own output. These used to
// call the same stdout path as print(), so `prog > out.txt` swallowed every
// error message.
pub fn eprint[T](u8* fmt, T... args) void {
    print_impl(stderr_stream(), fmt, args)
}

pub fn eprintln[T](u8* fmt, T... args) void {
    u8* e = stderr_stream()
    print_impl(e, fmt, args)
    fputc(10, e)
}

// Formatted output to any stream -- a File's handle, a pipe, anything libc
// hands back. Same reflective `%` walker as print(), no second implementation.
pub fn fprint[T](u8* out, u8* fmt, T... args) void {
    print_impl(out, fmt, args)
}

pub fn fprintln[T](u8* out, u8* fmt, T... args) void {
    print_impl(out, fmt, args)
    fputc(10, out)
}

// One value, then a newline.
pub fn put[T](T val) void {
    u8* o = stdout_stream()
    print_val(o, val)
    fputc(10, o)
}
