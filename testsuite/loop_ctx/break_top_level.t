//@ expect err outside of a loop
fn f()i32{ break  return 0 } fn main()i32{return f()}
