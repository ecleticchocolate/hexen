//@ expect err drop the '.'
// The leading `.` was the old assertion marker. A bare name now means exactly
// that, so the dot is redundant and rejected -- one spelling, not two.
fn p[T]() i32 { match T { struct { i32 .a } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[struct {i32 a}]() }
