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

/*
 *  graphics code generator wrapper
 *
 *  This library will eventually form the socket for run-time loadable
 *  render plugins.   Initially it just provides wrapper functions
 *  to the old codegens so that the changes can be locallized away from all
 *  the various codegen callers.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "const.h"
#include "types.h"
#include "macros.h"
#include "globals.h"
#include "graph.h"
#include "cdt.h"

#include "gvplugin_render.h"
#include "gvrender.h"

/* FIXME - need these but without rest of crap in common/ */
extern void colorxlate(char *str, color_t * color,
		       color_type_t target_type);
extern char *canontoken(char *str);

static point p0 = { 0, 0 };
static box b0 = { {0, 0}, {0, 0} };

int gvrender_select(GVC_t * gvc, char *str)
{
    gv_plugin_t *plugin;
    gvplugin_type_t *typeptr;
#if ENABLE_CODEGENS
    codegen_info_t *cg_info;
#endif

    plugin = gvplugin_load(gvc, API_render, str);
    if (plugin) {
#if ENABLE_CODEGENS
	if (strcmp(plugin->path, "cg") == 0) {
	    cg_info = (codegen_info_t *) (plugin->typeptr);
	    gvc->codegen = cg_info->cg;
	    return cg_info->id;
	} else {
#endif
	    typeptr = plugin->typeptr;
	    gvc->render_engine = (gvrender_engine_t *) (typeptr->engine);
	    gvc->render_features =
		(gvrender_features_t *) (typeptr->features);
	    gvc->render_id = typeptr->id;
	    return GVRENDER_PLUGIN;
#if ENABLE_CODEGENS
	}
#endif
    }
    return NO_SUPPORT;
}

int gvrender_features(GVC_t * gvc)
{
    int features = 0;
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre) {
	features = gvc->render_features->flags;
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg) {
	    if (cg->bezier_has_arrows)
		features |= GVRENDER_DOES_ARROWS;
	    if (cg->begin_layer)
		features |= GVRENDER_DOES_LAYERS;
	    /* WARNING - nasty hack to avoid modifying old codegens */
	    if (cg == &PS_CodeGen)
		features |= GVRENDER_DOES_MULTIGRAPH_OUTPUT_FILES;
	}
    }
#endif
    return features;
}

void gvrender_reset(GVC_t * gvc)
{
#if ENABLE_CODEGENS
    gvrender_engine_t *gvre = gvc->render_engine;

    if (!gvre) {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->reset)
	    cg->reset();
    }
#endif
}

void gvrender_begin_job(GVC_t * gvc, char **lib, point pages)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->lib = lib;
    gvc->pages = pages;
    if (gvre && gvre->begin_job)
//      gvre->begin_job(gvc, agget(g, "stylesheet"));
	gvre->begin_job(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_job)
	    cg->begin_job(gvc->job->output_file, gvc->g, lib, gvc->user,
			  gvc->info, pages);
    }
#endif
}

void gvrender_end_job(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_job)
	gvre->end_job(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_job)
	    cg->end_job();
    }
#endif
    gvc->lib = NULL;
    gvc->pages = p0;
}

/* font modifiers */
#define REGULAR 0
#define BOLD    1
#define ITALIC  2

static pointf gvrender_ptf(GVC_t * gvc, pointf p)
{
    pointf rv;

    if (gvc->rot == 0) {
	rv.x = (p.x - gvc->focus.x) * gvc->compscale.x + gvc->size.x / 2.;
	rv.y = (p.y - gvc->focus.y) * gvc->compscale.y + gvc->size.y / 2.;
    } else {
	rv.x = -(p.y - gvc->focus.y) * gvc->compscale.x + gvc->size.x / 2.;
	rv.y = (p.x - gvc->focus.x) * gvc->compscale.y + gvc->size.y / 2.;
    }
    return rv;
}

static pointf gvrender_pt(GVC_t * gvc, point p)
{
    pointf rv;

    if (gvc->rot == 0) {
	rv.x =
	    ((double) p.x - gvc->focus.x) * gvc->compscale.x +
	    gvc->size.x / 2.;
	rv.y =
	    ((double) p.y - gvc->focus.y) * gvc->compscale.y +
	    gvc->size.y / 2.;
    } else {
	rv.x =
	    -((double) p.y - gvc->focus.y) * gvc->compscale.x +
	    gvc->size.x / 2.;
	rv.y =
	    ((double) p.x - gvc->focus.x) * gvc->compscale.y +
	    gvc->size.y / 2.;
    }
    return rv;
}

static int gvrender_comparestr(const void *s1, const void *s2)
{
    return strcmp(*(char **) s1, *(char **) s2);
}

static void gvrender_resolve_color(gvrender_features_t * features,
				   char *name, color_t * color)
{
    char *tok;

    color->u.string = name;
    color->type = COLOR_STRING;
    tok = canontoken(name);
    if ((bsearch(&tok, features->knowncolors, features->sz_knowncolors,
		 sizeof(char *), gvrender_comparestr)) == NULL) {
	/* if tok was not found in known_colors */
	colorxlate(name, color, features->color_type);
    }
}

void gvrender_begin_graph(GVC_t * gvc, graph_t * g, box bb, point pb)
{
    gvrender_engine_t *gvre = gvc->render_engine;
    double dpi;
    char *str;

    gvc->g = g;
    gvc->bb = bb;
    gvc->pb = pb;
    dpi = gvc->dpi = GD_drawing(g)->dpi;
    gvc->scale = GD_drawing(g)->scale;
    gvc->margin = GD_drawing(g)->margin;

    if (gvre) {
	/* establish viewport and scaling */
	if (dpi < 1.0)
	    dpi = gvc->render_features->default_dpi;
	if (gvc->size.x == 0) {
	    gvc->size.x =
		(gvc->bb.UR.x - gvc->bb.LL.x +
		 2 * gvc->margin.x) * dpi / POINTS_PER_INCH + 2;
	    gvc->size.y =
		(gvc->bb.UR.y - gvc->bb.LL.y +
		 2 * gvc->margin.y) * dpi / POINTS_PER_INCH + 2;
	    gvc->focus.x = (GD_bb(gvc->g).UR.x - GD_bb(gvc->g).LL.x) / 2.;
	    gvc->focus.y = (GD_bb(gvc->g).UR.y - GD_bb(gvc->g).LL.y) / 2.;
	    gvc->zoom = 1.0;
	}
	gvc->compscale.x = gvc->zoom * gvc->scale * dpi / POINTS_PER_INCH;
	gvc->compscale.y =
	    gvc->compscale.x *
	    ((gvc->render_features->flags & GVRENDER_Y_GOES_DOWN) ? -1.0 : 1.0);

	/* render specific init */
	if (gvre->begin_graph)
	    gvre->begin_graph(gvc, g->name);

	/* background color */
	if (((str = agget(g, "bgcolor")) != 0) && str[0]) {
	    gvrender_resolve_color(gvc->render_features, str,
				   &(gvc->bgcolor));
	    if (gvre->resolve_color)
		gvre->resolve_color(gvc, &(gvc->bgcolor));
	}

	/* init stack */
	gvc->SP = 0;
	gvc->style = &(gvc->styles[0]);
	gvrender_set_pencolor(gvc, DEFAULT_COLOR);
	gvrender_set_fillcolor(gvc, DEFAULT_FILL);
	gvc->style->fontfam = DEFAULT_FONTNAME;
	gvc->style->fontsz = DEFAULT_FONTSIZE;
	gvc->style->fontopt = FONT_REGULAR;
	gvc->style->pen = PEN_SOLID;
	gvc->style->fill = PEN_NONE;
	gvc->style->penwidth = PENWIDTH_NORMAL;
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_graph)
	    cg->begin_graph(gvc, g, bb, pb);
    }
#endif
}

void gvrender_end_graph(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_graph)
	gvre->end_graph(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_graph)
	    cg->end_graph();
    }
#endif
    gvc->bb = b0;
    gvc->pb = p0;
}

void gvrender_begin_page(GVC_t * gvc, point page, double scale, int rot,
			 point offset)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->page = page;
    gvc->scale = scale;
    gvc->rot = rot;
//    gvc->offset = offset;
    gvc->page_number = page.x + page.y * gvc->pages.x + 1;
    if (gvre && gvre->begin_page)
	gvre->begin_page(gvc, gvc->g->name);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_page)
	    cg->begin_page(gvc->g, page, scale, rot, offset);
    }
#endif
}

void gvrender_end_page(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_page)
	gvre->end_page(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_page)
	    cg->end_page();
    }
#endif
}

void gvrender_begin_layer(GVC_t * gvc, char *layername, int layer,
			  int nLayers)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    gvc->layer = layer;
    gvc->nLayers = nLayers;
    if (gvre && gvre->begin_layer)
	gvre->begin_layer(gvc, layername);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_layer)
	    cg->begin_layer(layername, layer, nLayers);
    }
#endif
}

void gvrender_end_layer(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_layer)
	gvre->end_layer(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_layer)
	    cg->end_layer();
    }
#endif
    gvc->layer = 0;
    gvc->nLayers = 0;
}

void gvrender_begin_cluster(GVC_t * gvc, graph_t * sg)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_cluster)
	gvre->begin_cluster(gvc, sg->name, sg->meta_node->id);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_cluster)
	    cg->begin_cluster(sg);
    }
#endif
}

void gvrender_end_cluster(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_cluster)
	gvre->end_cluster(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_cluster)
	    cg->end_cluster();
    }
#endif
}

void gvrender_begin_nodes(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_nodes)
	gvre->begin_nodes(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_nodes)
	    cg->begin_nodes();
    }
#endif
}

void gvrender_end_nodes(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_nodes)
	gvre->end_nodes(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_nodes)
	    cg->end_nodes();
    }
#endif
}

void gvrender_begin_edges(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_edges)
	gvre->begin_edges(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_edges)
	    cg->begin_edges();
    }
#endif
}

void gvrender_end_edges(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_edges)
	gvre->end_edges(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_edges)
	    cg->end_edges();
    }
#endif
}

void gvrender_begin_node(GVC_t * gvc, node_t * n)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_node)
	gvre->begin_node(gvc, n->name, n->id);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_node)
	    cg->begin_node(n);
    }
#endif
}

void gvrender_end_node(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_node)
	gvre->end_node(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_node)
	    cg->end_node();
    }
#endif
}

void gvrender_begin_edge(GVC_t * gvc, edge_t * e)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_edge)
	gvre->begin_edge(gvc, e->tail->name,
			 e->tail->graph->root->kind & AGFLAG_DIRECTED,
			 e->head->name, e->id);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_edge)
	    cg->begin_edge(e);
    }
#endif
}

void gvrender_end_edge(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_edge)
	gvre->end_edge(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_edge)
	    cg->end_edge();
    }
#endif
}

void gvrender_begin_context(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre) {
	(gvc->SP)++;
	assert((gvc->SP) < MAXNEST);
	(gvc->styles)[gvc->SP] = (gvc->styles)[(gvc->SP) - 1];
	gvc->style = &((gvc->styles)[gvc->SP]);
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_context)
	    cg->begin_context();
    }
#endif
}

void gvrender_end_context(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre) {
	gvc->SP--;
	assert(gvc->SP >= 0);
	gvc->style = &(gvc->styles[gvc->SP]);
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_context)
	    cg->end_context();
    }
#endif
}

void gvrender_begin_anchor(GVC_t * gvc, char *href, char *tooltip,
			   char *target)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->begin_anchor)
	gvre->begin_anchor(gvc, href, tooltip, target);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->begin_anchor)
	    cg->begin_anchor(href, tooltip, target);
    }
#endif
}

void gvrender_end_anchor(GVC_t * gvc)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->end_anchor)
	gvre->end_anchor(gvc);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->end_anchor)
	    cg->end_anchor();
    }
#endif
}

void gvrender_set_font(GVC_t * gvc, char *fontname, double fontsize)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre) {
	gvc->style->fontfam = fontname;
	gvc->style->fontsz = fontsize;
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_font)
	    cg->set_font(fontname, fontsize);
    }
#endif
}

void gvrender_textline(GVC_t * gvc, pointf p, textline_t * line)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (line->str && line->str[0]) {
	if (gvre && gvre->textline) {
	    if (gvc->style->pen != PEN_NONE) {
		gvre->textline(gvc, gvrender_ptf(gvc, p), line);
	    }
	}
#if ENABLE_CODEGENS
	else {
	    codegen_t *cg = gvc->codegen;
	    point P;

	    PF2P(p, P);
	    if (cg && cg->textline)
		cg->textline(P, line);
	}
#endif
    }
}

void gvrender_set_pencolor(GVC_t * gvc, char *name)
{
    gvrender_engine_t *gvre = gvc->render_engine;
    color_t *color = &(gvc->style->pencolor);

    if (gvre) {
	gvrender_resolve_color(gvc->render_features, name, color);
	if (gvre->resolve_color)
	    gvre->resolve_color(gvc, color);
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_pencolor)
	    cg->set_pencolor(name);
    }
#endif
}

void gvrender_set_fillcolor(GVC_t * gvc, char *name)
{
    gvrender_engine_t *gvre = gvc->render_engine;
    color_t *color = &(gvc->style->fillcolor);

    if (gvre) {
	gvrender_resolve_color(gvc->render_features, name, color);
	if (gvre->resolve_color)
	    gvre->resolve_color(gvc, color);
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_fillcolor)
	    cg->set_fillcolor(name);
    }
#endif
}

void gvrender_set_style(GVC_t * gvc, char **s)
{
    gvrender_engine_t *gvre = gvc->render_engine;
    char *line, *p;
    gvstyle_t *style = gvc->style;

    if (gvre) {
	while ((p = line = *s++)) {
	    if (streq(line, "solid"))
		style->pen = PEN_SOLID;
	    else if (streq(line, "dashed"))
		style->pen = PEN_DASHED;
	    else if (streq(line, "dotted"))
		style->pen = PEN_DOTTED;
	    else if (streq(line, "invis"))
		style->pen = PEN_NONE;
	    else if (streq(line, "bold"))
		style->penwidth = PENWIDTH_BOLD;
	    else if (streq(line, "setlinewidth")) {
		while (*p)
		    p++;
		p++;
		style->penwidth = atol(p);
	    } else if (streq(line, "filled"))
		style->fill = FILL_SOLID;
	    else if (streq(line, "unfilled"))
		style->fill = FILL_NONE;
	    else {
		agerr(AGERR,
		      "svggen_set_style: unsupported style %s - ignoring\n",
		      line);
	    }
	}
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->set_style)
	    cg->set_style(s);
    }
#endif
}

void gvrender_ellipse(GVC_t * gvc, point p, int rx, int ry, int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->ellipse) {
	if (gvc->style->pen != PEN_NONE) {
/* temporary hack until client API is FP */
	    pointf AF[2];
	    int i;

	    /* left */
	    AF[0].x = (double) (p.x - rx);
	    AF[0].y = (double) p.y;
	    /* top */
	    AF[1].x = (double) p.x;
	    AF[1].y = (double) (p.y - ry);

/* end hack */
	    for (i = 0; i < 2; i++)
		AF[i] = gvrender_ptf(gvc, AF[i]);
	    gvre->ellipse(gvc, AF, filled);
	}
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->ellipse)
	    cg->ellipse(p, rx, ry, filled);
    }
#endif
}

void gvrender_polygon(GVC_t * gvc, point * A, int n, int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->polygon) {
	if (gvc->style->pen != PEN_NONE) {
/* temporary hack until client API is FP */
	    static pointf *AF;
	    static int sizeAF;
	    int i;

	    if (sizeAF < n)
		AF = realloc(AF, n * sizeof(pointf));
/* end hack */
	    for (i = 0; i < n; i++)
		AF[i] = gvrender_pt(gvc, A[i]);
	    gvre->polygon(gvc, AF, n, filled);
	}
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->polygon)
	    cg->polygon(A, n, filled);
    }
#endif
}

void gvrender_beziercurve(GVC_t * gvc, pointf * AF, int n,
			  int arrow_at_start, int arrow_at_end)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->beziercurve) {
	if (gvc->style->pen != PEN_NONE) {
	    static pointf *AF2;
	    static int sizeAF2;
	    int i;

	    if (sizeAF2 < n)
		AF2 = realloc(AF2, n * sizeof(pointf));
	    for (i = 0; i < n; i++)
		AF2[i] = gvrender_ptf(gvc, AF[i]);
	    gvre->beziercurve(gvc, AF2, n, arrow_at_start, arrow_at_end);
	}
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;
	/* hack for old codegen int API */
	static point *A;
	static int sizeA;
	int i;

	if (sizeA < n)
	    A = realloc(A, n * sizeof(point));
	for (i = 0; i < n; i++)
	    PF2P(AF[i], A[i]);
	/* end hack */

	if (cg && cg->beziercurve)
	    cg->beziercurve(A, n, arrow_at_start, arrow_at_end);
    }
#endif
}

void gvrender_polyline(GVC_t * gvc, point * A, int n)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->polyline) {
	if (gvc->style->pen != PEN_NONE) {
	    static pointf *AF;
	    static int sizeAF;
	    int i;

	    if (sizeAF < n)
		AF = realloc(AF, n * sizeof(pointf));
	    for (i = 0; i < n; i++)
		AF[i] = gvrender_pt(gvc, A[i]);
	    gvre->polyline(gvc, AF, n);
	}
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->polyline)
	    cg->polyline(A, n);
    }
#endif
}

void gvrender_comment(GVC_t * gvc, void *obj, attrsym_t * sym)
{
    gvrender_engine_t *gvre = gvc->render_engine;

    if (gvre && gvre->comment) {
	if (sym)
	    gvre->comment(gvc, agxget(obj, sym->index));
    }
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->comment)
	    cg->comment(obj, sym);
    }
#endif
}

void gvrender_user_shape(GVC_t * gvc, char *name, point * A, int n,
			 int filled)
{
    gvrender_engine_t *gvre = gvc->render_engine;

/* temporary hack until client API is FP */
    static pointf *AF;
    static int sizeAF;
    int i;

    if (sizeAF < n)
	AF = realloc(AF, n * sizeof(pointf));
    for (i = 0; i < n; i++)
	P2PF(A[i], AF[i]);
/* end hack */

    if (gvre && gvre->user_shape)
	gvre->user_shape(gvc, name, AF, n, filled);
#if ENABLE_CODEGENS
    else {
	codegen_t *cg = gvc->codegen;

	if (cg && cg->user_shape)
	    cg->user_shape(name, A, n, filled);
    }
#endif
}
