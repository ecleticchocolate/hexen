// codegen.h — the LayoutSink interface for the shared layout traversal.
// See codegen.c and docs/codegen_sink_design.md.
#pragma once
#include "compiler.h"
#include <stdbool.h>
#include <stdint.h>

// A sink receives the leaf actions of layout_fill. `ctx` is the consumer's state
// (a JITBuffer*+base for x86, an arena+base for constexpr). All offsets are
// relative to the CURRENT aggregate base, which the sink tracks via
// enter_sub/leave_sub — the traversal never inspects the base itself.
typedef struct LayoutSink {
    void* ctx;

    // Place a scalar: evaluate `value_node` and store its `width`-byte result at
    // `offset` (is_float selects float vs int store). The sink owns evaluation —
    // x86 emits code, constexpr folds.
    bool (*put_scalar)(void* ctx, uint64_t offset, int width, bool is_float, ASTNode* value_node);

    // Place a pre-folded default constant value at `offset`.
    bool (*put_default)(void* ctx, uint64_t offset, uint64_t size, uint8_t* bytes);

    // Place a nested aggregate VALUE that is not a literal (e.g. another const):
    // copy `size` bytes from the value of `value_node` to `offset`.
    bool (*put_agg_value)(void* ctx, uint64_t offset, uint64_t size, ASTNode* value_node);

    // Write the enum tag (u32) at offset 0 of the current aggregate.
    bool (*put_tag)(void* ctx, int tag_value);

    // Shift the current base down by `offset` (enter a nested aggregate), and
    // restore it (leave). Each sink implements its own base mechanism.
    bool (*enter_sub)(void* ctx, uint64_t offset);
    void (*leave_sub)(void* ctx);
} LayoutSink;

bool layout_fill(ASTNode* lit, const LayoutSink* sink);

// ─── Shared aggregate-binop traversal ──────────────────────────────────────
// "Which ops are valid on struct/array operands, and how do they decompose
// into scalar lanes" is ONE algorithm (mirrors layout_fill's split: the
// traversal owns WHICH ops are legal + lane enumeration + size/type matching;
// the sink owns HOW a single lane's op is computed and where the boolean/
// aggregate result goes). Before this, it lived once in backend_x64.c and was
// never shared -- constexpr had no aggregate-binop support at all beyond a
// hand-rolled EQ/NEQ special case. See docs/codegen_sink_design.md for the
// analogous layout_fill rationale; same trade, applied to binops instead of
// construction.
typedef struct AggBinopSink {
    void* ctx;

    // Perform `op` (AST_EQ/NEQ/ADD/SUB/MUL/BIT_AND/BIT_OR/BIT_XOR) on one
    // integer lane of `width` bytes at `offset` (relative to each operand's
    // own base -- the sink tracks two bases, left and right, however it likes).
    // For EQ/NEQ specifically the sink accumulates a running boolean instead of
    // writing a lane result; do_lane reports which mode via is_eq.
    bool (*do_lane)(void* ctx, ASTNodeType op, uint64_t offset, int width, bool is_eq);

    // EQ/NEQ only: after all lanes are ORed/ANDed by the sink internally,
    // finalize the boolean result (x86: sete/setne into a register; constexpr:
    // read the accumulated flag). Not called for arithmetic/bitwise ops --
    // those write directly to a destination the sink already knows about.
    bool (*finish_eq)(void* ctx, bool is_eq, int64_t* out_bool);
} AggBinopSink;

// Validates op is aggregate-legal and lt==rt, enumerates lanes via the shared
// Agg_Lanes walk, and drives `sink`. Returns false (no error text) if the op
// isn't defined on aggregates or the operand types don't match -- callers emit
// their own diagnostic (they already have better context: parse-time vs
// codegen-time error conventions differ, so this stays silent on failure,
// matching layout_fill's convention above).
bool agg_binop_apply(ASTNodeType op, Type* lt, Type* rt, const AggBinopSink* sink);

// Lane enumeration, promoted out of backend_x64.c (it was pure and
// backend-agnostic already -- just walks Type*, no JIT/arena state). Flattens
// an aggregate into (offset,width) integer lanes; -1 on float/pointer/enum
// lanes (unsupported for lanewise ops, same restriction as before the move).
int Agg_Lanes(Type* t, uint64_t base, uint64_t* offs, uint64_t* ws, int n, int cap);

