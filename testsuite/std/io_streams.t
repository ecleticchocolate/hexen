//@ use std/option.t std/io.t
//@ expect stdout
//@ | a=1 b=2
//@ | no holes
//@ | x=42
//@ | struct=P { x: 1, y: 2 }
//@ | array=[1, 2, 3]
//@ | 99
// print/println go to stdout, eprint/eprintln to STDERR. They used to share one
// path, so `prog > out.txt` swallowed diagnostics. The streams are also cached:
// fdopen() makes a NEW buffered stream per call, and opening one per print gave
// each line its own buffer -- output came out in reverse order.
struct P { i32 x  i32 y }
fn main() i32 {
    println("a=% b=%", 1, 2)
    println("no holes")
    println("x=%", 42)
    P p = {.x = 1, .y = 2}
    println("struct=%", p)
    i32[3] a = {1, 2, 3}
    println("array=%", a)
    put(99)
    eprintln("this goes to stderr, not stdout")
    return 0
}
