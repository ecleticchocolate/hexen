//@ expect stdout
//@ | Hello World: 42 3.140000
//@ | Extra: 1
//@ | [EXTRA_ARGS]Missing: 10 [MISSING_ARG]

extern fn putchar(i32 c) i32;
extern fn printf(u8* fmt, ...) i32;

fn print_val[T](T val) i32 {
    match T {
        i32 { return printf("%d", val) }
        f64 { return printf("%f", val) }
        u8* { return printf("%s", val) }
        i64 { return printf("%lld", val) }
    }
    return 0
}

fn safe_printf_impl[T](u8* fmt, T args) i32 {
    i32 i = 0
    while fmt[i] != 0 {
        if fmt[i] == '%' {
            if fmt[i+1] == '%' {
                i32 dummy = putchar('%')
                i = i + 2
                continue
            }
            
            // Consume an argument by recursively calling with (Rest)args
            match T {
                struct { H head  Rest... tail } {
                    i32 d = print_val(args._0)
                    return safe_printf_impl(fmt + i + 1, (Rest) args)
                }
                struct {} {
                    // We hit a '%' but ran out of variadic arguments!
                    i32 dummy = printf("[MISSING_ARG]")
                    i = i + 1
                    continue
                }
            }
        } else {
            i32 dummy = putchar((i32)fmt[i])
            i = i + 1
        }
    }
    
    // We reached the end of the format string. Check if there are unused arguments!
    match T {
        struct { H head  Rest... tail } {
            i32 dummy = printf("[EXTRA_ARGS]")
        }
        struct {} {
            // All arguments consumed perfectly!
        }
    }
    return 0
}

fn safe_printf[T](u8* fmt, T... args) i32 {
    return safe_printf_impl(fmt, args)
}

fn main() i32 {
    i32 a = safe_printf("Hello %: % %\n", "World", 42, 3.14)
    i32 b = safe_printf("Extra: %\n", 1, 2)
    i32 c = safe_printf("Missing: % %\n", 10)
    return 0
}
