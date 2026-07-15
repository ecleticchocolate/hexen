//@ expect val 99
fn f()u32{for u32 i=0 to 3{if i==1{return 99}} return 0} const u32 R=f() fn main()i32{return (i32)R}
