//@ expect val 104
struct Inner{u32[2] data} struct Mid{Inner[2] items} struct Outer{Mid[2] groups} fn main()i32{Outer o o.groups[1].items[1].data[1]=99 o.groups[0].items[0].data[0]=5 return (i32)(o.groups[1].items[1].data[1]+o.groups[0].items[0].data[0])}
