/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/


#include <aghdr.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

int agobjidcmpf(Dict_t * dict, void *arg0, void *arg1, Dtdisc_t * disc)
{
    Agobj_t *obj0, *obj1;

    NOTUSED(dict);
    obj0 = arg0;
    obj1 = arg1;
    NOTUSED(disc);
    return AGID(obj0) - AGID(obj1);
}

int agobjseqcmpf(Dict_t * dict, void *arg0, void *arg1, Dtdisc_t * disc)
{
    Agobj_t *obj0, *obj1;

    NOTUSED(dict);
    obj0 = arg0;
    obj1 = arg1;
    NOTUSED(disc);
    return AGSEQ(obj0) - AGSEQ(obj1);
}

Agobj_t *agrebind(Agraph_t * g, Agobj_t * obj)
{
    Agraph_t *h;
    Agobj_t *nobj;

    h = agraphof(obj);
    if (h == g)
	return obj;

    switch (AGTYPE(obj)) {
    case AGNODE:
	nobj = (Agobj_t *) agsubnode(g, (Agnode_t *) obj, FALSE);
	break;
    case AGINEDGE:
    case AGOUTEDGE:
	nobj = (Agobj_t *) agsubedge(g, (Agedge_t *) obj, FALSE);
	break;
    case AGRAPH:
	nobj = (Agobj_t *) g;
	break;
    default:
	nobj = 0;
	agerror(AGERROR_BADOBJ, "agrebind");
    }
    return nobj;
}

int agdelete(Agraph_t * g, void *obj)
{
    Agraph_t *h;

    h = agraphof(obj);
    if ((g != h)
	&& ((AGTYPE((Agobj_t *) obj) != AGRAPH) || (g != agparent(h))))
	agerror(AGERROR_WRONGGRAPH, "agdelete");

    switch (AGTYPE((Agobj_t *) obj)) {
    case AGNODE:
	return agdelnode(obj);
    case AGINEDGE:
    case AGOUTEDGE:
	return agdeledge(obj);
    case AGRAPH:
	return agclose(obj);
    default:
	agerror(AGERROR_BADOBJ, "agdelete");
    }
    return SUCCESS;		/* not reached */
}

int agrename(Agobj_t * obj, char *newname)
{
    Agraph_t *g;
    unsigned long old_id, new_id;

    switch (AGTYPE(obj)) {
    case AGRAPH:
	old_id = AGID(obj);
	g = agraphof(obj);
	/* can we reserve the id corresponding to newname? */
	if (agmapnametoid(agroot(g), AGTYPE(obj), newname,
			  &new_id, FALSE) == 0)
	    return FAILURE;
	if (new_id == old_id)
	    return SUCCESS;
	if (agmapnametoid(agroot(g), AGTYPE(obj), newname,
			  &new_id, TRUE) == 0)
	    return FAILURE;
	if (agparent(g) && agidsubg(agparent(g), new_id, 0))
	    return FAILURE;
	agfreeid(g, AGRAPH, old_id);
	AGID(g) = new_id;
	break;
    case AGNODE:
	return agrelabel_node((Agnode_t *) obj, newname);
    case AGINEDGE:
    case AGOUTEDGE:
	return FAILURE;
    }
    return SUCCESS;
}

/* perform initialization/update/finalization method invocation.
 * skip over nil pointers to next method below.
 */

void agmethod_init(Agraph_t * g, void *obj)
{
    if (g->clos->callbacks_enabled)
	aginitcb(obj, g->clos->cb);
    else
	agrecord_callback(obj, CB_INITIALIZE, NILsym);
}

void aginitcb(void *obj, Agcbstack_t * cbstack)
{
    agobjfn_t fn;

    if (cbstack == NIL(Agcbstack_t *))
	return;
    aginitcb(obj, cbstack->prev);
    fn = NIL(agobjfn_t);
    switch (AGTYPE(obj)) {
    case AGRAPH:
	fn = cbstack->f->graph.ins;
	break;
    case AGNODE:
	fn = cbstack->f->node.ins;
	break;
    case AGEDGE:
	fn = cbstack->f->edge.ins;
	break;
    }
    if (fn)
	fn(obj, cbstack->state);
}

void agmethod_upd(Agraph_t * g, void *obj, Agsym_t * sym)
{
    if (g->clos->callbacks_enabled)
	agupdcb(obj, sym, g->clos->cb);
    else
	agrecord_callback(obj, CB_UPDATE, sym);
}

void agupdcb(void *obj, Agsym_t * sym, Agcbstack_t * cbstack)
{
    agobjupdfn_t fn;

    if (cbstack == NIL(Agcbstack_t *))
	return;
    agupdcb(obj, sym, cbstack->prev);
    fn = NIL(agobjupdfn_t);
    switch (AGTYPE(obj)) {
    case AGRAPH:
	fn = cbstack->f->graph.mod;
	break;
    case AGNODE:
	fn = cbstack->f->node.mod;
	break;
    case AGEDGE:
	fn = cbstack->f->edge.mod;
	break;
    }
    if (fn)
	fn(obj, cbstack->state, sym);
}

void agmethod_delete(Agraph_t * g, void *obj)
{
    if (g->clos->callbacks_enabled)
	agdelcb(obj, g->clos->cb);
    else
	agrecord_callback(obj, CB_DELETION, NILsym);
}

void agdelcb(void *obj, Agcbstack_t * cbstack)
{
    agobjfn_t fn;

    if (cbstack == NIL(Agcbstack_t *))
	return;
    agdelcb(obj, cbstack->prev);
    fn = NIL(agobjfn_t);
    switch (AGTYPE(obj)) {
    case AGRAPH:
	fn = cbstack->f->graph.del;
	break;
    case AGNODE:
	fn = cbstack->f->node.del;
	break;
    case AGEDGE:
	fn = cbstack->f->edge.del;
	break;
    }
    if (fn)
	fn(obj, cbstack->state);
}

Agraph_t *agraphof(void *obj)
{
    switch (AGTYPE(obj)) {
    case AGINEDGE:
    case AGOUTEDGE:
	return ((Agedge_t *) obj)->node->g;
    case AGNODE:
	return ((Agnode_t *) obj)->g;
    case AGRAPH:
	return (Agraph_t *) obj;
    default:			/* actually can't occur if only 2 bit tags */
	agerror(AGERROR_BADOBJ, "agraphof");
	return NILgraph;
    }
}

int agisarootobj(void *obj)
{
    return (agraphof(obj)->desc.maingraph);
}

/* to manage disciplines */

void agpushdisc(Agraph_t * g, Agcbdisc_t * cbd, void *state)
{
    Agcbstack_t *stack_ent;

    stack_ent = AGNEW(g, Agcbstack_t);
    stack_ent->f = cbd;
    stack_ent->state = state;
    stack_ent->prev = g->clos->cb;
    g->clos->cb = stack_ent;
}

int agpopdisc(Agraph_t * g, Agcbdisc_t * cbd)
{
    Agcbstack_t *stack_ent;

    stack_ent = g->clos->cb;
    if (stack_ent) {
	if (stack_ent->f == cbd)
	    g->clos->cb = stack_ent->prev;
	else {
	    while (stack_ent && (stack_ent->prev->f != cbd))
		stack_ent = stack_ent->prev;
	    if (stack_ent && stack_ent->prev)
		stack_ent->prev = stack_ent->prev->prev;
	}
	if (stack_ent) {
	    agfree(g, stack_ent);
	    return SUCCESS;
	}
    }
    return FAILURE;
}

void *aggetuserptr(Agraph_t * g, Agcbdisc_t * cbd)
{
    Agcbstack_t *stack_ent;

    for (stack_ent = g->clos->cb; stack_ent; stack_ent = stack_ent->prev)
	if (stack_ent->f == cbd)
	    return stack_ent->state;
    return NIL(void *);
}

Dtdisc_t Ag_obj_id_disc = {
    0,				/* pass object ptr      */
    0,				/* size (ignored)       */
    offsetof(Agobj_t, id_link),	/* link offset */
    NIL(Dtmake_f),
    NIL(Dtfree_f),
    agobjidcmpf,
    NIL(Dthash_f),
    agdictobjmem,
    NIL(Dtevent_f)
};

Dtdisc_t Ag_obj_seq_disc = {
    0,				/* pass object ptr      */
    0,				/* size (ignored)       */
    offsetof(Agobj_t, seq_link),	/* link offset */
    NIL(Dtmake_f),
    NIL(Dtfree_f),
    agobjseqcmpf,
    NIL(Dthash_f),
    agdictobjmem,
    NIL(Dtevent_f)
};
