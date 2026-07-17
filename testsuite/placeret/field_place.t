//@ expect val 20
struct P{i32 x} fn main() i32 { P s; i32* p = &(s.x = 8); *p = 20; return s.x }
