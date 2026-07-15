//@ expect err recursive layout
struct A{A a i32 v} fn f(A a)i32{return a.v} fn main()i32{return 0}
