//@ expect val 3
// `Rest...` pack-tail peeling on an ENUM type, recursively, counting variants --
// previously a hard parse-time error ("a `T...` pack-tail is not meaningful in
// an anonymous enum"). Fixed: an enum's Rest tail is a real, smaller enum (not
// a struct), so the same peel-and-recurse idiom struct/union fields already use
// works here too. Also exercises a real parser bug found while fixing this: a
// wildcard payload type immediately followed by `...` (`Rest...`) was silently
// misread as a BARE NO-PAYLOAD VARIANT NAMED "Rest" (the lookahead only checked
// for a following identifier, not a following `...`).
enum Shape { i32 Circle  i32[2] Rect  bool Flag }

fn count_variants[T](u32 acc) u32 {
    match T {
        enum { H h  Rest... r } { return count_variants[Rest](acc + 1) }
        enum {} { return acc }
    }
}

fn main() i32 { return (i32)count_variants[Shape](0) }
