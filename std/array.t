// Array[T, u32 N] - fixed-size, inline-storage array.
//
// The stack counterpart to Vector[T]: N is part of the TYPE, so the elements
// live inside the Array itself and there is no allocation, no capacity, and no
// growth. Array[i32, 4] is 16 bytes, exactly its elements -- sizeof proves it.
//
// DESTRUCTION IS NEVER CALLED BY HAND (same rule as Vector):
//
//   delete p      one heap object
//   delete[] p    a heap array (destroys every element, then frees)
//   scope exit    RAII, for any local
//
// __delete() is the *implementation* the language invokes, never something this
// file calls. Array declares none at all: its storage is inline, so scope exit
// destroys the whole value -- including any owning elements -- with nothing for
// this file to write.
//
// Element ownership follows Vector's one predicate, asked in one place:
// store_elem() is the only way a T gets into self.e, and it deep-copies when T
// owns a resource. There is no move_out() counterpart, because nothing is ever
// removed from a fixed-size array -- set() overwrites in place and the old value
// is destroyed by the same scope-exit RAII that destroys any other local.
//
// Contract for an owning T: define __delete(), and define copy() to deep-copy --
// identical to Vector's contract, enforced at the same single choke point.

pub struct Array[T, u32 N] {
    T[N] e
}

pub impl Array[T, N] {
    // Per-array step function baked into IteratorCursor's const-generic type.
    //
    // DECLARED BEFORE begin(), and that order is load-bearing: a forward
    // reference to a static from earlier in the same impl block currently
    // resolves to a null symbol ("LIT_FN_SYMBOL literal doesn't hold a valid
    // function symbol"). Vector[T].step sits before Vector[T].begin() for the
    // same reason.
    static fn step(struct{T* data  u32 pos  u32 len}* s) Option[T*] {
        if s.pos >= s.len { return .None }
        T* p = &s.data[s.pos]
        s.pos = s.pos + 1
        return .Some(p)
    }

    // Single source of truth for "how does a T get stored into this array":
    // deep-copy if T owns a resource, plain value otherwise. set() and fill()
    // both route through this rather than each carrying their own match T.
    fn store_elem(u32 index, T item) void {
        match T {
            Owning { self.e[index] = item.copy() }
            else   { self.e[index] = item }
        }
    }

    // Lets a flat brace literal (`Array[T, N] a = {1, 2, 3}`) assign directly,
    // the same way Vector's __assign does for its own flat literal -- Array's
    // N is already fixed by the struct's own const-generic param, so unlike
    // Vector's __assign[u32 N] this needs no separate generic parameter of
    // its own. Routes through store_elem() so an owning T still deep-copies
    // per element instead of aliasing the literal's storage.
    fn __assign(T[N] arr) void {
        for u32 i = 0 to N {
            T old = self.e[i]
            self.store_elem(i, arr[i])
        }
    }

    // Zero-initialized array. `Array[T, N] a` alone does the same thing; this
    // exists so the type can be constructed in expression position.
    static fn create() Array[T, N] {
        Array[T, N] a
        return a
    }

    // Every element set to a copy of `item`.
    static fn filled(T item) Array[T, N] {
        Array[T, N] a
        for u32 i = 0 to N {
            a.store_elem(i, item)
        }
        return a
    }

    // Direct, unchecked access. Returns a pointer into the storage, so
    // `a[i] = x` also works. See get() for the checked equivalent.
    fn __index(u32 index) T* {
        return &self.e[index]
    }

    // Retrieves an element by index safely.
    fn get(u32 index) Option[T] {
        if index >= N {
            return .None
        }
        return {.Some = self.e[index]}
    }

    // Sets an element by index safely. Returns true if successful.
    //
    // The displaced element is copied into `old` first -- an ordinary local,
    // destroyed by scope exit like any other -- before store_elem overwrites
    // the slot. Without this, an owning T already live in that slot is
    // silently replaced with no destructor call: not aliased, just leaked,
    // since the slot itself is the only reference and store_elem overwrites
    // it directly. Same idiom as Vector.set()'s `T old = self.move_out(index)`,
    // adapted for inline storage: there's no buffer slot to blank afterward,
    // just the old value to hand to RAII.
    fn set(u32 index, T item) bool {
        if index >= N {
            return false
        }
        T old = self.e[index]
        self.store_elem(index, item)
        return true
    }

    // Overwrites every element with a copy of `item`. Same old-value-to-RAII
    // idiom as set() -- each slot's previous element (which may already be
    // live on a re-fill) is captured into a scope-exit-destroyed local before
    // being replaced, not dropped in place.
    fn fill(T item) void {
        for u32 i = 0 to N {
            T old = self.e[i]
            self.store_elem(i, item)
        }
    }

    // Element-wise equality. Elements are compared with `*a == *b` rather than
    // `a == b` on values: the deref form works uniformly for primitives, plain
    // structs, AND an element type whose own __eq takes a pointer, which a value
    // comparison cannot reach from generic code.
    //
    // N is part of the type, so two Arrays can only be compared when their
    // lengths already match -- there is no size check to write.
    fn __eq(Array[T, N] other) bool {
        for u32 i = 0 to N {
            T* mine = &self.e[i]
            T* theirs = &other.e[i]
            if !(*mine == *theirs) {
                return false
            }
        }
        return true
    }

    // `!=` dispatches to __neq on its own -- it does NOT fall back to
    // !(a == b) -- so a type defining only __eq has no `!=` at all. Written as
    // its own loop rather than delegating, because re-entering `==` on two
    // Array values from inside a generic instantiation hits "aggregate operator
    // not defined reached backend".
    fn __neq(Array[T, N] other) bool {
        for u32 i = 0 to N {
            T* mine = &self.e[i]
            T* theirs = &other.e[i]
            if !(*mine == *theirs) {
                return true
            }
        }
        return false
    }

    // Deep copy. Routes through store_elem(), so an owning T gets its own
    // resource per element instead of aliasing the source's.
    fn copy() Array[T, N] {
        Array[T, N] out
        for u32 i = 0 to N {
            out.store_elem(i, self.e[i])
        }
        return out
    }

    // The element count. A compile-time constant -- N is part of the type --
    // so this folds away entirely at the call site.
    fn len() u32 {
        return N
    }

    // Always false for N > 0; kept so Array and Vector present the same surface.
    fn is_empty() bool {
        return N == 0
    }

    // Iterator support for `for TYPE val in arr` / `for unpack ...`. The
    // cursor's state and step function are both baked into its type, so there
    // is no array-specific cursor implementation here.
    fn begin() IteratorCursor[T, struct{T* data  u32 pos  u32 len}, Array[T, N].step] {
        return {
            .state = {
                .data = &self.e[0],
                .pos = 0,
                .len = N
            }
        }
    }
}