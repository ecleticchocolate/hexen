//@ expect stdout
//@ | 3 3 3 3
// The correctness property enum pack-tail peeling depends on: a variant's
// ABSOLUTE tag (the number actually stored in memory) must survive being
// peeled off the front of the variant list, at any recursion depth -- a value
// of the concrete type reinterpreted as a progressively-peeled Rest type must
// always read back the SAME tag. A naive rebundle that renumbered tags from 0
// each time (the way a struct-tail rebundle already works, since struct field
// identity has no runtime-meaningful position) would read back 3, 2, 1, 0
// instead of 3 unchanged -- silently corrupting which variant a `match` on the
// peeled type thinks it's looking at.
extern fn printf(u8* fmt, ...) i32
enum Five { i32 V0  i32 V1  i32 V2  i32 V3  i32 V4 }

fn tag_after_n_peels[Walk, u32 N](Walk* p) u32 {
    match Walk {
        enum { H h  Rest... r } {
            if N == 0 { return *(u32*)p }
            return tag_after_n_peels[Rest, N - 1]((Rest*)p)
        }
        enum {} { return 999 }
    }
}

fn main() i32 {
    Five v = .V3{30}
    printf("%d %d %d %d\n",
        tag_after_n_peels[Five, 0](&v),
        tag_after_n_peels[Five, 1](&v),
        tag_after_n_peels[Five, 2](&v),
        tag_after_n_peels[Five, 3](&v))
    return 0
}
