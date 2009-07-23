#include "bool.h"
#include "stdlib.h"
#include "rdcss.h"
#include "ghost_cells.h"
#include "ghost_lists.h"
#include "assoc_list.h"
#include "ghost_counters.h"
#include "bitops.h"
#include "mcas.h"

struct cd {
    void *status;  // enum { UNDECIDED = 0, SUCCESS = 1, FAILURE = 2 } // Should really have type uintptr_t.
    int n;
    struct mcas_entry *es;
    struct cas_tracker *tracker;
    //@ int counter;
    //@ int statusCell;
    //@ mcas_op *op;
    //@ bool done;
    //@ bool success2;
    //@ bool committed;
    //@ bool disposed;
};

/*@

lemma void entries_separate_ith(int i);
    requires [?f]entries(?n, ?aes, ?es) &*& 0 <= i &*& i < n;
    ensures
        [f]entries(i, aes, take(i, es)) &*&
        [f]mcas_entry_a(aes + i, fst(ith(i, es))) &*&
        [f]mcas_entry_o(aes + i, fst(snd(ith(i, es)))) &*&
        [f]mcas_entry_n(aes + i, snd(snd(ith(i, es)))) &*&
        true == (((uintptr_t)fst<void *, void *>(snd(ith(i, es))) & 1) == 0) &*&
        true == (((uintptr_t)fst<void *, void *>(snd(ith(i, es))) & 2) == 0) &*&
        true == (((uintptr_t)snd<void *, void *>(snd(ith(i, es))) & 1) == 0) &*&
        true == (((uintptr_t)snd<void *, void *>(snd(ith(i, es))) & 2) == 0) &*&
        [f]entries(n - i - 1, aes + i + 1, drop(i + 1, es));

lemma void entries_unseparate_ith(int i, list<pair<void *, pair<void *, void *> > > es);
    requires
        0 <= i &*& i < length(es) &*&
        [?f]entries(i, ?aes, take(i, es)) &*&
        [f]mcas_entry_a(aes + i, fst(ith(i, es))) &*&
        [f]mcas_entry_o(aes + i, fst(snd(ith(i, es)))) &*&
        [f]mcas_entry_n(aes + i, snd(snd(ith(i, es)))) &*&
        true == (((uintptr_t)fst<void *, void *>(snd(ith(i, es))) & 1) == 0) &*&
        true == (((uintptr_t)fst<void *, void *>(snd(ith(i, es))) & 2) == 0) &*&
        true == (((uintptr_t)snd<void *, void *>(snd(ith(i, es))) & 1) == 0) &*&
        true == (((uintptr_t)snd<void *, void *>(snd(ith(i, es))) & 2) == 0) &*&
        [f]entries(length(es) - i - 1, aes + i + 1, drop(i + 1, es));
    ensures [f]entries(length(es), aes, es);

lemma void entries_length_lemma()
    requires [?f]entries(?n, ?entries, ?es);
    ensures [f]entries(n, entries, es) &*& n == length(es);
{
    open entries(n, entries, es);
    switch (es) {
        case nil:
        case cons(e, es0):
            entries_length_lemma();
    }
    close [f]entries(n, entries, es);
}

predicate cd(struct cd *cd; list<pair<void *, pair<void *, void *> > > es, struct cas_tracker *tracker, int counter, int statusCell, mcas_op *op) =
    true == (((uintptr_t)cd & 1) == 0) &*&
    true == (((uintptr_t)cd & 2) == 0) &*&
    cd->n |-> ?count &*& cd->es |-> ?entries &*& entries(count, entries, es) &*& cd->tracker |-> tracker &*& cd->counter |-> counter &*& cd->statusCell |-> statusCell &*&
    cd->op |-> op;

predicate_ctor mcas_cell(int rcsList, int dsList)(void **a, void *vr, void *va) =
    [?f]strong_ghost_assoc_list_member_handle(rcsList, a, vr) &*&
    true == (((uintptr_t)vr & 2) == 0) ?
        f == 1 &*& va == vr
    :
        true == (((uintptr_t)vr & 2) == 2) &*&
        [_]ghost_list_member_handle(dsList, (void *)((uintptr_t)vr & ~2)) &*&
        [_]cd((void *)((uintptr_t)vr & ~2), ?es, ?tracker, ?counter, ?statusCell, ?op) &*&
        ghost_counter_snapshot(counter, index_of_assoc(a, es) + 1) &*&
        counted_ghost_cell_ticket<pair<int, void *> >(statusCell, ?status) &*&
        va == (fst(status) == 1 ? snd(assoc(a, es)) : fst(assoc(a, es))) &*&
        snd(status) == (void *)0 ? f == 1/2 : f == 1;

predicate_ctor entry_mem(int rcsList)(pair<void *, pair<void *, void *> > e) =
    [_]strong_ghost_assoc_list_key_handle(rcsList, fst(e)) &*&
    true == (((uintptr_t)fst<void *, void *>(snd(e)) & 2) == 0) &*&
    true == (((uintptr_t)snd<void *, void *>(snd(e)) & 2) == 0);

predicate_ctor entry_attached(int rcsList, void *cd)(pair<void *, pair<void *, void *> > e) =
    [1/2]strong_ghost_assoc_list_member_handle(rcsList, fst(e), (void *)((uintptr_t)cd | 2));

predicate_ctor cdext(int rcsList, mcas_unsep *unsep, any mcasInfo)(struct cd *cd, void **pstatus, void *status) =
    true == (((uintptr_t)cd & 1) == 0) &*&
    true == (((uintptr_t)cd & 2) == 0) &*&
    pstatus == &cd->status &*&
    [_]cd(cd, ?es, ?tracker, ?counter, ?statusCell, ?op) &*&
    distinct(mapfst(es)) == true &*&
    [?fDone]cd->done |-> ?done &*&
    [?fSuccess2]cd->success2 |-> ?success2 &*&
    [?fCommitted]cd->committed |-> (status != 0) &*&
    ghost_counter(counter, ?count) &*& 0 <= count &*& count <= length(es) &*&
    (
        status == 0 ?
            fCommitted == 1 &*&
            foreach(take(count, es), entry_attached(rcsList, cd)) &*&
            !success2
        :
            true
    ) &*&
    foreach(es, entry_mem(rcsList)) &*&
    counted_ghost_cell<pair<int, void *> >(statusCell, pair(success2 ? 1 : 0, status), count) &*&
    (
        status != 0 ?
            done &*& cas_tracker(tracker, 1) &*& status == (success2 ? (void *)1 : (void *)2)
        :
            cas_tracker(tracker, 0)
    ) &*&
    [1/2]cd->disposed |-> ?disposed &*&
    (disposed ? done : true) &*&
    done ?
        [_]tracked_cas_prediction(tracker, 0, success2 ? (void *)1 : (void *)2) &*&
        disposed ?
            true
        :
            is_mcas_op(op) &*& mcas_post(op)(success2)
    :
        fDone == 1 &*& fSuccess2 == 1 &*& !success2 &*&
        is_mcas_op(op) &*& mcas_pre(op)(unsep, mcasInfo, es);

predicate mcas(int id, mcas_sep *sep, mcas_unsep *unsep, any mcasInfo, list<pair<void *, void *> > cs) =
    [_]ghost_cell6(id, ?rdcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
    rdcss(rdcssId, mcas_rdcss_unsep, boxed_int(id), ?sas, ?rcs) &*&
    strong_ghost_assoc_list(rcsList, rcs) &*&
    ghost_list(dsList, ?ds) &*&
    foreach3(ds, sas, ?svs, cdext(rcsList, unsep, mcasInfo)) &*&
    foreach_assoc(zip(sas, svs), pointer) &*&
    foreach_assoc2(rcs, cs, mcas_cell(rcsList, dsList));

lemma void mem_es_lemma(int k, list<pair<void *, pair<void *, void *> > > es, list<pair<void *, void *> > cs);
    requires
        foreach_assoc2(?rcs, cs, ?p) &*& mem_es(es, cs) == true &*& 0 <= k &*& k < length(es);
    ensures 
        foreach_assoc2(rcs, cs, p) &*& mem_assoc(fst(ith(k, es)), rcs) == true;

fixpoint list<pair<a, b> > fold_remove_assoc<a, b>(list<a> xs, list<pair<a, b> > xys);

lemma void foreach_assoc2_subset_separate(int n);
    requires
        strong_ghost_assoc_list(?rcsList, ?rcs) &*&
        foreach_assoc2(rcs, ?cs, ?p) &*&
        foreach(?es, entry_mem(rcsList)) &*&
        0 <= n &*& n <= length(es) &*&
        distinct(mapfst(es)) == true;
    ensures
        mapfst(cs) == mapfst(rcs) &*&
        mem_es(es, cs) == true &*&
        strong_ghost_assoc_list(rcsList, rcs) &*&
        foreach_assoc2(fold_remove_assoc(take(n, mapfst(es)), rcs), fold_remove_assoc(take(n, mapfst(es)), cs), p) &*&
        foreach_assoc2(
            drop(0, take(n, map_assoc(rcs, mapfst(es)))),
            drop(0, take(n, map_assoc(cs, mapfst(es)))),
            p) &*&
        foreach(es, entry_mem(rcsList));

lemma void foreach_assoc2_subset_unseparate(list<pair<void *, void *> > cs, int n);
    requires
        strong_ghost_assoc_list(?rcsList, ?rcs) &*&
        foreach(?es, entry_mem(rcsList)) &*&
        0 <= n &*& n <= length(es) &*&
        distinct(mapfst(es)) == true &*&
        mapfst(cs) == mapfst(rcs) &*&
        foreach_assoc2(fold_remove_assoc(take(n, mapfst(es)), rcs), fold_remove_assoc(take(n, mapfst(es)), cs), ?p) &*&
        foreach_assoc2(
            drop(0, take(n, map_assoc(rcs, mapfst(es)))),
            drop(0, take(n, map_assoc(cs, mapfst(es)))),
            p) &*&
        distinct(mapfst(es)) == true;
    ensures
        strong_ghost_assoc_list(rcsList, rcs) &*&
        foreach_assoc2(rcs, cs, p) &*&
        foreach(es, entry_mem(rcsList));

lemma void ith_neq_es_success(int i, list<pair<void *, pair<void *, void *> > > es, list<pair<void *, void *> > cs);
    requires 0 <= i &*& i < length(es) &*& assoc(fst(ith(i, es)), cs) != fst(snd(ith(i, es)));
    ensures !es_success(es, cs);

lemma void es_apply_lemma(int k, list<pair<void *, pair<void *, void *> > > es, list<pair<void *, void *> > cs);
    requires distinct(mapfst(es)) == true &*& 0 <= k &*& k < length(es) &*& mem_es(es, cs) == true;
    ensures
        ith(k, map_assoc(es_apply(es, cs), mapfst(es))) == pair(fst(ith(k, es)), snd(snd(ith(k, es)))) &*&
        assoc(fst(ith(k, es)), es_apply(es, cs)) == snd(snd(ith(k, es)));

lemma void es_apply_lemma2(list<pair<void *, pair<void *, void *> > > es, list<pair<void *, void *> > cs);
    requires true;
    ensures
        mapfst(es_apply(es, cs)) == mapfst(cs) &*&
        fold_remove_assoc(mapfst(es), es_apply(es, cs)) == fold_remove_assoc(mapfst(es), cs);

predicate_family_instance rdcss_separate_lemma(mcas_rdcss_sep)(boxed_int info, int rdcssId, predicate() inv, rdcss_unseparate_lemma *rdcssUnsep) =
    rdcssUnsep == mcas_rdcss_unsep &*& is_mcas_unsep(?unsep) &*& is_mcas_sep(?sep) &*&
    mcas_sep(sep)(?mcasInfo, unboxed_int(info), inv, unsep) &*& [_]ghost_cell6(unboxed_int(info), rdcssId, _, _, sep, unsep, mcasInfo);

predicate_family_instance rdcss_unseparate_lemma(mcas_rdcss_unsep)
    (boxed_int info, int rdcssId, predicate() inv, rdcss_separate_lemma *rdcssSep, list<void *> aas, list<void *> avs, list<pair<void *, void *> > bs) =
    rdcssSep == mcas_rdcss_sep &*&
    is_mcas_unsep(?unsep) &*& is_mcas_sep(?sep) &*& mcas_unsep(unsep)(?mcasInfo, unboxed_int(info), inv, sep, ?cs) &*&
    [_]ghost_cell6(unboxed_int(info), rdcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
    strong_ghost_assoc_list(rcsList, bs) &*&
    ghost_list(dsList, ?ds) &*&
    foreach3(ds, aas, avs, cdext(rcsList, unsep, mcasInfo)) &*& distinct(aas) == true &*&
    foreach_assoc2(bs, cs, mcas_cell(rcsList, dsList));

lemma void mcas_rdcss_sep() : rdcss_separate_lemma
    requires
        rdcss_separate_lemma(mcas_rdcss_sep)(?info, ?rdcssId, ?inv, ?rdcssUnsep) &*& inv();
    ensures
        rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId, inv, mcas_rdcss_sep, ?aas, ?avs, ?bs) &*&
        foreach_assoc(zip(aas, avs), pointer) &*& rdcss(rdcssId, rdcssUnsep, info, aas, bs);
{
    open rdcss_separate_lemma(mcas_rdcss_sep)(?info_, _, _, _);
    int id = 0;
    switch (info_) {
        case boxed_int(id_): id = id_;
    }
    assert is_mcas_sep(?sep);
    assert is_mcas_unsep(?unsep);
    sep();
    open mcas(id, sep, unsep, _, ?cs);
    merge_fractions ghost_cell6(id, _, _, ?dsList, _, _, _);
    assert rdcss(rdcssId, _, _, ?aas, ?bs);
    assert foreach3(?ds, aas, ?avs, _);
    foreach3_length();
    foreach_assoc_zip_pointer_distinct(aas, avs);
    close rdcss_unseparate_lemma(mcas_rdcss_unsep)(info_, rdcssId, inv, mcas_rdcss_sep, aas, avs, bs);
}

lemma void mcas_rdcss_unsep() : rdcss_unseparate_lemma
    requires
        rdcss_unseparate_lemma(mcas_rdcss_unsep)(?info, ?rdcssId, ?inv, ?rdcssSep, ?aas, ?avs, ?bs) &*&
        foreach_assoc(zip(aas, avs), pointer) &*& rdcss(rdcssId, mcas_rdcss_unsep, info, aas, bs);
    ensures
        rdcss_separate_lemma(rdcssSep)(info, rdcssId, inv, mcas_rdcss_unsep) &*& inv();
{
    open rdcss_unseparate_lemma(mcas_rdcss_unsep)(?info_, _, _, _, _, _, _);
    int id = 0;
    switch (info_) { case boxed_int(id_): id = id_; }
    assert foreach_assoc2(_, ?cs, _);
    assert is_mcas_sep(?sep);
    assert is_mcas_unsep(?unsep);
    assert mcas_unsep(unsep)(?mcasInfo, _, _, _, _);
    split_fraction ghost_cell6(id, _, _, _, _, _, _);
    close mcas(id, sep, unsep, mcasInfo, cs);
    unsep();
    close rdcss_separate_lemma(mcas_rdcss_sep)(info_, rdcssId, inv, mcas_rdcss_unsep);
}

lemma int create_mcas(mcas_sep *sep, mcas_unsep *unsep, any mcasInfo)
    requires true;
    ensures mcas(result, sep, unsep, mcasInfo, nil);
{
    int id = create_ghost_cell6(0, 0, 0, 0, 0, mcasInfo);
    int rdcssId = create_rdcss(mcas_rdcss_unsep, boxed_int(id));
    int rcsList = create_strong_ghost_assoc_list();
    int dsList = create_ghost_list();
    close foreach3(nil, nil, nil, cdext(rcsList, unsep, mcasInfo));
    close foreach_assoc(nil, pointer);
    close foreach_assoc2(nil, nil, mcas_cell(rcsList, dsList));
    ghost_cell6_update(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo);
    close mcas(id, sep, unsep, mcasInfo, nil);
    return id;
}

lemma void mcas_add_cell(int id, void *a)
    requires mcas(id, ?sep, ?unsep, ?mcasInfo, ?cs) &*& !mem_assoc(a, cs) &*& pointer(a, ?v) &*& true == (((uintptr_t)v & 1) == 0) &*& true == (((uintptr_t)v & 2) == 0);
    ensures mcas(id, sep, unsep, mcasInfo, cons(pair(a, v), cs));
{
    open mcas(_, _, _, _, _);
    assert [_]ghost_cell6(id, ?rcdssId, ?rcsList, ?dsList, _, _, _);
    assert foreach_assoc2(?rcs, cs, _);
    if (mem_assoc(a, rcs)) {
        foreach_assoc2_separate(a);
    }
    strong_ghost_assoc_list_add(rcsList, a, v);
    close mcas_cell(rcsList, dsList)(a, v, v);
    close foreach_assoc2(cons(pair(a, v), rcs), cons(pair(a, v), cs), mcas_cell(rcsList, dsList));
    rdcss_add_b(a);
    close mcas(id, sep, unsep, mcasInfo, cons(pair(a, v), cs));
}

@*/

void *mcas_read(void *a)
    /*@
    requires
        is_mcas_sep(?sep) &*& is_mcas_unsep(?unsep) &*& mcas_sep(sep)(?mcasInfo, ?id, ?inv, unsep) &*&
        [?f]atomic_space(inv) &*&
        is_mcas_cs_mem(?csMem) &*& mcas_cs_mem(csMem)(unsep, mcasInfo, a) &*&
        is_mcas_read_op(?rop) &*& mcas_read_pre(rop)(unsep, mcasInfo, a);
    @*/
    /*@
    ensures
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        [f]atomic_space(inv) &*&
        is_mcas_cs_mem(csMem) &*& mcas_cs_mem(csMem)(unsep, mcasInfo, a) &*&
        is_mcas_read_op(rop) &*& mcas_read_post(rop)(result);
    @*/
{
    {
        /*@
        predicate_family_instance atomic_noop_context_pre(noop)(predicate() inv_) =
            inv_ == inv &*&
            is_mcas_cs_mem(csMem) &*& mcas_cs_mem(csMem)(unsep, mcasInfo, a) &*&
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep);
        predicate_family_instance atomic_noop_context_post(noop)() =
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            is_mcas_cs_mem(csMem) &*& mcas_cs_mem(csMem)(unsep, mcasInfo, a) &*&
             [_]ghost_cell6(id, ?rdcssId, ?rcsList, _, sep, unsep, mcasInfo) &*&
             [_]strong_ghost_assoc_list_key_handle(rcsList, a);
        lemma void noop() : atomic_noop_context
            requires atomic_noop_context_pre(noop)(?inv_) &*& inv_();
            ensures atomic_noop_context_post(noop)() &*& inv_();
        {
            open atomic_noop_context_pre(noop)(_);
            sep();
            csMem();
            open mcas(id, sep, unsep, mcasInfo, ?cs);
            split_fraction ghost_cell6(id, _, _, _, _, _, _);
            assert foreach_assoc2(?rcs, cs, _);
            foreach_assoc2_separate(a);
            foreach_assoc2_unseparate_nochange(rcs, cs, a);
            create_strong_ghost_assoc_list_key_handle(a);
            close mcas(id, sep, unsep, mcasInfo, cs);
            unsep();
            close atomic_noop_context_post(noop)();
        }
        @*/
        //@ close atomic_noop_context_pre(noop)(inv);
        //@ produce_lemma_function_pointer_chunk(noop);
        atomic_noop();
        //@ leak is_atomic_noop_context(noop);
        //@ open atomic_noop_context_post(noop)();
    }
start:
    /*@
    invariant
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        [f]atomic_space(inv) &*&
        is_mcas_cs_mem(csMem) &*& mcas_cs_mem(csMem)(unsep, mcasInfo, a) &*&
        [_]ghost_cell6(id, ?rdcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
        [_]strong_ghost_assoc_list_key_handle(rcsList, a) &*&
        is_mcas_read_op(rop) &*& mcas_read_pre(rop)(unsep, mcasInfo, a);
    @*/
    {
        /*@
        predicate_family_instance rdcss_bs_membership_lemma(bsMem)(rdcss_unseparate_lemma *rdcssUnsep, boxed_int info, void *a2) =
            rdcssUnsep == mcas_rdcss_unsep &*& info == boxed_int(id) &*& a2 == a &*&
            [_]strong_ghost_assoc_list_key_handle(rcsList, a) &*&
            [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo);
        lemma void bsMem() : rdcss_bs_membership_lemma
            requires
                rdcss_bs_membership_lemma(bsMem)(?rdcssUnsep, ?info, ?a2) &*&
                rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
            ensures
                rdcss_bs_membership_lemma(bsMem)(rdcssUnsep, info, a2) &*&
                rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs) &*&
                mem_assoc(a2, bs) == true;
        {
            open rdcss_bs_membership_lemma(bsMem)(_, _, _);
            open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
            merge_fractions ghost_cell6(id, _, _, _, _, _, _);
            split_fraction ghost_cell6(id, _, _, _, _, _, _);
            strong_ghost_assoc_list_key_handle_lemma();
            close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId, inv_, rdcssSep, aas, avs, bs);
            close rdcss_bs_membership_lemma(bsMem)(rdcssUnsep, boxed_int(id), a2);
        }
        predicate_family_instance rdcss_read_operation_pre(rdcssRop)(rdcss_unseparate_lemma *rdcssUnsep, boxed_int info, void *a2) =
            rdcssUnsep == mcas_rdcss_unsep &*& info == boxed_int(id) &*& a2 == a &*&
            [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
            [_]strong_ghost_assoc_list_key_handle(rcsList, a) &*&
            is_mcas_read_op(rop) &*& mcas_read_pre(rop)(unsep, mcasInfo, a);
        predicate_family_instance rdcss_read_operation_post(rdcssRop)(void *result) =
            [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
            [_]strong_ghost_assoc_list_key_handle(rcsList, a) &*&
            true == (((uintptr_t)result & 2) == 0) ?
                is_mcas_read_op(rop) &*& mcas_read_post(rop)(result)
            :
                is_mcas_read_op(rop) &*& mcas_read_pre(rop)(unsep, mcasInfo, a) &*&
                [_]ghost_list_member_handle(dsList, (void *)((uintptr_t)result & ~2)) &*&
                [_]cd((void *)((uintptr_t)result & ~2), _, _, _, _, _);
        lemma void *rdcssRop() : rdcss_read_operation_lemma
            requires
                rdcss_read_operation_pre(rdcssRop)(?rdcssUnsep, ?info, ?a2) &*&
                rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
            ensures
                result == assoc(a2, bs) &*& mem_assoc(a2, bs) == true &*&
                rdcss_read_operation_post(rdcssRop)(result) &*& rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs);
        {
            open rdcss_read_operation_pre(rdcssRop)(_, _, _);
            open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
            merge_fractions ghost_cell6(id, _, _, _, _, _, _);
            split_fraction ghost_cell6(id, _, _, _, _, _, _);
            strong_ghost_assoc_list_key_handle_lemma();
            assert foreach_assoc2(?rcs, ?cs, _);
            foreach_assoc2_separate(a);
            open mcas_cell(rcsList, dsList)(a, ?realValue, ?abstractValue);
            if (((uintptr_t)realValue & 2) == 0) {
                rop();
            } else {
                split_fraction ghost_list_member_handle(dsList, _);
                split_fraction cd(_, _, _, _, _, _);
            }
            close mcas_cell(rcsList, dsList)(a, realValue, abstractValue);
            foreach_assoc2_unseparate_nochange(rcs, cs, a);
            close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId, inv_, mcas_rdcss_sep, aas, avs, bs);
            close rdcss_read_operation_post(rdcssRop)(realValue);
            return realValue;
        }
        @*/
        //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
        //@ split_fraction strong_ghost_assoc_list_key_handle(rcsList, a);
        //@ close rdcss_bs_membership_lemma(bsMem)(mcas_rdcss_unsep, boxed_int(id), a);
        //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
        //@ close rdcss_read_operation_pre(rdcssRop)(mcas_rdcss_unsep, boxed_int(id), a);
        //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
        //@ close rdcss_separate_lemma(mcas_rdcss_sep)(boxed_int(id), rdcssId, inv, mcas_rdcss_unsep);
        //@ produce_lemma_function_pointer_chunk(bsMem);
        //@ produce_lemma_function_pointer_chunk(rdcssRop);
        //@ produce_lemma_function_pointer_chunk(mcas_rdcss_sep);
        //@ produce_lemma_function_pointer_chunk(mcas_rdcss_unsep);
        void *r = rdcss_read(a);
        //@ leak is_rdcss_separate_lemma(_);
        //@ leak is_rdcss_unseparate_lemma(_);
        //@ leak is_rdcss_read_operation_lemma(rdcssRop);
        //@ leak is_rdcss_bs_membership_lemma(bsMem);
        //@ open rdcss_separate_lemma(mcas_rdcss_sep)(_, _, _, _);
        //@ merge_fractions ghost_cell6(id, _, _, _, _, _, _);
        //@ open rdcss_read_operation_post(rdcssRop)(_);
        //@ merge_fractions ghost_cell6(id, _, _, _, _, _, _);
        //@ leak rdcss_bs_membership_lemma(_)(_, _, _);
        if (((uintptr_t)r & 2) != 0) {
            mcas_impl((struct cd *)((uintptr_t)r & ~2));
            //@ leak [_]ghost_list_member_handle(_, _);
            //@ leak [_]cd(_, _, _, _, _, _);
            //@ leak [_]cd_committed(_, _);
            //@ leak [_]cd_success2(_, _);
            goto start;
        }
        //@ leak [_]ghost_cell6(id, _, _, _, _, _, _);
        //@ leak [_]strong_ghost_assoc_list_key_handle(_, _);
        return r;
    }
}

bool mcas(int n, struct mcas_entry *aes)
    /*@
    requires
        [?f]atomic_space(?inv) &*&
        entries(n, aes, ?es) &*&
        distinct(mapfst(es)) == true &*&
        is_mcas_sep(?sep) &*& is_mcas_unsep(?unsep) &*& mcas_sep(sep)(?mcasInfo, ?id, inv, unsep) &*&
        is_mcas_mem(?mem) &*& mcas_mem(mem)(unsep, mcasInfo, es) &*&
        is_mcas_op(?op) &*& mcas_pre(op)(unsep, mcasInfo, es);
    @*/
    /*@
    ensures
        [f]atomic_space(inv) &*&
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        is_mcas_mem(mem) &*& mcas_mem(mem)(unsep, mcasInfo, es) &*&
        is_mcas_op(op) &*& mcas_post(op)(result);
    @*/
{
    struct cas_tracker *tracker = create_cas_tracker();
    struct cd *cd = malloc(sizeof(struct cd));
    if (cd == 0) abort();
    //@ assume(((uintptr_t)cd & 1) == 0);
    //@ assume(((uintptr_t)cd & 2) == 0);
    cd->status = 0;
    cd->n = n;
    cd->es = aes;
    cd->tracker = tracker;
    //@ int counter = create_ghost_counter(0);
    //@ cd->counter = counter;
    //@ int statusCell = create_counted_ghost_cell(pair(0, (void *)0));
    //@ cd->statusCell = statusCell;
    //@ cd->op = op;
    //@ cd->done = false;
    //@ cd->success2 = false;
    //@ cd->committed = false;
    //@ cd->disposed = false;
    //@ close cd(cd, es, tracker, counter, statusCell, op);
    
    // Install the descriptor in the atomic space.
    {
        /*@
        predicate_family_instance atomic_noop_context_pre(context)(predicate() inv_) =
            inv_ == inv &*&
            cd(cd, es, tracker, counter, statusCell, op) &*&
            cas_tracker(tracker, 0) &*&
            ghost_counter(counter, 0) &*&
            counted_ghost_cell(statusCell, pair(0, (void *)0), 0) &*&
            cd->status |-> 0 &*&
            cd->done |-> false &*&
            cd->success2 |-> false &*&
            cd->committed |-> false &*&
            cd->disposed |-> false &*&
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            is_mcas_mem(mem) &*& mcas_mem(mem)(unsep, mcasInfo, es) &*&
            is_mcas_op(op) &*& mcas_pre(op)(unsep, mcasInfo, es);
        predicate_family_instance atomic_noop_context_post(context)() =
            [_]cd(cd, es, tracker, counter, statusCell, op) &*&
            [_]ghost_cell6(id, ?rdcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
            [_]ghost_list_member_handle(dsList, cd) &*&
            [1/2]cd->disposed |-> false &*&
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            is_mcas_mem(mem) &*& mcas_mem(mem)(unsep, mcasInfo, es);
        lemma void context() : atomic_noop_context
            requires atomic_noop_context_pre(context)(?inv_) &*& inv_();
            ensures atomic_noop_context_post(context)() &*& inv_();
        {
            open atomic_noop_context_pre(context)(_);
            sep();
            mem();
            open mcas(id, sep, unsep, mcasInfo, ?cs);
            assert [_]ghost_cell6(id, ?rcssId, ?rcsList, ?dsList, _, _, _);
            assert foreach3(?ds, ?sas, ?svs, _);
            rdcss_add_a(&cd->status);
            open cd_status(cd, _);
            close foreach_assoc(cons(pair(&cd->status, (void *)0), zip(sas, svs)), pointer);
            ghost_list_add(dsList, cd);
            length_nonnegative(es);
            take_0(es);
            close foreach(take(0, es), entry_attached(rcsList, cd));
            assert foreach_assoc2(?rcs, cs, _);
            {
                lemma void close_foreach_entry_mem(list<pair<void *, pair<void *, void *> > > es1, int k)
                    requires
                        es1 == drop(k, es) &*&
                        foreach_assoc2(rcs, cs, mcas_cell(rcsList, dsList)) &*&
                        0 <= k &*& k <= length(es) &*&
                        [?fEntries]entries(?n_, ?aes_, es) &*&
                        strong_ghost_assoc_list(rcsList, rcs);
                    ensures
                        strong_ghost_assoc_list(rcsList, rcs) &*&
                        foreach_assoc2(rcs, cs, mcas_cell(rcsList, dsList)) &*&
                        [fEntries]entries(n_, aes_, es) &*&
                        foreach(drop(k, es), entry_mem(rcsList));
                {
                    switch (es1) {
                        case nil:
                            close foreach(nil, entry_mem(rcsList));
                        case cons(e, es10):
                            drop_k_plus_one_alt(k, es);
                            close_foreach_entry_mem(es10, k + 1);
                            mem_es_lemma(k, es, cs);
                            create_strong_ghost_assoc_list_key_handle(fst(ith(k, es)));
                            entries_length_lemma();
                            entries_separate_ith(k);
                            entries_unseparate_ith(k, es);
                            close entry_mem(rcsList)(e);
                            close foreach(drop(k, es), entry_mem(rcsList));
                    }
                }
                drop_0(es);
                open [?fcd]cd(cd, _, _, _, _, _);
                close_foreach_entry_mem(es, 0);
                close [fcd]cd(cd, es, tracker, counter, statusCell, op);
            }
            split_fraction ghost_cell6(id, _, _, _, _, _, _);
            split_fraction cd(cd, _, _, _, _, _);
            split_fraction cd_disposed(cd, false);
            close cdext(rcsList, unsep, mcasInfo)(cd, &cd->status, 0);
            close foreach3(cons(cd, ds), cons(&cd->status, sas), cons((void *)0, svs), cdext(rcsList, unsep, mcasInfo));
            close mcas(id, sep, unsep, mcasInfo, cs);
            unsep();
            close atomic_noop_context_post(context)();
        }
        @*/
        //@ close atomic_noop_context_pre(context)(inv);
        //@ produce_lemma_function_pointer_chunk(context);
        atomic_noop();
        //@ leak is_atomic_noop_context(context);
        //@ open atomic_noop_context_post(context)();
    }
    
    bool success = mcas_impl(cd);
    
    // Extract the postcondition from the atomic space.
    {
        /*@
        predicate_family_instance atomic_noop_context_pre(context)(predicate() inv_) =
            inv_ == inv &*&
            [_]ghost_cell6(id, ?rcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
            [_]ghost_list_member_handle(dsList, cd) &*&
            [_]cd(cd, es, tracker, counter, statusCell, op) &*&
            [_]cd->committed |-> true &*&
            [_]cd->success2 |-> success &*&
            [1/2]cd->disposed |-> false &*&
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep);
        predicate_family_instance atomic_noop_context_post(context)() =
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            is_mcas_op(op) &*& mcas_post(op)(success);
        lemma void context() : atomic_noop_context
            requires atomic_noop_context_pre(context)(?inv_) &*& inv_();
            ensures atomic_noop_context_post(context)() &*& inv_();
        {
            open atomic_noop_context_pre(context)(_);
            sep();
            open mcas(id, _, _, _, ?cs);
            merge_fractions ghost_cell6(id, ?rdcssId, ?rcsList, ?dsList, _, _, _);
            ghost_list_member_handle_lemma();
            leak [_]ghost_list_member_handle(dsList, cd);
            assert foreach3(?ds, ?sas, ?svs, _);
            foreach3_separate(cd);
            open cdext(rcsList, unsep, mcasInfo)(cd, _, ?status_);
            merge_fractions cd(cd, _, _, _, _, _);
            merge_fractions cd_committed(cd, _);
            merge_fractions cd_success2(cd, _);
            merge_fractions cd_disposed(cd, _);
            cd->disposed = true;
            split_fraction cd_disposed(cd, _);
            leak [1/2]cd_disposed(cd, _);
            close cdext(rcsList, unsep, mcasInfo)(cd, &cd->status, status_);
            foreach3_unseparate_nochange(ds, sas, svs, cd);
            close mcas(id, sep, unsep, mcasInfo, cs);
            unsep();
            close atomic_noop_context_post(context)();
        }
        @*/
        //@ close atomic_noop_context_pre(context)(inv);
        //@ produce_lemma_function_pointer_chunk(context);
        atomic_noop();
        //@ leak is_atomic_noop_context(context);
        //@ open atomic_noop_context_post(context)();
    }
    
    //@ leak malloc_block_cd(cd); // We're assuming the presence of a garbage collector.
    
    return success;
}

//@ predicate done_copy(bool done) = true;
//@ predicate committed_copy(bool committed) = true;

bool mcas_impl(struct cd *cd)
    /*@
    requires
        [?f]atomic_space(?inv) &*&
        is_mcas_sep(?sep) &*& is_mcas_unsep(?unsep) &*& mcas_sep(sep)(?mcasInfo, ?id, inv, unsep) &*&
        [_]ghost_cell6(id, ?rdcssId, ?rcsList, ?dsList, sep, unsep, mcasInfo) &*&
        [_]ghost_list_member_handle(dsList, cd) &*&
        [_]cd(cd, ?es, ?tracker, ?counter, ?statusCell, ?op);
    @*/
    /*@
    ensures
        [f]atomic_space(inv) &*&
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
        [_]ghost_list_member_handle(dsList, cd) &*&
        [_]cd(cd, es, tracker, counter, statusCell, op) &*&
        [_]cd->committed |-> true &*& [_]cd->success2 |-> result;
    @*/
{
    bool success = false;
start:
    /*@
    invariant
        [f]atomic_space(inv) &*&
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
        [_]ghost_list_member_handle(dsList, cd) &*&
        [_]cd(cd, es, tracker, counter, statusCell, op);
    @*/
    //@ length_nonnegative(es);
    void *status = 0;
    //@ void *statusProphecy = create_prophecy_pointer();
    //@ split_fraction ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo);
    //@ split_fraction cd(cd, es, tracker, counter, statusCell, op);
    {
        /*@
        predicate_family_instance atomic_load_pointer_context_pre(context)(predicate() inv_, void *pp, void *prophecy) =
            inv_ == inv &*& pp == &cd->status &*& prophecy == statusProphecy &*&
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
            [_]ghost_list_member_handle(dsList, cd) &*&
            [_]cd(cd, es, tracker, counter, statusCell, op);
        predicate_family_instance atomic_load_pointer_context_post(context)() =
            is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
            [_]ghost_list_member_handle(dsList, cd) &*&
            statusProphecy == 0 ?
                ghost_counter_snapshot(counter, 0) &*& committed_copy(false)
            :
                [_]cd->committed |-> true &*& [_]cd->success2 |-> (statusProphecy == (void *)1);
        lemma void context(atomic_load_pointer_operation *aop) : atomic_load_pointer_context
            requires
                atomic_load_pointer_context_pre(context)(?inv_, ?pp, ?prophecy) &*& inv_() &*&
                is_atomic_load_pointer_operation(aop) &*& atomic_load_pointer_operation_pre(aop)(pp, prophecy);
            ensures
                atomic_load_pointer_context_post(context)() &*& inv_() &*&
                is_atomic_load_pointer_operation(aop) &*& atomic_load_pointer_operation_post(aop)();
        {
            open atomic_load_pointer_context_pre(context)(_, _, _);
            sep();
            open mcas(id, sep, unsep, mcasInfo, ?cs);
            merge_fractions ghost_cell6(id, _, _, _, _, _, _);
            ghost_list_member_handle_lemma();
            assert foreach3(?ds, ?sas, ?svs, _);
            foreach3_foreach_assoc_separate(cd);
            open cdext(rcsList, unsep, mcasInfo)(cd, _, _);
            aop();
            merge_fractions cd(cd, _, _, _, _, _);
            if (statusProphecy == 0) {
                create_ghost_counter_snapshot(0);
                close committed_copy(false);
            } else {
                split_fraction cd_committed(cd, true);
                split_fraction cd_success2(cd, _);
            }
            close cdext(rcsList, unsep, mcasInfo)(cd, &cd->status, statusProphecy);
            foreach3_foreach_assoc_unseparate_nochange(ds, sas, svs, cd);
            close mcas(id, sep, unsep, mcasInfo, cs);
            unsep();
            close atomic_load_pointer_context_post(context)();
        }
        @*/
        //@ close atomic_load_pointer_context_pre(context)(inv, &cd->status, statusProphecy);
        //@ produce_lemma_function_pointer_chunk(context);
        status = atomic_load_pointer(&cd->status);
        //@ leak is_atomic_load_pointer_context(context);
        //@ open atomic_load_pointer_context_post(context)();
    }
    //@ split_fraction cd(cd, es, tracker, counter, statusCell, op);
    //@ open cd(cd, es, tracker, counter, statusCell, op);
    if (status == 0) {
        uintptr_t s = 1;
        int i = 0;
        while (i < cd->n)
            /*@
            invariant
                [f]atomic_space(inv) &*&
                is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
                [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                [_]ghost_list_member_handle(dsList, cd) &*&
                [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                [?fcd]cd->n |-> ?n &*& [fcd]cd->es |-> ?entries &*& [fcd]entries(n, entries, es) &*&
                [fcd]cd->tracker |-> tracker &*& [fcd]cd->counter |-> counter &*& [fcd]cd->statusCell |-> statusCell &*& [fcd]cd->op |-> op &*&
                s == 1 &*&
                0 <= i &*& i <= length(es) &*&
                committed_copy(?committedCopy) &*&
                committedCopy ?
                    [_]cd->committed |-> true
                :
                    ghost_counter_snapshot(counter, i);
            @*/
        {
            //@ entries_length_lemma();
            void *r = 0;
            {
                /*@
                predicate_family_instance rdcss_as_membership_lemma(asMem)(rdcss_unseparate_lemma *unsep_, boxed_int info, void *a) =
                    unsep_ == mcas_rdcss_unsep &*& info == boxed_int(id) &*&
                    a == &cd->status &*& [_]ghost_list_member_handle(dsList, cd) &*& [_]ghost_cell6(unboxed_int(info), rdcssId, rcsList, dsList, sep, unsep, mcasInfo);
                lemma void asMem() : rdcss_as_membership_lemma
                    requires
                        rdcss_as_membership_lemma(asMem)(?rdcssUnsep, ?info, ?a) &*& rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
                    ensures
                        rdcss_as_membership_lemma(asMem)(rdcssUnsep, info, a) &*& rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs) &*&
                        mem((void *)a, aas) == true;
                {
                    open rdcss_as_membership_lemma(asMem)(_, _, _);
                    open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
                    merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                    ghost_list_member_handle_lemma();
                    assert foreach3(?ds, ?sas, ?svs, _);
                    foreach3_separate(cd);
                    assert is_mcas_unsep(?unsep_);
                    open cdext(rcsList, unsep_, mcasInfo)(cd, _, ?status_);
                    close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                    foreach3_unseparate_nochange(ds, sas, svs, cd);
                    foreach3_mem_x_mem_assoc_x_ys(cd);
                    split_fraction ghost_cell6(id, _, _, _, _, _, _);
                    close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, rdcssSep, aas, avs, bs);
                    close rdcss_as_membership_lemma(asMem)(rdcssUnsep, boxed_int(id), a);
                }
                predicate_family_instance rdcss_bs_membership_lemma(bsMem)(rdcss_unseparate_lemma *unsep_, boxed_int info, void *a) =
                    unsep_ == mcas_rdcss_unsep &*& info == boxed_int(id) &*&
                    a == fst(ith(i, es)) &*& 0 <= i &*& i < length(es) &*&
                    [_]ghost_list_member_handle(dsList, cd) &*& [_]ghost_cell6(unboxed_int(info), rdcssId, rcsList, dsList, _, _, mcasInfo) &*&
                    [_]cd(cd, es, tracker, counter, statusCell, op);
                lemma void bsMem() : rdcss_bs_membership_lemma
                    requires
                        rdcss_bs_membership_lemma(bsMem)(?rdcssUnsep, ?info, ?a) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
                    ensures
                        rdcss_bs_membership_lemma(bsMem)(rdcssUnsep, info, a) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs) &*&
                        mem_assoc((void *)a, bs) == true;
                {
                    open rdcss_bs_membership_lemma(bsMem)(_, _, _);
                    open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
                    merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                    ghost_list_member_handle_lemma();
                    assert foreach3(?ds, ?sas, ?svs, _);
                    foreach3_separate(cd);
                    assert is_mcas_unsep(?unsep_);
                    open cdext(rcsList, unsep_, mcasInfo)(cd, _, ?status_);
                    merge_fractions cd(cd, _, _, _, _, _);
                    split_fraction cd(cd, _, _, _, _, _);
                    foreach_separate_ith(i, es);
                    open entry_mem(rcsList)(ith(i, es));
                    strong_ghost_assoc_list_key_handle_lemma();
                    close entry_mem(rcsList)(ith(i, es));
                    foreach_unseparate_ith_nochange(i, es);
                    close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                    foreach3_unseparate_nochange(ds, sas, svs, cd);
                    split_fraction ghost_cell6(id, _, _, _, _, _, _);
                    close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, rdcssSep, aas, avs, bs);
                    close rdcss_bs_membership_lemma(bsMem)(mcas_rdcss_unsep, boxed_int(id), a);
                }
                predicate_family_instance rdcss_operation_pre(rop)
                    (rdcss_unseparate_lemma *rdcssUnsep, boxed_int info, void *a1, void *o1, void *a2, void *o2, void *n2) =
                    rdcssUnsep == mcas_rdcss_unsep &*& info == boxed_int(id) &*&
                    a1 == &cd->status &*& o1 == 0 &*& a2 == fst(ith(i, es)) &*& o2 == fst(snd(ith(i, es))) &*&
                    n2 == (void *)((uintptr_t)cd | 2) &*&
                    [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                    [_]ghost_list_member_handle(dsList, cd) &*&
                    [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                    committed_copy(?committedCopy1) &*&
                    committedCopy1 ?
                        [_]cd->committed |-> true
                    :
                        ghost_counter_snapshot(counter, i);
                predicate_family_instance rdcss_operation_post(rop)(void *result) =
                    [_]ghost_list_member_handle(dsList, cd) &*&
                    true == (((uintptr_t)result & 2) == 2) ?
                        true == (((uintptr_t)result & ~2) != (uintptr_t)cd) ?
                            [_]ghost_list_member_handle(dsList, (void *)((uintptr_t)result & ~2)) &*&
                            [_]cd((void *)((uintptr_t)result & ~2), _, _, _, _, _)
                        :
                            committed_copy(?committedCopy1) &*&
                            committedCopy1 ?
                                [_]cd->committed |-> true
                            :
                                ghost_counter_snapshot(counter, i + 1)
                    :
                        result != fst(snd(ith(i, es))) ?
                            [_]tracked_cas_prediction(tracker, 0, ?prediction) &*&
                            true == ((uintptr_t)prediction == 2) ?
                                [_]cd->done |-> true &*& [_]cd->success2 |-> false
                            :
                                true
                        :
                            committed_copy(?committedCopy1) &*&
                            committedCopy1 ?
                                [_]cd->committed |-> true
                            :
                                ghost_counter_snapshot(counter, i + 1);
                lemma void *rop() : rdcss_operation_lemma
                    requires
                        rdcss_operation_pre(rop)(?rdcssUnsep, ?info, ?a1, ?o1, ?a2, ?o2, ?n2) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
                    ensures
                        rdcss_operation_post(rop)(result) &*&
                        mem((void *)a1, aas) == true &*& mem_assoc(a2, bs) == true &*& result == assoc(a2, bs) &*&
                        assoc(a1, zip(aas, avs)) == o1 && assoc(a2, bs) == o2 ?
                            rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, update(bs, a2, n2))
                        :
                            rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs);
                {
                    open rdcss_operation_pre(rop)(_, _, _, _, _, _, _);
                    open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
                    open committed_copy(?committedCopy1);
                    void *result = assoc(a2, bs);
                    merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                    ghost_list_member_handle_lemma();
                    assert foreach3(?ds, ?sas, ?svs, _);
                    foreach3_length();
                    distinct_assoc_yzs(ds, sas, svs, cd);
                    foreach3_separate(cd);
                    assert is_mcas_unsep(?unsep_);
                    open cdext(rcsList, unsep_, mcasInfo)(cd, _, ?status_);
                    merge_fractions cd(cd, _, _, _, _, _);
                    assoc_fst_ith_snd_ith(es, i);
                    if (committedCopy1) {
                        merge_fractions cd_committed(cd, _);
                    } else {
                        match_ghost_counter_snapshot();
                        leak ghost_counter_snapshot(counter, _);
                    }
                    foreach_separate_ith(i, es);
                    open entry_mem(rcsList)(ith(i, es));
                    strong_ghost_assoc_list_key_handle_lemma();
                    close entry_mem(rcsList)(ith(i, es));
                    foreach_unseparate_ith_nochange(i, es);
                    assert foreach_assoc2(?rcs, ?cs, _);
                    foreach_assoc2_separate(a2);
                    open mcas_cell(rcsList, dsList)(a2, ?realCellValue, ?abstractCellValue);
                    if (((uintptr_t)result & 2) == 2) {
                        if (((uintptr_t)result & ~2) != (uintptr_t)cd) {
                            void *cd1 = (void *)((uintptr_t)result & ~2);
                            split_fraction ghost_list_member_handle(dsList, cd1);
                            split_fraction cd(cd1, _, _, _, _, _);
                        } else {
                            merge_fractions cd(cd, _, _, _, _, _);
                            if (committedCopy1) {
                                close committed_copy(true);
                                split_fraction cd_committed(cd, _);
                            } else {
                                close committed_copy(false);
                                match_ghost_counter_snapshot();
                                index_of_assoc_fst_ith(es, i);
                                create_ghost_counter_snapshot(i + 1);
                            }
                            split_fraction cd(cd, _, _, _, _, _);
                        }
                        close mcas_cell(rcsList, dsList)(a2, realCellValue, abstractCellValue);
                        foreach_assoc2_unseparate_nochange(rcs, cs, a2);
                        close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                        foreach3_unseparate_nochange(ds, sas, svs, cd);
                        foreach3_mem_x_mem_assoc_x_ys(cd);
                        close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, mcas_rdcss_sep, aas, avs, bs);
                    } else {
                        assert realCellValue == abstractCellValue;
                        if (result != fst(snd(ith(i, es)))) {
                            ith_neq_es_success(i, es, cs);
                            void *prediction = create_tracked_cas_prediction(tracker, 0);
                            if (prediction == (void *)2) {
                                assert [_]cd->done |-> ?done;
                                if (done) {
                                    merge_fractions tracked_cas_prediction(tracker, 0, _);
                                    split_fraction tracked_cas_prediction(tracker, 0, _);
                                    split_fraction cd_done(cd, _);
                                    split_fraction cd_success2(cd, _);
                                } else {
                                    op();
                                    cd->done = true;
                                    split_fraction tracked_cas_prediction(tracker, 0, _);
                                    split_fraction cd_done(cd, true);
                                    split_fraction cd_success2(cd, false);
                                }
                            } else {
                            }
                            close mcas_cell(rcsList, dsList)(a2, realCellValue, abstractCellValue);
                            foreach_assoc2_unseparate_nochange(rcs, cs, a2);
                            close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                            foreach3_unseparate_nochange(ds, sas, svs, cd);
                            foreach3_mem_x_mem_assoc_x_ys(cd);
                            close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, mcas_rdcss_sep, aas, avs, bs);
                        } else {
                            if (status_ == 0) {
                                assert ghost_counter(counter, ?count);
                                assert count <= length(es);
                                if (!committedCopy1) {
                                    if (count == i) {
                                        ghost_counter_add(1);
                                    }
                                }
                                // Attach the RDCSS cell to the descriptor.
                                bitand_bitor_lemma((uintptr_t)cd, 2);
                                assert counted_ghost_cell(statusCell, ?status2, _);
                                if (count > i) {
                                    length_take(count, es);
                                    foreach_separate_ith(i, take(count, es));
                                    ith_take(i, count, es);
                                    open entry_attached(rcsList, cd)(_);
                                    merge_fractions strong_ghost_assoc_list_member_handle(rcsList, a2, _);
                                }
                                strong_ghost_assoc_list_update(rcsList, a2, n2);
                                split_fraction strong_ghost_assoc_list_member_handle(rcsList, a2, _);
                                split_fraction ghost_list_member_handle(dsList, cd);
                                split_fraction cd(cd, _, _, _, _, _);
                                close committed_copy(false);
                                create_ghost_counter_snapshot(i + 1);
                                create_counted_ghost_cell_ticket(statusCell);
                                index_of_assoc_fst_ith(es, i);
                                close mcas_cell(rcsList, dsList)(a2, n2, abstractCellValue);
                                foreach_assoc2_unseparate_1changed(rcs, cs, a2);
                                create_ghost_counter_snapshot(i + 1);
                                close entry_attached(rcsList, cd)(ith(i, es));
                                foreach_take_plus_one_unseparate(i, es);
                                close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                                foreach3_unseparate_nochange(ds, sas, svs, cd);
                                foreach3_mem_x_mem_assoc_x_ys(cd);
                                close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, mcas_rdcss_sep, aas, avs, update(bs, a2, n2));
                            } else {
                                close committed_copy(true);
                                split_fraction cd_committed(cd, _);
                                close mcas_cell(rcsList, dsList)(a2, realCellValue, abstractCellValue);
                                foreach_assoc2_unseparate_nochange(rcs, cs, a2);
                                close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                                foreach3_unseparate_nochange(ds, sas, svs, cd);
                                foreach3_mem_x_mem_assoc_x_ys(cd);
                                close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, mcas_rdcss_sep, aas, avs, bs);
                            }
                        }
                    }
                    close rdcss_operation_post(rop)(result);
                    return result;
                }
                @*/
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ split_fraction ghost_list_member_handle(dsList, cd);
                //@ split_fraction cd(cd, _, _, _, _, _);
                /*@
                close rdcss_operation_pre(rop)
                    (mcas_rdcss_unsep, boxed_int(id), &cd->status, 0, fst(ith(i, es)), fst(snd(ith(i, es))), (void *)((uintptr_t)cd | 2));
                @*/
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ split_fraction ghost_list_member_handle(dsList, cd);
                //@ split_fraction cd(cd, _, _, _, _, _);
                //@ close rdcss_separate_lemma(mcas_rdcss_sep)(boxed_int(id), rdcssId, inv, mcas_rdcss_unsep);
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ split_fraction ghost_list_member_handle(dsList, cd);
                //@ close rdcss_as_membership_lemma(asMem)(mcas_rdcss_unsep, boxed_int(id), &cd->status);
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ close rdcss_bs_membership_lemma(bsMem)(mcas_rdcss_unsep, boxed_int(id), fst(ith(i, es)));
                //@ produce_lemma_function_pointer_chunk(mcas_rdcss_sep);
                //@ produce_lemma_function_pointer_chunk(mcas_rdcss_unsep);
                //@ produce_lemma_function_pointer_chunk(asMem);
                //@ produce_lemma_function_pointer_chunk(bsMem);
                //@ produce_lemma_function_pointer_chunk(rop);
                //@ entries_separate_ith(i);
                //@ bitand_bitor_1_2_lemma(cd);
                r = rdcss(&cd->status, 0, (cd->es + i)->a, (cd->es + i)->o, (void *)((uintptr_t)cd | 2));
                //@ leak is_rdcss_operation_lemma(_);
                //@ leak is_rdcss_as_membership_lemma(_);
                //@ leak is_rdcss_bs_membership_lemma(_);
                //@ leak is_rdcss_unseparate_lemma(_);
                //@ leak is_rdcss_separate_lemma(_);
                //@ leak rdcss_as_membership_lemma(_)(_, _, _);
                //@ leak rdcss_bs_membership_lemma(_)(_, _, _);
                //@ open rdcss_separate_lemma(mcas_rdcss_sep)(_, _, _, _);
                //@ open rdcss_operation_post(rop)(_);
                //@ merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                //@ leak [_]ghost_list_member_handle(dsList, cd);
            }
            if (((uintptr_t)r & 2) == 2) {
                struct cd *cd1 = (void *)((uintptr_t)r & ~2);
                if (cd1 != cd) {
                    mcas_impl(cd1);
                    //@ leak [_]cd(cd1, _, _, _, _, _);
                    //@ leak [_]cd(cd, _, _, _, _, _);
                    //@ leak [_]cd1->committed |-> _;
                    //@ leak [_]cd1->success2 |-> _;
                    //@ leak [_]ghost_list_member_handle(dsList, cd1);
                    //@ entries_unseparate_ith(i, es);
                    //@ close [fcd]cd(cd, es, tracker, counter, statusCell, op);
                    goto start;
                }
            } else if (r != (cd->es + i)->o) {
                s = 2;
                //@ entries_unseparate_ith(i, es);
                goto done;
            }
            //@ entries_unseparate_ith(i, es);
            i++;
        }
    done:
        //@ entries_length_lemma();
        //@ void *casProphecy = create_prophecy_pointer();
        {
            /*@
            predicate_family_instance tracked_cas_ctxt_pre(context)
                (struct cas_tracker *tracker_, predicate() inv_, void **pp, void *old, void *new, void *prophecy) =
                tracker_ == tracker &*& inv_ == inv &*& pp == &cd->status &*& old == 0 &*& new == (void *)s &*& prophecy == casProphecy &*&
                is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
                [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                [_]ghost_list_member_handle<void *>(dsList, cd) &*&
                [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                s == 1 ?
                    committed_copy(?committedCopy1) &*&
                    committedCopy1 ?
                        [_]cd->committed |-> true
                    :
                        ghost_counter_snapshot(counter, length(es))
                :
                    [_]tracked_cas_prediction(tracker, 0, ?prediction) &*&
                    prediction == (void *)2 ?
                        [_]cd->done |-> true &*& [_]cd->success2 |-> false
                    :
                        true;
            predicate_family_instance tracked_cas_ctxt_post(context)() =
                is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
                [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                [_]ghost_list_member_handle(dsList, cd) &*&
                [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                [_]cd->committed |-> true &*& [_]cd->success2 |-> ((casProphecy == 0 ? s : (uintptr_t)casProphecy) == 1);
            lemma void context(tracked_cas_operation *aop) : tracked_cas_ctxt
                requires
                    tracked_cas_ctxt_pre(context)(?tracker_, ?inv_, ?pp, ?old, ?new, ?prophecy) &*& inv_() &*&
                    is_tracked_cas_operation(aop) &*&
                    tracked_cas_pre(aop)(tracker_, pp, old, new, prophecy);
                ensures
                    tracked_cas_ctxt_post(context)() &*& inv_() &*&
                    is_tracked_cas_operation(aop) &*&
                    tracked_cas_post(aop)();
            {
                open tracked_cas_ctxt_pre(context)(_, _, _, _, _, _);
                sep();
                open mcas(_, _, _, _, ?cs);
                merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                ghost_list_member_handle_lemma<void *>();
                assert foreach3(?ds, ?sas, ?svs, _);
                foreach3_foreach_assoc_separate(cd);
                open cdext(rcsList, unsep, mcasInfo)(cd, _, ?status_);
                merge_fractions cd(cd, _, _, _, _, _);
                if (s == 1) {
                    open committed_copy(?committedCopy1);
                    if (committedCopy1) {
                        merge_fractions cd_committed(cd, _);
                        aop(0, 0);
                        split_fraction cd_committed(cd, _);
                        split_fraction cd_success2(cd, _);
                    } else {
                        if (casProphecy == 0) {
                            void *prediction = create_tracked_cas_prediction(tracker, 0);
                            assert [_]cd->done |-> ?done;
                            if (done) {
                                merge_fractions tracked_cas_prediction(tracker, 0, _);
                            }
                            split_fraction tracked_cas_prediction(tracker, 0, _);
                            aop(0, prediction);
                            cd->committed = true;
                            if (!done) {
                                op();
                                cd->done = true;
                                cd->success2 = true;
                                assert foreach_assoc2(?rcs, _, _);
                                assert i == length(es);
                                foreach_assoc2_subset_separate(i);
                                {
                                    lemma void update_status_cell(int k)
                                        requires
                                            foreach_assoc2(
                                                drop(k, take(i, map_assoc(rcs, mapfst(es)))),
                                                drop(k, take(i, map_assoc(cs, mapfst(es)))),
                                                mcas_cell(rcsList, dsList)) &*&
                                            [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                                            0 <= k &*& k <= length(es) &*&
                                            foreach(drop(k, take(i, es)), entry_attached(rcsList, cd)) &*&
                                            counted_ghost_cell(statusCell, pair(0, (void *)0), i - k);
                                        ensures
                                            foreach_assoc2(
                                                drop(k, take(i, map_assoc(rcs, mapfst(es)))),
                                                drop(k, take(i, map_assoc(es_apply(es, cs), mapfst(es)))),
                                                mcas_cell(rcsList, dsList)) &*&
                                            [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                                            es_success(drop(k, es), cs) == true &*&
                                            counted_ghost_cell(statusCell, pair(1, (void *)1), i - k);
                                    {
                                        open foreach_assoc2(_, _, _);
                                        if (k == i) {
                                            drop_length(es);
                                            drop_n_take_n(k, map_assoc(rcs, mapfst(es)));
                                            drop_n_take_n(k, map_assoc(cs, mapfst(es)));
                                            drop_n_take_n(k, map_assoc(es_apply(es, cs), mapfst(es)));
                                            counted_ghost_cell_update(statusCell, pair(1, (void *)1));
                                            leak foreach(_, _);
                                        } else {
                                            lt_drop_take(k, i, es);
                                            lt_drop_take_map_assoc_mapfst(k, i, rcs, es);
                                            lt_drop_take_map_assoc_mapfst(k, i, cs, es);
                                            lt_drop_take_map_assoc_mapfst(k, i, es_apply(es, cs), es);
                                            open foreach(_, _);
                                            open mcas_cell(rcsList, dsList)(_, _, _);
                                            open entry_attached(rcsList, cd)(_);
                                            merge_fractions strong_ghost_assoc_list_member_handle(rcsList, fst(ith(k, es)), _);
                                            bitand_bitor_lemma((uintptr_t)cd, 2);
                                            merge_fractions cd(cd, _, _, _, _, _);
                                            split_fraction cd(cd, _, _, _, _, _);
                                            counted_ghost_cell_dispose_ticket(statusCell);
                                            update_status_cell(k + 1);
                                            create_counted_ghost_cell_ticket(statusCell);
                                            assoc_fst_ith_snd_ith(es, k);
                                            close mcas_cell(rcsList, dsList)(fst(ith(k, es)), (void *)((uintptr_t)cd | 2), snd(snd(ith(k, es))));
                                            es_apply_lemma(k, es, cs);
                                            drop_k_plus_one(k, es);
                                        }
                                        close foreach_assoc2(
                                            drop(k, take(i, map_assoc(rcs, mapfst(es)))),
                                            drop(k, take(i, map_assoc(es_apply(es, cs), mapfst(es)))),
                                            mcas_cell(rcsList, dsList));
                                    }
                                    take_length(es);
                                    drop_0(take(i, es));
                                    match_ghost_counter_snapshot();
                                    leak [_]ghost_counter_snapshot(counter, _);
                                    update_status_cell(0);
                                }
                            }
                            es_apply_lemma2(es, cs);
                            length_mapfst(es);
                            take_length(mapfst(es));
                            foreach_assoc2_subset_unseparate(es_apply(es, cs), i);
                            cs = es_apply(es, cs);
                            split_fraction cd_committed(cd, _);
                            split_fraction cd_success2(cd, _);
                        } else {
                            aop(0, 0);
                            leak ghost_counter_snapshot(counter, _);
                            split_fraction cd_committed(cd, _);
                            split_fraction cd_success2(cd, _);
                        }
                    }
                } else {
                    if (casProphecy == 0) {
                        assert [_]tracked_cas_prediction(tracker, 0, ?prediction);
                        if (status_ != 0)
                            aop(0, prediction);
                        else
                            aop(0, prediction);
                        merge_fractions cd_done(cd, _);
                        merge_fractions cd_success2(cd, _);
                        cd->committed = true;
                        assert counted_ghost_cell(statusCell, _, ?count);
                        assert foreach_assoc2(?rcs, cs, _);
                        {
                            lemma void detach(int k)
                                requires
                                    foreach(drop(k, take(count, es)), entry_attached(rcsList, cd)) &*&
                                    0 <= k &*& k <= count &*&
                                    [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                                    foreach_assoc2(
                                        drop(k, take(count, map_assoc(rcs, mapfst(es)))),
                                        drop(k, take(count, map_assoc(cs, mapfst(es)))),
                                        mcas_cell(rcsList, dsList)) &*&
                                    counted_ghost_cell(statusCell, pair(0, (void *)0), count - k);
                                ensures
                                    [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                                    foreach_assoc2(
                                        drop(k, take(count, map_assoc(rcs, mapfst(es)))),
                                        drop(k, take(count, map_assoc(cs, mapfst(es)))),
                                        mcas_cell(rcsList, dsList)) &*&
                                    counted_ghost_cell(statusCell, pair(0, (void *)2), count - k);
                            {
                                open foreach(_, _);
                                if (k == count) {
                                    drop_n_take_n(count, es);
                                    counted_ghost_cell_update(statusCell, pair(0, (void *)2));
                                } else {
                                    lt_drop_take(k, count, es);
                                    lt_drop_take_map_assoc_mapfst(k, count, rcs, es);
                                    lt_drop_take_map_assoc_mapfst(k, count, cs, es);
                                    open foreach_assoc2(_, _, _);
                                    open entry_attached(rcsList, cd)(ith(k, es));
                                    open mcas_cell(rcsList, dsList)(fst(ith(k, es)), ?realCellValue, ?abstractCellValue);
                                    merge_fractions strong_ghost_assoc_list_member_handle(rcsList, fst(ith(k, es)), _);
                                    bitand_bitor_lemma((uintptr_t)cd, 2);
                                    merge_fractions cd(cd, _, _, _, _, _);
                                    split_fraction cd(cd, _, _, _, _, _);
                                    counted_ghost_cell_dispose_ticket(statusCell);
                                    detach(k + 1);
                                    create_counted_ghost_cell_ticket(statusCell);
                                    close mcas_cell(rcsList, dsList)(fst(ith(k, es)), realCellValue, abstractCellValue);
                                    close foreach_assoc2(
                                        drop(k, take(count, map_assoc(rcs, mapfst(es)))),
                                        drop(k, take(count, map_assoc(cs, mapfst(es)))),
                                        mcas_cell(rcsList, dsList));
                                }
                            }
                            drop_0(take(count, es));
                            foreach_assoc2_subset_separate(count);
                            detach(0);
                            foreach_assoc2_subset_unseparate(cs, count);
                        }
                        split_fraction cd_committed(cd, _);
                        split_fraction cd_success2(cd, _);
                    } else {
                        if (status_ != 0) {
                            leak [_]tracked_cas_prediction(tracker, 0, ?prediction);
                            if (prediction == (void *)2) {
                                merge_fractions cd_done(cd, _);
                                merge_fractions cd_success2(cd, _);
                            }
                            aop(0, 0);
                            split_fraction cd_committed(cd, _);
                            split_fraction cd_success2(cd, _);
                        } else {
                            aop(0, 0);
                        }
                    }
                }
                assert pointer(&cd->status, ?newStatus);
                split_fraction cd(cd, _, _, _, _, _);
                close cdext(rcsList, unsep, mcasInfo)(cd, &cd->status, newStatus);
                foreach3_foreach_assoc_unseparate(ds, sas, svs, cd);
                split_fraction ghost_cell6(id, _, _, _, _, _, _);
                close mcas(id, sep, unsep, mcasInfo, cs);
                unsep();
                close tracked_cas_ctxt_post(context)();
            }
            @*/
            //@ close tracked_cas_ctxt_pre(context)(tracker, inv, &cd->status, 0, (void *)s, casProphecy);
            //@ produce_lemma_function_pointer_chunk(context);
            status = tracked_cas(cd->tracker, &cd->status, 0, (void *)s);
            //@ leak is_tracked_cas_ctxt(context);
            //@ open tracked_cas_ctxt_post(context)();
        }
        if (status == 0) status = (void *)s;
    }
    success = (uintptr_t)status == 1;
    /*@
    invariant
        [f]atomic_space(inv) &*&
        is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
        [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
        [_]ghost_list_member_handle(dsList, cd) &*&
        [_]cd(cd, es, tracker, counter, statusCell, op) &*&
        [?fcd]cd->n |-> ?n &*& [fcd]cd->es |-> ?entries &*& [fcd]entries(n, entries, es) &*&
        [fcd]cd->tracker |-> tracker &*& [fcd]cd->counter |-> counter &*& [fcd]cd->statusCell |-> statusCell &*& [fcd]cd->op |-> op &*&
        true == (((uintptr_t)cd & 1) == 0) &*&
        true == (((uintptr_t)cd & 2) == 0) &*&
        [_]cd->committed |-> true &*& [_]cd->success2 |-> success;
    @*/
    {
        int i = 0;
        while (i < cd->n)
            /*@
            invariant
                0 <= i &*&
                [f]atomic_space(inv) &*&
                is_mcas_sep(sep) &*& is_mcas_unsep(unsep) &*& mcas_sep(sep)(mcasInfo, id, inv, unsep) &*&
                [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                [_]ghost_list_member_handle(dsList, cd) &*&
                [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                [fcd]cd->n |-> n &*& [fcd]cd->es |-> entries &*& [fcd]entries(n, entries, es) &*&
                [fcd]cd->tracker |-> tracker &*& [fcd]cd->counter |-> counter &*& [fcd]cd->statusCell |-> statusCell &*& [fcd]cd->op |-> op &*&
                [_]cd->committed |-> true &*& [_]cd->success2 |-> success;
            @*/
        {
            //@ entries_length_lemma();
            {
                /*@
                predicate_family_instance rdcss_bs_membership_lemma(bsMem)(rdcss_unseparate_lemma *unsep_, boxed_int info, void *a) =
                    unsep_ == mcas_rdcss_unsep &*& info == boxed_int(id) &*&
                    a == fst(ith(i, es)) &*& 0 <= i &*& i < length(es) &*&
                    [_]ghost_list_member_handle(dsList, cd) &*& [_]ghost_cell6(unboxed_int(info), rdcssId, rcsList, dsList, _, _, mcasInfo) &*&
                    [_]cd(cd, es, tracker, counter, statusCell, op);
                lemma void bsMem() : rdcss_bs_membership_lemma
                    requires
                        rdcss_bs_membership_lemma(bsMem)(?rdcssUnsep, ?info, ?a) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep, ?aas, ?avs, ?bs);
                    ensures
                        rdcss_bs_membership_lemma(bsMem)(rdcssUnsep, info, a) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep, aas, avs, bs) &*&
                        mem_assoc((void *)a, bs) == true;
                {
                    open rdcss_bs_membership_lemma(bsMem)(_, _, _);
                    open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
                    merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                    ghost_list_member_handle_lemma();
                    assert foreach3(?ds, ?sas, ?svs, _);
                    foreach3_separate(cd);
                    assert is_mcas_unsep(?unsep_);
                    open cdext(rcsList, unsep_, mcasInfo)(cd, _, ?status_);
                    merge_fractions cd(cd, _, _, _, _, _);
                    split_fraction cd(cd, _, _, _, _, _);
                    foreach_separate_ith(i, es);
                    open entry_mem(rcsList)(ith(i, es));
                    strong_ghost_assoc_list_key_handle_lemma();
                    close entry_mem(rcsList)(ith(i, es));
                    foreach_unseparate_ith_nochange(i, es);
                    close cdext(rcsList, unsep_, mcasInfo)(cd, &cd->status, status_);
                    foreach3_unseparate_nochange(ds, sas, svs, cd);
                    split_fraction ghost_cell6(id, _, _, _, _, _, _);
                    close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, rdcssSep, aas, avs, bs);
                    close rdcss_bs_membership_lemma(bsMem)(mcas_rdcss_unsep, boxed_int(id), a);
                }
                predicate_family_instance rdcss_cas_pre(casOp)(rdcss_unseparate_lemma *rdcssUnsep, boxed_int info, void **a2, void *o2, void *n2) =
                    rdcssUnsep == mcas_rdcss_unsep &*& info == boxed_int(id) &*&
                    a2 == fst(ith(i, es)) &*& o2 == (void *)((uintptr_t)cd | 2) &*&
                    n2 == (success ? snd(snd(ith(i, es))) : fst(snd(ith(i, es)))) &*&
                    true == (((uintptr_t)n2 & 2) == 0) &*&
                    [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                    [_]ghost_list_member_handle(dsList, cd) &*&
                    [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                    [_]cd->committed |-> true &*& [_]cd->success2 |-> success;
                predicate_family_instance rdcss_cas_post(casOp)(bool casSuccess) =
                    [_]ghost_cell6(id, rdcssId, rcsList, dsList, sep, unsep, mcasInfo) &*&
                    [_]ghost_list_member_handle(dsList, cd) &*&
                    [_]cd(cd, es, tracker, counter, statusCell, op) &*&
                    [_]cd->committed |-> true &*& [_]cd->success2 |-> success;
                lemma void casOp(bool casSuccess) : rdcss_cas_lemma
                    requires
                        rdcss_cas_pre(casOp)(?rdcssUnsep, ?info, ?a2, ?o2, ?n2) &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, ?rdcssId_, ?inv_, ?rdcssSep_, ?aas, ?avs, ?bs) &*&
                        mem_assoc(a2, bs) == true &*&
                        casSuccess ? assoc(a2, bs) == o2 : true;
                    ensures
                        rdcss_cas_post(casOp)(casSuccess) &*& mem_assoc(a2, bs) == true &*&
                        rdcss_unseparate_lemma(rdcssUnsep)(info, rdcssId_, inv_, rdcssSep_, aas, avs, ?bs1) &*&
                        casSuccess ?
                            bs1 == update(bs, a2, n2)
                        :
                            bs1 == bs;
                {
                    open rdcss_cas_pre(casOp)(_, _, _, _, _);
                    if (casSuccess) {
                        open rdcss_unseparate_lemma(mcas_rdcss_unsep)(_, _, _, _, _, _, _);
                        merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                        assert foreach_assoc2(?rcs, ?cs, _);
                        foreach_assoc2_separate(a2);
                        ghost_list_member_handle_lemma();
                        assert foreach3(?ds, ?sas, ?svs, _);
                        foreach3_separate(cd);
                        open cdext(rcsList, unsep, mcasInfo)(cd, _, ?status_);
                        merge_fractions cd(cd, _, _, _, _, _);
                        merge_fractions cd_committed(cd, _);
                        merge_fractions cd_success2(cd, ?success2);
                        bitand_bitor_lemma((uintptr_t)cd, 2);
                        open mcas_cell(rcsList, dsList)(a2, o2, assoc(a2, cs));
                        merge_fractions cd(cd, _, _, _, _, _);
                        counted_ghost_cell_match_ticket(statusCell);
                        strong_ghost_assoc_list_update(rcsList, a2, n2);
                        assoc_fst_ith_snd_ith(es, i);
                        close mcas_cell(rcsList, dsList)(a2, n2, n2);
                        foreach_assoc2_unseparate_1changed(rcs, cs, a2);
                        split_fraction cd(cd, _, _, _, _, _);
                        split_fraction cd_committed(cd, _);
                        split_fraction cd_success2(cd, _);
                        close cdext(rcsList, unsep, mcasInfo)(cd, &cd->status, status_);
                        foreach3_unseparate_nochange(ds, sas, svs, cd);
                        split_fraction ghost_cell6(id, _, _, _, _, _, _);
                        close rdcss_unseparate_lemma(mcas_rdcss_unsep)(boxed_int(id), rdcssId_, inv_, mcas_rdcss_sep, aas, avs, update(bs, a2, n2));
                        leak [_]ghost_list_member_handle(dsList, cd);
                        leak [_]ghost_counter_snapshot(counter, _);
                        leak [_]counted_ghost_cell_ticket(statusCell, _);
                    }
                    close rdcss_cas_post(casOp)(casSuccess);
                }
                @*/
                //@ entries_separate_ith(i);
                //@ split_fraction ghost_list_member_handle(dsList, cd);
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ split_fraction cd(cd, _, _, _, _, _);
                //@ close rdcss_cas_pre(casOp)(mcas_rdcss_unsep, boxed_int(id), fst(ith(i, es)), (void *)((uintptr_t)cd | 2), success ? snd(snd(ith(i, es))) : fst(snd(ith(i, es))));
                //@ split_fraction ghost_cell6(id, _, _, _, _, _, _);
                //@ close rdcss_bs_membership_lemma(bsMem)(mcas_rdcss_unsep, boxed_int(id), fst(ith(i, es)));
                //@ close rdcss_separate_lemma(mcas_rdcss_sep)(boxed_int(id), rdcssId, inv, mcas_rdcss_unsep);
                //@ produce_lemma_function_pointer_chunk(casOp);
                //@ produce_lemma_function_pointer_chunk(mcas_rdcss_sep);
                //@ produce_lemma_function_pointer_chunk(mcas_rdcss_unsep);
                //@ produce_lemma_function_pointer_chunk(bsMem);
                //@ bitand_bitor_1_2_lemma(cd);
                rdcss_compare_and_store((cd->es + i)->a, (void *)((uintptr_t)cd | 2), success ? (cd->es + i)->n : (cd->es + i)->o);
                //@ leak is_rdcss_cas_lemma(casOp);
                //@ leak is_rdcss_separate_lemma(mcas_rdcss_sep);
                //@ leak is_rdcss_unseparate_lemma(mcas_rdcss_unsep);
                //@ leak is_rdcss_bs_membership_lemma(bsMem);
                //@ open rdcss_separate_lemma(mcas_rdcss_sep)(_, _, _, _);
                //@ leak rdcss_bs_membership_lemma(_)(_, _, _);
                //@ open rdcss_cas_post(casOp)(_);
                //@ merge_fractions ghost_cell6(id, _, _, _, _, _, _);
                //@ entries_unseparate_ith(i, es);
            }
            i++;
        }
        //@ close [fcd]cd(cd, es, tracker, counter, statusCell, op);
        //@ leak [fcd]cd(cd, _, _, _, _, _);
    }
    return success;
}