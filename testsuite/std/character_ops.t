//@ use std/option.t std/io.t std/character.t
//@ expect stdout
//@ | space=1 digit=1 alpha=1 alnum=1
//@ | upper=1 lower=1 hex=1 punct=1
//@ | print=1 control=1 ascii=0
//@ | toupper=65 tolower=122
//@ | digitval=7 hexval=15 notdigit=none
// ASCII-only classification, replacing C's <ctype.h>. Takes u8 rather than i32,
// so there is no EOF-vs-byte confusion and no UB for a negative char.
fn main() i32 {
    println("space=% digit=% alpha=% alnum=%",
        (i32)is_space(' '), (i32)is_digit('7'), (i32)is_alpha('q'), (i32)is_alnum('4'))
    println("upper=% lower=% hex=% punct=%",
        (i32)is_upper('A'), (i32)is_lower('z'), (i32)is_hex_digit('f'), (i32)is_punct('!'))
    println("print=% control=% ascii=%",
        (i32)is_print('x'), (i32)is_control('\n'), (i32)is_ascii((u8)200))
    // u8 formats as a NUMBER, not a character -- print_val has no char case
    println("toupper=% tolower=%", to_upper('a'), to_lower('Z'))
    match digit_value('7') { {.Some = v} { print("digitval=%", (i32)v) }  .None {} }
    match hex_value('f')   { {.Some = v} { print(" hexval=%", (i32)v) }  .None {} }
    match digit_value('x') { {.Some = v} { print(" BAD\n") }  .None { println(" notdigit=none") } }
    return 0
}
