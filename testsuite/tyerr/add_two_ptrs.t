//@ expect err cannot assign
fn main()i32{i32 x=1 i32 y=2 i32* a=&x i32* b=&y i32* c=a+b return *c}
