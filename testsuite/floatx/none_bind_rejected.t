//@ expect err variant has none
enum N{f64 Val None} fn main()i32{N n=.None{0} match n{.Val{v} {return 0} .None{x} {return 1}} return -1}
