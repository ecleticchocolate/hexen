// ---------------------------------------------------------
// Str / String - borrowed view and owned buffer
// ---------------------------------------------------------
// Two types, and the split is load-bearing rather than stylistic:
//
//   Str    { u8* data  u32 size }          BORROWED. Has no __delete().
//   String { u8* data  u32 size  u32 capacity } OWNED.    Has __delete().
//
// A string literal is a `u8*` into static storage (specs.md §2), so a view
// over a literal owns nothing and must never be freed. A concatenation
// allocates and must be. Encoding that difference as a runtime `bool owned`
// flag would put the answer somewhere no generic code can see it; encoding it
// as *two types* puts it in the type, where the ownership convention every
// other container already uses -- `match T { impl { fn __delete() } }` --
// reads it for free:
//
//   Vector[Str]     -- element type has no __delete(). The loop is never emitted.
//   Vector[String]  -- element type has __delete(). Each element is freed.
//
// Neither Vector nor this file knows anything about the other. Nothing was
// added to make that work; it is the same structural question vector.t and
// linkedlist.t already ask, answered correctly because the answer is a fact
// about the type instead of a bit inside it.
//
// NOT provided, on purpose: `==`. Struct equality in Torrent is field-by-field
// by value (§5), so `a == b` on two Str compares the POINTER and the LENGTH --
// it is an identity test, not a content test, and it silently says `false` for
// two identical strings at different addresses. Content comparison is `eq()`,
// spelled out, because a wrong answer here is the kind that survives review.
//
// Every fallible read returns Option[T] and is opened by an ordinary match with
// a bound payload -- `match s.slice(2,5) { .Some(v) { ... } .None { ... } }`.
// The one friction is that `match` is a statement, not an expression (§24), so
// unwrapping into a variable costs a line and a temp:
//
//     Str mid = str_empty()
//     match s.slice(2, 5) { .Some(v) { mid = v }  .None { } }
//
// That is the only place this file wanted something the language does not have,
// and it is already a named gap -- not a missing protocol, just `match` needing
// to be an expression like `if` already is for scalars.

// ---------------------------------------------------------
// Str - a borrowed view: a pointer and a length, nothing else
// ---------------------------------------------------------
// Owns nothing. Copying one is free. Outliving the buffer it points into is
// the caller's problem -- the same contract a raw pointer already has, with a
// length attached so that every read can be bounded and no NUL scan is needed.
// Field defaults ARE the empty state: storage is not zeroed, so a bare
// `Str s` would otherwise hold a garbage pointer and a garbage length.
pub struct Str {
    u8* data = null
    u32 size = 0
}

// Length of a NUL-terminated C string, not counting the NUL. The one place in
// this file that trusts a terminator -- everything downstream carries a length.
pub fn cstr_len(u8* p) u32 {
    u32 n = 0
    while p[n] != 0 {
        n = n + 1
    }
    return n
}

// Borrows a NUL-terminated C string (a literal, or anything from libc). O(n)
// once, to find the length; every operation after this is O(1)-bounded.
pub fn str_from_cstr(u8* p) Str {
    return { .data = p, .size = cstr_len(p) }
}

// Borrows an explicit range of bytes. No scan, no NUL required -- this is how
// you view the middle of a buffer that has no terminator.
pub fn str_from_parts(u8* p, u32 n) Str {
    return { .data = p, .size = n }
}

// The empty view. `.data` is null, not a pointer to a NUL byte: there is no
// byte to point at, and `len == 0` already stops every loop in this file.
pub fn str_empty() Str {
    return { .data = null, .size = 0 }
}

pub impl Str {
    fn len() u32 {
        return self.size
    }

    fn is_empty() bool {
        return self.size == 0
    }

    // Byte at `i`, or None. The checked read -- the reason the length is
    // carried at all.
    fn at(u32 i) Option[u8] {
        if i >= self.size {
            return .None
        }
        return {.Some = self.data[i]}
    }

    // Byte at `i`, UNCHECKED. Caller guarantees i < len. Named to say so:
    // the checked form is `at()`, and a caller who wanted checking had it.
    fn at_unchecked(u32 i) u8 {
        return self.data[i]
    }

    // Content equality. NOT `==` -- see the note at the top of this file.
    // Per-Str step function baked into IteratorCursor's const-generic type.
    // Declared before begin(), same ordering rule every container follows.
    static fn step(struct{u8* data  u32 pos  u32 len}* s) Option[u8*] {
        if s.pos >= s.len { return .None }
        u8* p = &s.data[s.pos]
        s.pos = s.pos + 1
        return {.Some = p}
    }

    // Content equality. `==` on two Str would otherwise be Torrent's field-by-
    // field struct compare -- pointer AND length -- which is an identity test
    // that answers false for two identical strings at different addresses.
    // Defining __eq makes `a == b` mean what a reader expects.
    fn __eq(Str other) bool {
        if self.size != other.size { return false }
        for u32 i = 0 to self.size {
            if self.data[i] != other.data[i] { return false }
        }
        return true
    }

    // `!=` dispatches to __neq on its own -- it does NOT fall back to
    // !(a == b) -- so a type defining only __eq has no `!=` at all.
    fn __neq(Str other) bool {
        if self.size != other.size { return true }
        for u32 i = 0 to self.size {
            if self.data[i] != other.data[i] { return true }
        }
        return false
    }

    // Direct, unchecked byte access. Returns a pointer into the storage, so
    // `s[i]` reads and (on a String's own buffer) writes. at() is the checked
    // equivalent.
    fn __index(u32 i) u8* {
        return &self.data[i]
    }

    // Iteration over bytes: `for u8* b in s { ... }`.
    fn begin() IteratorCursor[u8, struct{u8* data  u32 pos  u32 len}, Str.step] {
        return {
            .state = {
                .data = self.data,
                .pos = 0,
                .len = self.size
            }
        }
    }

    fn eq(Str other) bool {
        if self.size != other.size {
            return false
        }
        for u32 i = 0 to self.size {
            if self.data[i] != other.data[i] {
                return false
            }
        }
        return true
    }

    // The sub-view [start, end). Returns None rather than clamping: a caller
    // who slices out of range has a bug, and clamping would hide it behind a
    // shorter-than-expected string. No copy -- a slice of a view is a view.
    fn slice(u32 start, u32 end) Option[Str] {
        if start > end {
            return .None
        }
        if end > self.size {
            return .None
        }
        return {.Some = str_from_parts(self.data + start, end - start)}
    }

    // The first `n` bytes, or the whole view if it is shorter. Unlike slice(),
    // these two clamp on purpose: "up to n" is the operation, not "exactly n".
    fn take(u32 n) Str {
        if n >= self.size {
            return str_from_parts(self.data, self.size)
        }
        return str_from_parts(self.data, n)
    }

    // Everything after the first `n` bytes. Empty if n >= len.
    fn drop(u32 n) Str {
        if n >= self.size {
            return str_empty()
        }
        return str_from_parts(self.data + n, self.size - n)
    }

    fn starts_with(Str prefix) bool {
        if prefix.size > self.size {
            return false
        }
        return self.take(prefix.size).eq(prefix)
    }

    fn ends_with(Str suffix) bool {
        if suffix.size > self.size {
            return false
        }
        return self.drop(self.size - suffix.size).eq(suffix)
    }

    // Index of the first `b`, or None.
    fn find_byte(u8 b) Option[u32] {
        for u32 i = 0 to self.size {
            if self.data[i] == b {
                return {.Some = i}
            }
        }
        return .None
    }

    // Index of the first occurrence of `needle`, or None. Naive O(n*m) --
    // deliberately: this file is a string type, not a search library, and a
    // real search (Boyer-Moore, or regex.t, which is already in std) belongs
    // where its preprocessing cost can be amortized across many haystacks.
    // An empty needle matches at 0, which is the convention that makes
    // `find(x) == Some{i}` imply `starts_with` at `i` for every needle.
    fn find(Str needle) Option[Str] {
        if needle.size > self.size {
            return .None
        }
        u32 last = self.size - needle.size
        u32 i = 0
        while i <= last {
            if self.drop(i).starts_with(needle) {
                return {.Some = str_from_parts(self.data + i, needle.size)}
            }
            i = i + 1
        }
        return .None
    }

    // The binding is omitted on purpose: `contains` asks only whether a match
    // exists, so there is nothing to bind. A caller who wants the match itself
    // calls find() and binds `.Some(hit)`.
    fn contains(Str needle) bool {
        match self.find(needle) {
            .Some { return true }
            .None { return false }
        }
    }

    fn count_byte(u8 b) u32 {
        u32 n = 0
        for u32 i = 0 to self.size {
            if self.data[i] == b {
                n = n + 1
            }
        }
        return n
    }

    // Views with leading / trailing ASCII whitespace removed. Still borrowed --
    // trimming allocates nothing, which is the whole argument for having a view
    // type at all. (Rebuilding a trimmed copy is `String` + `push_str`.)
    fn trim_start() Str {
        u32 i = 0
        while i < self.size {
            if !is_space(self.data[i]) {
                return self.drop(i)
            }
            i = i + 1
        }
        return str_empty()
    }

    fn trim_end() Str {
        u32 n = self.size
        while n > 0 {
            if !is_space(self.data[n - 1]) {
                return self.take(n)
            }
            n = n - 1
        }
        return str_empty()
    }

    fn trim() Str {
        return self.trim_start().trim_end()
    }

    // Parses a leading unsigned decimal integer. None on an empty view, a
    // non-digit first byte, or overflow past u32. Stops at the first non-digit
    // rather than rejecting the whole view -- so `"42abc".parse_u32()` is
    // Some{42}; combine with trim()/len checks if trailing junk should be an
    // error.
    fn parse_u32() Option[u32] {
        if self.size == 0 {
            return .None
        }
        if !is_digit(self.data[0]) {
            return .None
        }
        u32 acc = 0
        u32 i = 0
        while i < self.size {
            u8 c = self.data[i]
            if !is_digit(c) {
                return {.Some = acc}
            }
            u32 d = (u32) (c - '0')
            // Overflow check BEFORE the multiply, not after: unsigned wrap is
            // defined behavior in Torrent (§10), so a post-hoc "did it get
            // smaller" test would be checking a value that already wrapped.
            if acc > 429496729 {
                return .None
            }
            if acc == 429496729 {
                if d > 5 {
                    return .None
                }
            }
            acc = acc * 10 + d
            i = i + 1
        }
        return {.Some = acc}
    }
}

// ---------------------------------------------------------
// Byte classification - ASCII only, and honest about it
// ---------------------------------------------------------
// A Str is bytes, not codepoints. These answer questions about bytes. Anything
// ---------------------------------------------------------
// String - an owned, growable byte buffer
// ---------------------------------------------------------
// The only type in this file that allocates, and therefore the only one with a
// free(). Growth is amplitude-doubling, same as Vector[T] -- and for the same
// reason: it makes a sequence of n pushes O(n) rather than O(n^2).
//
// Not NUL-terminated. The length is in the struct; a terminator would be a
// second, redundant source of truth, and the two would eventually disagree.
// `to_cstr()` exists for the boundary where libc demands one, and it is
// explicit about the cost.
// String embeds Str via `super`, so every Str method -- len, at, slice, find,
// trim, starts_with, parse_u32, all of them -- works on a String directly. That
// is the whole reason for the embedding: without it String would need its own
// copy of twenty query methods, or every caller would write `s.str().trim()`.
//
// `capacity` is the only field String adds -- `data` and `size` are Str's,
// promoted by the embedding. Its default is the empty state (storage is not
// zeroed); the promoted fields get theirs from Str's own declaration.
pub struct String {
    super Str view
    u32 capacity = 0
}

pub impl String {
    // `String` converts to `Str` implicitly, at every assignability site: an
    // argument, a declaration, an assignment, a return, a struct field. So an
    // API takes the BORROWED type and callers hand it either:
    //
    //     fn parse(Str src) ...
    //     parse(some_str)       // a view
    //     parse(some_string)    // an owned buffer -- converts here, no .str()
    //
    // That direction is the free one (a view into storage that already exists);
    // Str -> String is not, and stays an explicit `string_from` because it
    // allocates. Making the cheap direction implicit and the expensive one
    // spelled out is the whole point.
    //
    // The generic form (`__cast[T]`) rather than a fixed `__cast() Str` so the
    // same hook serves any future view type without a second method.
    fn __cast[T]() T {
        return self.view
    }

    static fn with_capacity(u32 cap = 8) String {
        u32 c = cap
        if c == 0 { c = 8 }
        return {
            .view = {.data = new[c] u8, .size = 0},
            .capacity = c
        }
    }

    static fn create() String {
        return String.with_capacity()
    }

    // Copies a view into fresh storage -- the borrowed -> owned boundary. A
    // named call rather than an implicit conversion, so the allocation is
    // visible at the point it happens (the reverse direction, String -> Str,
    // IS implicit because it allocates nothing).
    static fn from(Str s) String {
        String out = String.with_capacity(s.size)
        out.push_str(s)
        return out
    }

    fn len() u32 {
        return self.size
    }

    fn cap() u32 {
        return self.capacity
    }

    fn is_empty() bool {
        return self.size == 0
    }

    // The whole point of the two-type split: every read-only Str method is
    // available on a String for free, with no duplication and no copy, by
    // handing out a view of the buffer. `s.str().find(needle)` etc.
    //
    // The view is invalidated by any subsequent push/reserve that reallocates.
    // That is the same rule Vector[T] has for a T* into its data, stated out
    // loud because a Str is easier to hold onto than a raw pointer is.
    fn str() Str {
        return str_from_parts(self.data, self.size)
    }

    // Grows to hold at least `want` bytes total. Doubling until it fits, so a
    // single large reserve() is one allocation rather than a doubling cascade.
    fn reserve(u32 want) {
        if want <= self.capacity {
            return
        }
        u32 c = self.capacity
        while c < want {
            c = c * 2
        }
        u8* fresh = new[c] u8
        for u32 i = 0 to self.size {
            fresh[i] = self.data[i]
        }
        delete self.data
        self.data = fresh
        self.capacity = c
    }

    fn push(u8 b) {
        self.reserve(self.size + 1)
        self.data[self.size] = b
        self.size = self.size + 1
    }

    // Appends a view. Safe even when `s` views into self.data (e.g.
    // `s.push_str(s.str())` to double a string): reserve() may reallocate and
    // leave `s.data` dangling, so the copy is driven off a view re-derived
    // AFTER the reserve, from an index recorded before it.
    fn push_str(Str s) {
        if s.size == 0 {
            return
        }
        // Record the aliasing case before we can possibly invalidate `s`.
        bool aliases = false
        u32 offset = 0
        if s.data >= self.data {
            if s.data < self.data + self.capacity {
                aliases = true
                offset = (u32) (s.data - self.data)
            }
        }
        u32 n = s.size
        self.reserve(self.size + n)
        u8* src = s.data
        if aliases {
            src = self.data + offset
        }
        for u32 i = 0 to n {
            self.data[self.size + i] = src[i]
        }
        self.size = self.size + n
    }

    fn push_cstr(u8* p) {
        self.push_str(str_from_cstr(p))
    }

    // Removes and returns the last byte, or None if empty. Mirrors
    // Vector[T]::pop(), including the Option return.
    fn pop() Option[u8] {
        if self.size == 0 {
            return .None
        }
        self.size = self.size - 1
        return {.Some = self.data[self.size]}
    }

    // Empties the string without releasing the buffer -- so a String reused in
    // a loop allocates once, not once per iteration.
    fn clear() {
        self.size = 0
    }

    fn map_upper() {
        for u32 i = 0 to self.size {
            self.data[i] = to_upper(self.data[i])
        }
    }

    fn map_lower() {
        for u32 i = 0 to self.size {
            self.data[i] = to_lower(self.data[i])
        }
    }

    // A freshly allocated, NUL-terminated copy for handing to libc. The caller
    // owns it and must `delete` it. A copy, not a view into self.data, because
    // appending a NUL in place would either overwrite a byte or silently grow
    // the buffer -- and because the result must outlive any later push().
    // Content equality on two Strings. `super` forwards ordinary METHOD calls,
    // but not operators -- `a == b` looks for String's own __eq and does not
    // fall back to the embedded Str's -- so it is declared here too, delegating
    // to the one real implementation rather than repeating the byte loop.
    fn __eq(Str other) bool {
        return self.view == other
    }

    fn __neq(Str other) bool {
        return self.view != other
    }

    // Deep copy: fresh storage, same bytes. The owned counterpart of handing
    // out a Str, which shares storage instead.
    fn copy() String {
        String out = String.with_capacity(self.size)
        out.push_str(self.view)
        return out
    }

    // Replaces the contents from a byte-array literal:  s = {'a', 'b', 'c'}
    fn __assign[u32 N](u8[N] bytes) void {
        self.clear()
        for u32 i = 0 to N {
            self.push(bytes[i])
        }
    }

    fn to_cstr() u8* {
        u8* out = new[self.size + 1] u8
        for u32 i = 0 to self.size {
            out[i] = self.data[i]
        }
        out[self.size] = 0
        return out
    }

    // Destructor: called automatically by `delete` on a String* (see
    // Vector[T]'s __delete() for the full convention). A String owns nothing
    // but its bytes -- a u8 has no __delete() -- so unlike Vector[T] there is
    // no element-ownership question to ask here, and no
    // `match T { impl { fn __delete() } }` to ask it with.
    //
    // The presence of this method is exactly what makes a Vector[String] free
    // its elements and a Vector[Str] not: vector.t asks whether the element
    // type HAS a __delete(), and String answers yes, Str answers no. Neither
    // file knows the other exists.
    fn __delete() {
        if self.data != null {
            delete self.data
            self.data = null
        }
        self.size = 0
        self.capacity = 0
    }
}
