//@ expect val 1210
fn add(i32 x)i32{return x+1}
fn sub(i32 x)i32{return x-1}
fn dbl(i32 x)i32{return x*2}
fn main()i32{
    (fn(i32)i32)[3] ops={add,sub,dbl}
    return ops[0](10)*100+ops[1](10)*10+ops[2](10)
}
