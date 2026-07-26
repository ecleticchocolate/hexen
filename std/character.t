// character - byte classification and case conversion
//
// ASCII only, deliberately. Every byte above 0x7F is not a letter, not a digit
// and not whitespace as far as this file is concerned -- a UTF-8 continuation
// byte is none of those things, and pretending otherwise is how a naive
// to_upper() corrupts a multi-byte character. A UTF-8-aware layer belongs on
// top of this, not inside it.
//
// Replaces C's <ctype.h>, minus its two design mistakes: these take a `u8` (so
// there is no EOF-vs-byte confusion, and no undefined behaviour for a negative
// char), and they are not locale-dependent.

pub fn is_space(u8 c) bool {
    match c {
        ' '  { return true }
        '\t' { return true }
        '\n' { return true }
        '\r' { return true }
        else { return false }
    }
}

pub fn is_digit(u8 c) bool {
    if c < '0' { return false }
    if c > '9' { return false }
    return true
}

pub fn is_upper(u8 c) bool {
    if c < 'A' { return false }
    if c > 'Z' { return false }
    return true
}

pub fn is_lower(u8 c) bool {
    if c < 'a' { return false }
    if c > 'z' { return false }
    return true
}

pub fn is_alpha(u8 c) bool {
    if is_upper(c) { return true }
    if is_lower(c) { return true }
    return false
}

pub fn is_alnum(u8 c) bool {
    if is_alpha(c) { return true }
    return is_digit(c)
}

// 0-9, a-f, A-F.
pub fn is_hex_digit(u8 c) bool {
    if is_digit(c) { return true }
    if c >= 'a' { if c <= 'f' { return true } }
    if c >= 'A' { if c <= 'F' { return true } }
    return false
}

// Printable and not a space: the classic ispunct.
pub fn is_punct(u8 c) bool {
    if c <= 32 { return false }
    if c >= 127 { return false }
    if is_alnum(c) { return false }
    return true
}

// Any printable character, space included.
pub fn is_print(u8 c) bool {
    if c < 32 { return false }
    if c >= 127 { return false }
    return true
}

// A control byte: below space, or DEL.
pub fn is_control(u8 c) bool {
    if c < 32 { return true }
    return c == 127
}

// True for every byte the ASCII table defines. A UTF-8 lead or continuation
// byte answers false, which is the honest answer for a byte-oriented API.
pub fn is_ascii(u8 c) bool {
    return c < 128
}

pub fn to_upper(u8 c) u8 {
    if is_lower(c) {
        return c - 32
    }
    return c
}

pub fn to_lower(u8 c) u8 {
    if is_upper(c) {
        return c + 32
    }
    return c
}

// The numeric value of a digit byte, or .None if it is not one. Returning an
// Option rather than a sentinel is the same rule every fallible read in the
// std follows -- there is no "-1 means no" to remember.
pub fn digit_value(u8 c) Option[u32] {
    if is_digit(c) { return {.Some = (u32)(c - '0')} }
    return .None
}

// The numeric value of a hex digit byte (0-15), or .None.
pub fn hex_value(u8 c) Option[u32] {
    if is_digit(c) { return {.Some = (u32)(c - '0')} }
    if c >= 'a' { if c <= 'f' { return {.Some = (u32)(c - 'a') + 10} } }
    if c >= 'A' { if c <= 'F' { return {.Some = (u32)(c - 'A') + 10} } }
    return .None
}
