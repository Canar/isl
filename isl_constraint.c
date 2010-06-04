/*
 * Copyright 2008-2009 Katholieke Universiteit Leuven
 * Copyright 2010      INRIA Saclay
 *
 * Use of this software is governed by the GNU LGPLv2.1 license
 *
 * Written by Sven Verdoolaege, K.U.Leuven, Departement
 * Computerwetenschappen, Celestijnenlaan 200A, B-3001 Leuven, Belgium
 * and INRIA Saclay - Ile-de-France, Parc Club Orsay Universite,
 * ZAC des vignes, 4 rue Jacques Monod, 91893 Orsay, France 
 */

#include <isl_constraint.h>
#include "isl_seq.h"
#include "isl_map_private.h"

static unsigned n(struct isl_constraint *c, enum isl_dim_type type)
{
	return isl_basic_map_dim(c->bmap, type);
}

static unsigned offset(struct isl_constraint *c, enum isl_dim_type type)
{
	struct isl_dim *dim = c->bmap->dim;
	switch (type) {
	case isl_dim_param:	return 1;
	case isl_dim_in:	return 1 + dim->nparam;
	case isl_dim_out:	return 1 + dim->nparam + dim->n_in;
	case isl_dim_div:	return 1 + dim->nparam + dim->n_in + dim->n_out;
	default:		return 0;
	}
}

static unsigned basic_set_offset(struct isl_basic_set *bset,
							enum isl_dim_type type)
{
	struct isl_dim *dim = bset->dim;
	switch (type) {
	case isl_dim_param:	return 1;
	case isl_dim_in:	return 1 + dim->nparam;
	case isl_dim_out:	return 1 + dim->nparam + dim->n_in;
	case isl_dim_div:	return 1 + dim->nparam + dim->n_in + dim->n_out;
	default:		return 0;
	}
}

struct isl_constraint *isl_basic_map_constraint(struct isl_basic_map *bmap,
	isl_int **line)
{
	struct isl_constraint *constraint;

	if (!bmap || !line)
		goto error;
	
	constraint = isl_alloc_type(bmap->ctx, struct isl_constraint);
	if (!constraint)
		goto error;

	constraint->ctx = bmap->ctx;
	isl_ctx_ref(constraint->ctx);
	constraint->ref = 1;
	constraint->bmap = bmap;
	constraint->line = line;

	return constraint;
error:
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_constraint *isl_basic_set_constraint(struct isl_basic_set *bset,
	isl_int **line)
{
	return isl_basic_map_constraint((struct isl_basic_map *)bset, line);
}

struct isl_constraint *isl_equality_alloc(struct isl_dim *dim)
{
	struct isl_basic_map *bmap;

	if (!dim)
		return NULL;

	bmap = isl_basic_map_alloc_dim(dim, 0, 1, 0);
	if (!bmap)
		return NULL;

	isl_basic_map_alloc_equality(bmap);
	isl_seq_clr(bmap->eq[0], 1 + isl_basic_map_total_dim(bmap));
	return isl_basic_map_constraint(bmap, &bmap->eq[0]);
}

struct isl_constraint *isl_inequality_alloc(struct isl_dim *dim)
{
	struct isl_basic_map *bmap;

	if (!dim)
		return NULL;

	bmap = isl_basic_map_alloc_dim(dim, 0, 0, 1);
	if (!bmap)
		return NULL;

	isl_basic_map_alloc_inequality(bmap);
	isl_seq_clr(bmap->ineq[0], 1 + isl_basic_map_total_dim(bmap));
	return isl_basic_map_constraint(bmap, &bmap->ineq[0]);
}

struct isl_constraint *isl_constraint_dup(struct isl_constraint *c)
{
	struct isl_basic_map *bmap;
	int i;
	int eq;

	if (!c)
		return NULL;

	eq = c->line < c->bmap->eq + c->bmap->n_eq;
	i = eq ? c->line - c->bmap->eq : c->line - c->bmap->ineq;
	bmap = isl_basic_map_copy(c->bmap);
	if (!bmap)
		return NULL;
	return isl_basic_map_constraint(bmap, eq ? bmap->eq + i : bmap->ineq + i);
}

struct isl_constraint *isl_constraint_cow(struct isl_constraint *c)
{
	if (!c)
		return NULL;

	if (c->ref == 1)
		return c;
	c->ref--;
	return isl_constraint_dup(c);
}

struct isl_constraint *isl_constraint_copy(struct isl_constraint *constraint)
{
	if (!constraint)
		return NULL;

	constraint->ref++;
	return constraint;
}

void isl_constraint_free(struct isl_constraint *c)
{
	if (!c)
		return;

	if (--c->ref > 0)
		return;

	isl_basic_map_free(c->bmap);
	isl_ctx_deref(c->ctx);
	free(c);
}

__isl_give isl_constraint *isl_basic_map_first_constraint(
	__isl_take isl_basic_map *bmap)
{
	if (!bmap)
		return NULL;

	if (bmap->n_eq > 0)
		return isl_basic_map_constraint(bmap, &bmap->eq[0]);

	if (bmap->n_ineq > 0)
		return isl_basic_map_constraint(bmap, &bmap->ineq[0]);

	isl_basic_map_free(bmap);
	return NULL;
}

__isl_give isl_constraint *isl_basic_set_first_constraint(
	__isl_take isl_basic_set *bset)
{
	return isl_basic_map_first_constraint((struct isl_basic_map *)bset);
}

struct isl_constraint *isl_constraint_next(struct isl_constraint *c)
{
	c = isl_constraint_cow(c);
	if (c->line >= c->bmap->eq) {
		c->line++;
		if (c->line < c->bmap->eq + c->bmap->n_eq)
			return c;
		c->line = c->bmap->ineq;
	} else
		c->line++;
	if (c->line < c->bmap->ineq + c->bmap->n_ineq)
		return c;
	isl_constraint_free(c);
	return NULL;
}

int isl_basic_map_foreach_constraint(__isl_keep isl_basic_map *bmap,
	int (*fn)(__isl_take isl_constraint *c, void *user), void *user)
{
	int i;
	struct isl_constraint *c;

	if (!bmap)
		return -1;

	isl_assert(bmap->ctx, ISL_F_ISSET(bmap, ISL_BASIC_MAP_FINAL),
			return -1);

	for (i = 0; i < bmap->n_eq; ++i) {
		c = isl_basic_map_constraint(isl_basic_map_copy(bmap),
						&bmap->eq[i]);
		if (!c)
			return -1;
		if (fn(c, user) < 0)
			return -1;
	}

	for (i = 0; i < bmap->n_ineq; ++i) {
		c = isl_basic_map_constraint(isl_basic_map_copy(bmap),
						&bmap->ineq[i]);
		if (!c)
			return -1;
		if (fn(c, user) < 0)
			return -1;
	}

	return 0;
}

int isl_basic_set_foreach_constraint(__isl_keep isl_basic_set *bset,
	int (*fn)(__isl_take isl_constraint *c, void *user), void *user)
{
	return isl_basic_map_foreach_constraint((isl_basic_map *)bset, fn, user);
}

int isl_constraint_is_equal(struct isl_constraint *constraint1,
	struct isl_constraint *constraint2)
{
	if (!constraint1 || !constraint2)
		return 0;
	return constraint1->bmap == constraint2->bmap &&
	       constraint1->line == constraint2->line;
}

struct isl_basic_map *isl_basic_map_add_constraint(
	struct isl_basic_map *bmap, struct isl_constraint *constraint)
{
	if (!bmap || !constraint)
		goto error;

	isl_assert(constraint->ctx,
		isl_dim_equal(bmap->dim, constraint->bmap->dim), goto error);

	bmap = isl_basic_map_intersect(bmap,
				isl_basic_map_from_constraint(constraint));
	return bmap;
error:
	isl_basic_map_free(bmap);
	isl_constraint_free(constraint);
	return NULL;
}

struct isl_basic_set *isl_basic_set_add_constraint(
	struct isl_basic_set *bset, struct isl_constraint *constraint)
{
	return (struct isl_basic_set *)
		isl_basic_map_add_constraint((struct isl_basic_map *)bset,
						constraint);
}

struct isl_constraint *isl_constraint_add_div(struct isl_constraint *constraint,
	struct isl_div *div, int *pos)
{
	if (!constraint || !div)
		goto error;

	isl_assert(constraint->ctx,
	    isl_dim_equal(div->bmap->dim, constraint->bmap->dim), goto error);
	isl_assert(constraint->ctx,
	    constraint->bmap->n_eq + constraint->bmap->n_ineq == 1, goto error);

	constraint->bmap = isl_basic_map_cow(constraint->bmap);
	constraint->bmap = isl_basic_map_extend_dim(constraint->bmap,
				isl_dim_copy(constraint->bmap->dim), 1, 0, 0);
	if (!constraint->bmap)
		goto error;
	constraint->line = &constraint->bmap->eq[0];
	*pos = isl_basic_map_alloc_div(constraint->bmap);
	if (*pos < 0)
		goto error;
	isl_seq_cpy(constraint->bmap->div[*pos], div->line[0],
			1 + 1 + isl_basic_map_total_dim(constraint->bmap));
	isl_div_free(div);
	return constraint;
error:
	isl_constraint_free(constraint);
	isl_div_free(div);
	return NULL;
}

int isl_constraint_dim(struct isl_constraint *constraint,
	enum isl_dim_type type)
{
	if (!constraint)
		return -1;
	return n(constraint, type);
}

void isl_constraint_get_constant(struct isl_constraint *constraint, isl_int *v)
{
	if (!constraint)
		return;
	isl_int_set(*v, constraint->line[0][0]);
}

void isl_constraint_get_coefficient(struct isl_constraint *constraint,
	enum isl_dim_type type, int pos, isl_int *v)
{
	if (!constraint)
		return;

	isl_assert(constraint->ctx, pos < n(constraint, type), return);
	isl_int_set(*v, constraint->line[0][offset(constraint, type) + pos]);
}

struct isl_div *isl_constraint_div(struct isl_constraint *constraint, int pos)
{
	if (!constraint)
		return NULL;

	isl_assert(constraint->ctx, pos < n(constraint, isl_dim_div),
			return NULL);
	isl_assert(constraint->ctx,
		!isl_int_is_zero(constraint->bmap->div[pos][0]), return NULL);
	return isl_basic_map_div(isl_basic_map_copy(constraint->bmap), pos);
}

void isl_constraint_set_constant(struct isl_constraint *constraint, isl_int v)
{
	if (!constraint)
		return;
	isl_int_set(constraint->line[0][0], v);
}

void isl_constraint_set_coefficient(struct isl_constraint *constraint,
	enum isl_dim_type type, int pos, isl_int v)
{
	if (!constraint)
		return;

	isl_assert(constraint->ctx, pos < n(constraint, type), return);
	isl_int_set(constraint->line[0][offset(constraint, type) + pos], v);
}

void isl_constraint_clear(struct isl_constraint *constraint)
{
	unsigned total;

	if (!constraint)
		return;
	total = isl_basic_map_total_dim(constraint->bmap);
	isl_seq_clr(constraint->line[0], 1 + total);
}

struct isl_constraint *isl_constraint_negate(struct isl_constraint *constraint)
{
	unsigned total;

	if (!constraint)
		return NULL;

	isl_assert(constraint->ctx, !isl_constraint_is_equality(constraint),
			goto error);
	isl_assert(constraint->ctx, constraint->bmap->ref == 1, goto error);
	total = isl_basic_map_total_dim(constraint->bmap);
	isl_seq_neg(constraint->line[0], constraint->line[0], 1 + total);
	isl_int_sub_ui(constraint->line[0][0], constraint->line[0][0], 1);
	ISL_F_CLR(constraint->bmap, ISL_BASIC_MAP_NORMALIZED);
	return constraint;
error:
	isl_constraint_free(constraint);
	return NULL;
}

int isl_constraint_is_equality(struct isl_constraint *constraint)
{
	if (!constraint)
		return -1;
	return constraint->line >= constraint->bmap->eq;
}

__isl_give isl_basic_map *isl_basic_map_from_constraint(
	__isl_take isl_constraint *constraint)
{
	int k;
	struct isl_basic_map *bmap;
	isl_int *c;
	unsigned total;

	if (!constraint)
		return NULL;

	if (constraint->bmap->n_eq + constraint->bmap->n_ineq == 1) {
		bmap = isl_basic_map_copy(constraint->bmap);
		isl_constraint_free(constraint);
		return bmap;
	}

	bmap = isl_basic_map_universe_like(constraint->bmap);
	bmap = isl_basic_map_align_divs(bmap, constraint->bmap);
	bmap = isl_basic_map_cow(bmap);
	bmap = isl_basic_map_extend_constraints(bmap, 1, 1);
	if (isl_constraint_is_equality(constraint)) {
		k = isl_basic_map_alloc_equality(bmap);
		if (k < 0)
			goto error;
		c = bmap->eq[k];
	}
	else {
		k = isl_basic_map_alloc_inequality(bmap);
		if (k < 0)
			goto error;
		c = bmap->ineq[k];
	}
	total = isl_basic_map_total_dim(bmap);
	isl_seq_cpy(c, constraint->line[0], 1 + total);
	isl_constraint_free(constraint);
	bmap = isl_basic_map_finalize(bmap);
	return bmap;
error:
	isl_constraint_free(constraint);
	isl_basic_map_free(bmap);
	return NULL;
}

struct isl_basic_set *isl_basic_set_from_constraint(
	struct isl_constraint *constraint)
{
	if (!constraint)
		return NULL;

	isl_assert(constraint->ctx,n(constraint, isl_dim_in) == 0, goto error);
	return (isl_basic_set *)isl_basic_map_from_constraint(constraint);
error:
	isl_constraint_free(constraint);
	return NULL;
}

int isl_basic_set_has_defining_equality(
	struct isl_basic_set *bset, enum isl_dim_type type, int pos,
	struct isl_constraint **c)
{
	int i;
	unsigned offset;
	unsigned total;

	if (!bset)
		return -1;
	offset = basic_set_offset(bset, type);
	total = isl_basic_set_total_dim(bset);
	isl_assert(bset->ctx, pos < isl_basic_set_dim(bset, type), return -1);
	for (i = 0; i < bset->n_eq; ++i)
		if (!isl_int_is_zero(bset->eq[i][offset + pos]) &&
		    isl_seq_first_non_zero(bset->eq[i]+offset+pos+1,
					   1+total-offset-pos-1) == -1) {
			*c= isl_basic_set_constraint(isl_basic_set_copy(bset),
								&bset->eq[i]);
			return 1;
		}
	return 0;
}

int isl_basic_set_has_defining_inequalities(
	struct isl_basic_set *bset, enum isl_dim_type type, int pos,
	struct isl_constraint **lower,
	struct isl_constraint **upper)
{
	int i, j;
	unsigned offset;
	unsigned total;
	isl_int m;
	isl_int **lower_line, **upper_line;

	if (!bset)
		return -1;
	offset = basic_set_offset(bset, type);
	total = isl_basic_set_total_dim(bset);
	isl_assert(bset->ctx, pos < isl_basic_set_dim(bset, type), return -1);
	isl_int_init(m);
	for (i = 0; i < bset->n_ineq; ++i) {
		if (isl_int_is_zero(bset->ineq[i][offset + pos]))
			continue;
		if (isl_int_is_one(bset->ineq[i][offset + pos]))
			continue;
		if (isl_int_is_negone(bset->ineq[i][offset + pos]))
			continue;
		if (isl_seq_first_non_zero(bset->ineq[i]+offset+pos+1,
						1+total-offset-pos-1) != -1)
			continue;
		for (j = i + 1; j < bset->n_ineq; ++j) {
			if (!isl_seq_is_neg(bset->ineq[i]+1, bset->ineq[j]+1,
					    total))
				continue;
			isl_int_add(m, bset->ineq[i][0], bset->ineq[j][0]);
			if (isl_int_abs_ge(m, bset->ineq[i][offset+pos]))
				continue;

			if (isl_int_is_pos(bset->ineq[i][offset+pos])) {
				lower_line = &bset->ineq[i];
				upper_line = &bset->ineq[j];
			} else {
				lower_line = &bset->ineq[j];
				upper_line = &bset->ineq[i];
			}
			*lower = isl_basic_set_constraint(
					isl_basic_set_copy(bset), lower_line);
			*upper = isl_basic_set_constraint(
					isl_basic_set_copy(bset), upper_line);
			isl_int_clear(m);
			return 1;
		}
	}
	*lower = NULL;
	*upper = NULL;
	isl_int_clear(m);
	return 0;
}

/* Given two constraints "a" and "b" on the variable at position "abs_pos"
 * (in "a" and "b"), add a constraint to "bset" that ensures that the
 * bound implied by "a" is (strictly) larger than the bound implied by "b".
 *
 * If both constraints imply lower bounds, then this means that "a" is
 * active in the result.
 * If both constraints imply upper bounds, then this means that "b" is
 * active in the result.
 */
static __isl_give isl_basic_set *add_larger_bound_constraint(
	__isl_take isl_basic_set *bset, isl_int *a, isl_int *b,
	unsigned abs_pos, int strict)
{
	int k;
	isl_int t;
	unsigned total;

	k = isl_basic_set_alloc_inequality(bset);
	if (k < 0)
		goto error;

	total = isl_basic_set_dim(bset, isl_dim_all);

	isl_int_init(t);
	isl_int_neg(t, b[1 + abs_pos]);

	isl_seq_combine(bset->ineq[k], t, a, a[1 + abs_pos], b, 1 + abs_pos);
	isl_seq_combine(bset->ineq[k] + 1 + abs_pos,
		t, a + 1 + abs_pos + 1, a[1 + abs_pos], b + 1 + abs_pos + 1,
		total - abs_pos);

	if (strict)
		isl_int_sub_ui(bset->ineq[k][0], bset->ineq[k][0], 1);

	isl_int_clear(t);

	return bset;
error:
	isl_basic_set_free(bset);
	return NULL;
}

/* Add constraints to "context" that ensure that "u" is the smallest
 * (and therefore active) upper bound on "abs_pos" in "bset" and return
 * the resulting basic set.
 */
static __isl_give isl_basic_set *set_smallest_upper_bound(
	__isl_keep isl_basic_set *context,
	__isl_keep isl_basic_set *bset, unsigned abs_pos, int n_upper, int u)
{
	int j;

	context = isl_basic_set_copy(context);
	context = isl_basic_set_cow(context);

	context = isl_basic_set_extend_constraints(context, 0, n_upper - 1);

	for (j = 0; j < bset->n_ineq; ++j) {
		if (j == u)
			continue;
		if (!isl_int_is_neg(bset->ineq[j][1 + abs_pos]))
			continue;
		context = add_larger_bound_constraint(context,
			bset->ineq[j], bset->ineq[u], abs_pos, j > u);
	}

	context = isl_basic_set_simplify(context);
	context = isl_basic_set_finalize(context);

	return context;
}

/* Add constraints to "context" that ensure that "u" is the largest
 * (and therefore active) upper bound on "abs_pos" in "bset" and return
 * the resulting basic set.
 */
static __isl_give isl_basic_set *set_largest_lower_bound(
	__isl_keep isl_basic_set *context,
	__isl_keep isl_basic_set *bset, unsigned abs_pos, int n_lower, int l)
{
	int j;

	context = isl_basic_set_copy(context);
	context = isl_basic_set_cow(context);

	context = isl_basic_set_extend_constraints(context, 0, n_lower - 1);

	for (j = 0; j < bset->n_ineq; ++j) {
		if (j == l)
			continue;
		if (!isl_int_is_pos(bset->ineq[j][1 + abs_pos]))
			continue;
		context = add_larger_bound_constraint(context,
			bset->ineq[l], bset->ineq[j], abs_pos, j > l);
	}

	context = isl_basic_set_simplify(context);
	context = isl_basic_set_finalize(context);

	return context;
}

static int foreach_upper_bound(__isl_keep isl_basic_set *bset,
	enum isl_dim_type type, unsigned abs_pos,
	__isl_take isl_basic_set *context, int n_upper,
	int (*fn)(__isl_take isl_constraint *lower,
		  __isl_take isl_constraint *upper,
		  __isl_take isl_basic_set *bset, void *user), void *user)
{
	isl_basic_set *context_i;
	isl_constraint *upper = NULL;
	int i;

	for (i = 0; i < bset->n_ineq; ++i) {
		if (isl_int_is_zero(bset->ineq[i][1 + abs_pos]))
			continue;

		context_i = set_smallest_upper_bound(context, bset,
							abs_pos, n_upper, i);
		if (isl_basic_set_is_empty(context_i)) {
			isl_basic_set_free(context_i);
			continue;
		}
		upper = isl_basic_set_constraint(isl_basic_set_copy(bset),
						&bset->ineq[i]);
		if (!upper || !context_i)
			goto error;
		if (fn(NULL, upper, context_i, user) < 0)
			break;
	}

	isl_basic_set_free(context);

	if (i < bset->n_ineq)
		return -1;

	return 0;
error:
	isl_constraint_free(upper);
	isl_basic_set_free(context_i);
	isl_basic_set_free(context);
	return -1;
}

static int foreach_lower_bound(__isl_keep isl_basic_set *bset,
	enum isl_dim_type type, unsigned abs_pos,
	__isl_take isl_basic_set *context, int n_lower,
	int (*fn)(__isl_take isl_constraint *lower,
		  __isl_take isl_constraint *upper,
		  __isl_take isl_basic_set *bset, void *user), void *user)
{
	isl_basic_set *context_i;
	isl_constraint *lower = NULL;
	int i;

	for (i = 0; i < bset->n_ineq; ++i) {
		if (isl_int_is_zero(bset->ineq[i][1 + abs_pos]))
			continue;

		context_i = set_largest_lower_bound(context, bset,
							abs_pos, n_lower, i);
		if (isl_basic_set_is_empty(context_i)) {
			isl_basic_set_free(context_i);
			continue;
		}
		lower = isl_basic_set_constraint(isl_basic_set_copy(bset),
						&bset->ineq[i]);
		if (!lower || !context_i)
			goto error;
		if (fn(lower, NULL, context_i, user) < 0)
			break;
	}

	isl_basic_set_free(context);

	if (i < bset->n_ineq)
		return -1;

	return 0;
error:
	isl_constraint_free(lower);
	isl_basic_set_free(context_i);
	isl_basic_set_free(context);
	return -1;
}

static int foreach_bound_pair(__isl_keep isl_basic_set *bset,
	enum isl_dim_type type, unsigned abs_pos,
	__isl_take isl_basic_set *context, int n_lower, int n_upper,
	int (*fn)(__isl_take isl_constraint *lower,
		  __isl_take isl_constraint *upper,
		  __isl_take isl_basic_set *bset, void *user), void *user)
{
	isl_basic_set *context_i, *context_j;
	isl_constraint *lower = NULL;
	isl_constraint *upper = NULL;
	int i, j;

	for (i = 0; i < bset->n_ineq; ++i) {
		if (!isl_int_is_pos(bset->ineq[i][1 + abs_pos]))
			continue;

		context_i = set_largest_lower_bound(context, bset,
							abs_pos, n_lower, i);
		if (isl_basic_set_is_empty(context_i)) {
			isl_basic_set_free(context_i);
			continue;
		}

		for (j = 0; j < bset->n_ineq; ++j) {
			if (!isl_int_is_neg(bset->ineq[j][1 + abs_pos]))
				continue;

			context_j = set_smallest_upper_bound(context_i, bset,
							    abs_pos, n_upper, j);
			context_j = isl_basic_set_extend_constraints(context_j,
									0, 1);
			context_j = add_larger_bound_constraint(context_j,
				bset->ineq[i], bset->ineq[j], abs_pos, 0);
			context_j = isl_basic_set_simplify(context_j);
			context_j = isl_basic_set_finalize(context_j);
			if (isl_basic_set_is_empty(context_j)) {
				isl_basic_set_free(context_j);
				continue;
			}
			lower = isl_basic_set_constraint(isl_basic_set_copy(bset),
							&bset->ineq[i]);
			upper = isl_basic_set_constraint(isl_basic_set_copy(bset),
							&bset->ineq[j]);
			if (!lower || !upper || !context_j)
				goto error;
			if (fn(lower, upper, context_j, user) < 0)
				break;
		}

		isl_basic_set_free(context_i);

		if (j < bset->n_ineq)
			break;
	}

	isl_basic_set_free(context);

	if (i < bset->n_ineq)
		return -1;

	return 0;
error:
	isl_constraint_free(lower);
	isl_constraint_free(upper);
	isl_basic_set_free(context_i);
	isl_basic_set_free(context_j);
	isl_basic_set_free(context);
	return -1;
}

/* For each pair of lower and upper bounds on the variable "pos"
 * of type "type", call "fn" with these lower and upper bounds and the
 * set of constraints on the remaining variables where these bounds
 * are active, i.e., (stricly) larger/smaller than the other lower/upper bounds.
 *
 * If the designated variable is equal to an affine combination of the
 * other variables then fn is called with both lower and upper
 * set to the corresponding equality.
 *
 * If there is no lower (or upper) bound, then NULL is passed
 * as the corresponding bound.
 *
 * We first check if the variable is involved in any equality.
 * If not, we count the number of lower and upper bounds and
 * act accordingly.
 */
int isl_basic_set_foreach_bound_pair(__isl_keep isl_basic_set *bset,
	enum isl_dim_type type, unsigned pos,
	int (*fn)(__isl_take isl_constraint *lower,
		  __isl_take isl_constraint *upper,
		  __isl_take isl_basic_set *bset, void *user), void *user)
{
	int i;
	isl_constraint *lower = NULL;
	isl_constraint *upper = NULL;
	isl_basic_set *context = NULL;
	unsigned abs_pos;
	int n_lower, n_upper;

	if (!bset)
		return -1;
	isl_assert(bset->ctx, pos < isl_basic_set_dim(bset, type), return -1);
	isl_assert(bset->ctx, type == isl_dim_param || type == isl_dim_set,
		return -1);

	abs_pos = pos;
	if (type == isl_dim_set)
		abs_pos += isl_basic_set_dim(bset, isl_dim_param);

	for (i = 0; i < bset->n_eq; ++i) {
		if (isl_int_is_zero(bset->eq[i][1 + abs_pos]))
			continue;

		lower = isl_basic_set_constraint(isl_basic_set_copy(bset),
						&bset->eq[i]);
		upper = isl_constraint_copy(lower);
		context = isl_basic_set_remove(isl_basic_set_copy(bset),
					type, pos, 1);
		if (!lower || !upper || !context)
			goto error;
		return fn(lower, upper, context, user);
	}

	n_lower = 0;
	n_upper = 0;
	for (i = 0; i < bset->n_ineq; ++i) {
		if (isl_int_is_pos(bset->ineq[i][1 + abs_pos]))
			n_lower++;
		else if (isl_int_is_neg(bset->ineq[i][1 + abs_pos]))
			n_upper++;
	}

	context = isl_basic_set_copy(bset);
	context = isl_basic_set_cow(context);
	if (!context)
		goto error;
	for (i = context->n_ineq - 1; i >= 0; --i)
		if (!isl_int_is_zero(context->ineq[i][1 + abs_pos]))
			isl_basic_set_drop_inequality(context, i);

	context = isl_basic_set_drop(context, type, pos, 1);
	if (!n_lower && !n_upper)
		return fn(NULL, NULL, context, user);
	if (!n_lower)
		return foreach_upper_bound(bset, type, abs_pos, context, n_upper,
						fn, user);
	if (!n_upper)
		return foreach_lower_bound(bset, type, abs_pos, context, n_lower,
						fn, user);
	return foreach_bound_pair(bset, type, abs_pos, context, n_lower, n_upper,
					fn, user);
error:
	isl_constraint_free(lower);
	isl_constraint_free(upper);
	isl_basic_set_free(context);
	return -1;
}
