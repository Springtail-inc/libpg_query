/*--------------------------------------------------------------------
 * Symbols referenced in this file:
 * - exprLocation
 * - leftmostLoc
 * - raw_expression_tree_walker_impl
 *--------------------------------------------------------------------
 */

/*-------------------------------------------------------------------------
 *
 * nodeFuncs.c
 *		Various general-purpose manipulations of Node trees
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/nodes/nodeFuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/execnodes.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

static bool expression_returns_set_walker(Node *node, void *context);
static int	leftmostLoc(int loc1, int loc2);
static bool fix_opfuncids_walker(Node *node, void *context);
static bool planstate_walk_subplans(List *plans,
									planstate_tree_walker_callback walker,
									void *context);
static bool planstate_walk_members(PlanState **planstates, int nplans,
								   planstate_tree_walker_callback walker,
								   void *context);


/*
 *	exprType -
 *	  returns the Oid of the type of the expression's result.
 */


/*
 *	exprTypmod -
 *	  returns the type-specific modifier of the expression's result type,
 *	  if it can be determined.  In many cases, it can't and we return -1.
 */


/*
 * exprIsLengthCoercion
 *		Detect whether an expression tree is an application of a datatype's
 *		typmod-coercion function.  Optionally extract the result's typmod.
 *
 * If coercedTypmod is not NULL, the typmod is stored there if the expression
 * is a length-coercion function, else -1 is stored there.
 *
 * Note that a combined type-and-length coercion will be treated as a
 * length coercion by this routine.
 */


/*
 * applyRelabelType
 *		Add a RelabelType node if needed to make the expression expose
 *		the specified type, typmod, and collation.
 *
 * This is primarily intended to be used during planning.  Therefore, it must
 * maintain the post-eval_const_expressions invariants that there are not
 * adjacent RelabelTypes, and that the tree is fully const-folded (hence,
 * we mustn't return a RelabelType atop a Const).  If we do find a Const,
 * we'll modify it in-place if "overwrite_ok" is true; that should only be
 * passed as true if caller knows the Const is newly generated.
 */


/*
 * relabel_to_typmod
 *		Add a RelabelType node that changes just the typmod of the expression.
 *
 * Convenience function for a common usage of applyRelabelType.
 */


/*
 * strip_implicit_coercions: remove implicit coercions at top level of tree
 *
 * This doesn't modify or copy the input expression tree, just return a
 * pointer to a suitable place within it.
 *
 * Note: there isn't any useful thing we can do with a RowExpr here, so
 * just return it unchanged, even if it's marked as an implicit coercion.
 */


/*
 * expression_returns_set
 *	  Test whether an expression returns a set result.
 *
 * Because we use expression_tree_walker(), this can also be applied to
 * whole targetlists; it'll produce true if any one of the tlist items
 * returns a set.
 */





/*
 *	exprCollation -
 *	  returns the Oid of the collation of the expression's result.
 *
 * Note: expression nodes that can invoke functions generally have an
 * "inputcollid" field, which is what the function should use as collation.
 * That is the resolved common collation of the node's inputs.  It is often
 * but not always the same as the result collation; in particular, if the
 * function produces a non-collatable result type from collatable inputs
 * or vice versa, the two are different.
 */


/*
 *	exprInputCollation -
 *	  returns the Oid of the collation a function should use, if available.
 *
 * Result is InvalidOid if the node type doesn't store this information.
 */


/*
 *	exprSetCollation -
 *	  Assign collation information to an expression tree node.
 *
 * Note: since this is only used during parse analysis, we don't need to
 * worry about subplans or PlaceHolderVars.
 */
#ifdef USE_ASSERT_CHECKING
#endif							/* USE_ASSERT_CHECKING */

/*
 *	exprSetInputCollation -
 *	  Assign input-collation information to an expression tree node.
 *
 * This is a no-op for node types that don't store their input collation.
 * Note we omit RowCompareExpr, which needs special treatment since it
 * contains multiple input collation OIDs.
 */



/*
 *	exprLocation -
 *	  returns the parse location of an expression tree, for error reports
 *
 * -1 is returned if the location can't be determined.
 *
 * For expressions larger than a single token, the intent here is to
 * return the location of the expression's leftmost token, not necessarily
 * the topmost Node's location field.  For example, an OpExpr's location
 * field will point at the operator name, but if it is not a prefix operator
 * then we should return the location of the left-hand operand instead.
 * The reason is that we want to reference the entire expression not just
 * that operator, and pointing to its start seems to be the most natural way.
 *
 * The location is not perfect --- for example, since the grammar doesn't
 * explicitly represent parentheses in the parsetree, given something that
 * had been written "(a + b) * c" we are going to point at "a" not "(".
 * But it should be plenty good enough for error reporting purposes.
 *
 * You might think that this code is overly general, for instance why check
 * the operands of a FuncExpr node, when the function name can be expected
 * to be to the left of them?  There are a couple of reasons.  The grammar
 * sometimes builds expressions that aren't quite what the user wrote;
 * for instance x IS NOT BETWEEN ... becomes a NOT-expression whose keyword
 * pointer is to the right of its leftmost argument.  Also, nodes that were
 * inserted implicitly by parse analysis (such as FuncExprs for implicit
 * coercions) will have location -1, and so we can have odd combinations of
 * known and unknown locations in a tree.
 */
int
exprLocation(const Node *expr)
{
	int			loc;

	if (expr == NULL)
		return -1;
	switch (nodeTag(expr))
	{
		case T_RangeVar:
			loc = ((const RangeVar *) expr)->location;
			break;
		case T_TableFunc:
			loc = ((const TableFunc *) expr)->location;
			break;
		case T_Var:
			loc = ((const Var *) expr)->location;
			break;
		case T_Const:
			loc = ((const Const *) expr)->location;
			break;
		case T_Param:
			loc = ((const Param *) expr)->location;
			break;
		case T_Aggref:
			/* function name should always be the first thing */
			loc = ((const Aggref *) expr)->location;
			break;
		case T_GroupingFunc:
			loc = ((const GroupingFunc *) expr)->location;
			break;
		case T_WindowFunc:
			/* function name should always be the first thing */
			loc = ((const WindowFunc *) expr)->location;
			break;
		case T_SubscriptingRef:
			/* just use container argument's location */
			loc = exprLocation((Node *) ((const SubscriptingRef *) expr)->refexpr);
			break;
		case T_FuncExpr:
			{
				const FuncExpr *fexpr = (const FuncExpr *) expr;

				/* consider both function name and leftmost arg */
				loc = leftmostLoc(fexpr->location,
								  exprLocation((Node *) fexpr->args));
			}
			break;
		case T_NamedArgExpr:
			{
				const NamedArgExpr *na = (const NamedArgExpr *) expr;

				/* consider both argument name and value */
				loc = leftmostLoc(na->location,
								  exprLocation((Node *) na->arg));
			}
			break;
		case T_OpExpr:
		case T_DistinctExpr:	/* struct-equivalent to OpExpr */
		case T_NullIfExpr:		/* struct-equivalent to OpExpr */
			{
				const OpExpr *opexpr = (const OpExpr *) expr;

				/* consider both operator name and leftmost arg */
				loc = leftmostLoc(opexpr->location,
								  exprLocation((Node *) opexpr->args));
			}
			break;
		case T_ScalarArrayOpExpr:
			{
				const ScalarArrayOpExpr *saopexpr = (const ScalarArrayOpExpr *) expr;

				/* consider both operator name and leftmost arg */
				loc = leftmostLoc(saopexpr->location,
								  exprLocation((Node *) saopexpr->args));
			}
			break;
		case T_BoolExpr:
			{
				const BoolExpr *bexpr = (const BoolExpr *) expr;

				/*
				 * Same as above, to handle either NOT or AND/OR.  We can't
				 * special-case NOT because of the way that it's used for
				 * things like IS NOT BETWEEN.
				 */
				loc = leftmostLoc(bexpr->location,
								  exprLocation((Node *) bexpr->args));
			}
			break;
		case T_SubLink:
			{
				const SubLink *sublink = (const SubLink *) expr;

				/* check the testexpr, if any, and the operator/keyword */
				loc = leftmostLoc(exprLocation(sublink->testexpr),
								  sublink->location);
			}
			break;
		case T_FieldSelect:
			/* just use argument's location */
			loc = exprLocation((Node *) ((const FieldSelect *) expr)->arg);
			break;
		case T_FieldStore:
			/* just use argument's location */
			loc = exprLocation((Node *) ((const FieldStore *) expr)->arg);
			break;
		case T_RelabelType:
			{
				const RelabelType *rexpr = (const RelabelType *) expr;

				/* Much as above */
				loc = leftmostLoc(rexpr->location,
								  exprLocation((Node *) rexpr->arg));
			}
			break;
		case T_CoerceViaIO:
			{
				const CoerceViaIO *cexpr = (const CoerceViaIO *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_ArrayCoerceExpr:
			{
				const ArrayCoerceExpr *cexpr = (const ArrayCoerceExpr *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_ConvertRowtypeExpr:
			{
				const ConvertRowtypeExpr *cexpr = (const ConvertRowtypeExpr *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_CollateExpr:
			/* just use argument's location */
			loc = exprLocation((Node *) ((const CollateExpr *) expr)->arg);
			break;
		case T_CaseExpr:
			/* CASE keyword should always be the first thing */
			loc = ((const CaseExpr *) expr)->location;
			break;
		case T_CaseWhen:
			/* WHEN keyword should always be the first thing */
			loc = ((const CaseWhen *) expr)->location;
			break;
		case T_ArrayExpr:
			/* the location points at ARRAY or [, which must be leftmost */
			loc = ((const ArrayExpr *) expr)->location;
			break;
		case T_RowExpr:
			/* the location points at ROW or (, which must be leftmost */
			loc = ((const RowExpr *) expr)->location;
			break;
		case T_RowCompareExpr:
			/* just use leftmost argument's location */
			loc = exprLocation((Node *) ((const RowCompareExpr *) expr)->largs);
			break;
		case T_CoalesceExpr:
			/* COALESCE keyword should always be the first thing */
			loc = ((const CoalesceExpr *) expr)->location;
			break;
		case T_MinMaxExpr:
			/* GREATEST/LEAST keyword should always be the first thing */
			loc = ((const MinMaxExpr *) expr)->location;
			break;
		case T_SQLValueFunction:
			/* function keyword should always be the first thing */
			loc = ((const SQLValueFunction *) expr)->location;
			break;
		case T_XmlExpr:
			{
				const XmlExpr *xexpr = (const XmlExpr *) expr;

				/* consider both function name and leftmost arg */
				loc = leftmostLoc(xexpr->location,
								  exprLocation((Node *) xexpr->args));
			}
			break;
		case T_JsonFormat:
			loc = ((const JsonFormat *) expr)->location;
			break;
		case T_JsonValueExpr:
			loc = exprLocation((Node *) ((const JsonValueExpr *) expr)->raw_expr);
			break;
		case T_JsonConstructorExpr:
			loc = ((const JsonConstructorExpr *) expr)->location;
			break;
		case T_JsonIsPredicate:
			loc = ((const JsonIsPredicate *) expr)->location;
			break;
		case T_NullTest:
			{
				const NullTest *nexpr = (const NullTest *) expr;

				/* Much as above */
				loc = leftmostLoc(nexpr->location,
								  exprLocation((Node *) nexpr->arg));
			}
			break;
		case T_BooleanTest:
			{
				const BooleanTest *bexpr = (const BooleanTest *) expr;

				/* Much as above */
				loc = leftmostLoc(bexpr->location,
								  exprLocation((Node *) bexpr->arg));
			}
			break;
		case T_CoerceToDomain:
			{
				const CoerceToDomain *cexpr = (const CoerceToDomain *) expr;

				/* Much as above */
				loc = leftmostLoc(cexpr->location,
								  exprLocation((Node *) cexpr->arg));
			}
			break;
		case T_CoerceToDomainValue:
			loc = ((const CoerceToDomainValue *) expr)->location;
			break;
		case T_SetToDefault:
			loc = ((const SetToDefault *) expr)->location;
			break;
		case T_TargetEntry:
			/* just use argument's location */
			loc = exprLocation((Node *) ((const TargetEntry *) expr)->expr);
			break;
		case T_IntoClause:
			/* use the contained RangeVar's location --- close enough */
			loc = exprLocation((Node *) ((const IntoClause *) expr)->rel);
			break;
		case T_List:
			{
				/* report location of first list member that has a location */
				ListCell   *lc;

				loc = -1;		/* just to suppress compiler warning */
				foreach(lc, (const List *) expr)
				{
					loc = exprLocation((Node *) lfirst(lc));
					if (loc >= 0)
						break;
				}
			}
			break;
		case T_A_Expr:
			{
				const A_Expr *aexpr = (const A_Expr *) expr;

				/* use leftmost of operator or left operand (if any) */
				/* we assume right operand can't be to left of operator */
				loc = leftmostLoc(aexpr->location,
								  exprLocation(aexpr->lexpr));
			}
			break;
		case T_ColumnRef:
			loc = ((const ColumnRef *) expr)->location;
			break;
		case T_ParamRef:
			loc = ((const ParamRef *) expr)->location;
			break;
		case T_A_Const:
			loc = ((const A_Const *) expr)->location;
			break;
		case T_FuncCall:
			{
				const FuncCall *fc = (const FuncCall *) expr;

				/* consider both function name and leftmost arg */
				/* (we assume any ORDER BY nodes must be to right of name) */
				loc = leftmostLoc(fc->location,
								  exprLocation((Node *) fc->args));
			}
			break;
		case T_A_ArrayExpr:
			/* the location points at ARRAY or [, which must be leftmost */
			loc = ((const A_ArrayExpr *) expr)->location;
			break;
		case T_ResTarget:
			/* we need not examine the contained expression (if any) */
			loc = ((const ResTarget *) expr)->location;
			break;
		case T_MultiAssignRef:
			loc = exprLocation(((const MultiAssignRef *) expr)->source);
			break;
		case T_TypeCast:
			{
				const TypeCast *tc = (const TypeCast *) expr;

				/*
				 * This could represent CAST(), ::, or TypeName 'literal', so
				 * any of the components might be leftmost.
				 */
				loc = exprLocation(tc->arg);
				loc = leftmostLoc(loc, tc->typeName->location);
				loc = leftmostLoc(loc, tc->location);
			}
			break;
		case T_CollateClause:
			/* just use argument's location */
			loc = exprLocation(((const CollateClause *) expr)->arg);
			break;
		case T_SortBy:
			/* just use argument's location (ignore operator, if any) */
			loc = exprLocation(((const SortBy *) expr)->node);
			break;
		case T_WindowDef:
			loc = ((const WindowDef *) expr)->location;
			break;
		case T_RangeTableSample:
			loc = ((const RangeTableSample *) expr)->location;
			break;
		case T_TypeName:
			loc = ((const TypeName *) expr)->location;
			break;
		case T_ColumnDef:
			loc = ((const ColumnDef *) expr)->location;
			break;
		case T_Constraint:
			loc = ((const Constraint *) expr)->location;
			break;
		case T_FunctionParameter:
			/* just use typename's location */
			loc = exprLocation((Node *) ((const FunctionParameter *) expr)->argType);
			break;
		case T_XmlSerialize:
			/* XMLSERIALIZE keyword should always be the first thing */
			loc = ((const XmlSerialize *) expr)->location;
			break;
		case T_GroupingSet:
			loc = ((const GroupingSet *) expr)->location;
			break;
		case T_WithClause:
			loc = ((const WithClause *) expr)->location;
			break;
		case T_InferClause:
			loc = ((const InferClause *) expr)->location;
			break;
		case T_OnConflictClause:
			loc = ((const OnConflictClause *) expr)->location;
			break;
		case T_CTESearchClause:
			loc = ((const CTESearchClause *) expr)->location;
			break;
		case T_CTECycleClause:
			loc = ((const CTECycleClause *) expr)->location;
			break;
		case T_CommonTableExpr:
			loc = ((const CommonTableExpr *) expr)->location;
			break;
		case T_JsonKeyValue:
			/* just use the key's location */
			loc = exprLocation((Node *) ((const JsonKeyValue *) expr)->key);
			break;
		case T_JsonObjectConstructor:
			loc = ((const JsonObjectConstructor *) expr)->location;
			break;
		case T_JsonArrayConstructor:
			loc = ((const JsonArrayConstructor *) expr)->location;
			break;
		case T_JsonArrayQueryConstructor:
			loc = ((const JsonArrayQueryConstructor *) expr)->location;
			break;
		case T_JsonAggConstructor:
			loc = ((const JsonAggConstructor *) expr)->location;
			break;
		case T_JsonObjectAgg:
			loc = exprLocation((Node *) ((const JsonObjectAgg *) expr)->constructor);
			break;
		case T_JsonArrayAgg:
			loc = exprLocation((Node *) ((const JsonArrayAgg *) expr)->constructor);
			break;
		case T_PlaceHolderVar:
			/* just use argument's location */
			loc = exprLocation((Node *) ((const PlaceHolderVar *) expr)->phexpr);
			break;
		case T_InferenceElem:
			/* just use nested expr's location */
			loc = exprLocation((Node *) ((const InferenceElem *) expr)->expr);
			break;
		case T_PartitionElem:
			loc = ((const PartitionElem *) expr)->location;
			break;
		case T_PartitionSpec:
			loc = ((const PartitionSpec *) expr)->location;
			break;
		case T_PartitionBoundSpec:
			loc = ((const PartitionBoundSpec *) expr)->location;
			break;
		case T_PartitionRangeDatum:
			loc = ((const PartitionRangeDatum *) expr)->location;
			break;
		default:
			/* for any other node type it's just unknown... */
			loc = -1;
			break;
	}
	return loc;
}

/*
 * leftmostLoc - support for exprLocation
 *
 * Take the minimum of two parse location values, but ignore unknowns
 */
static int
leftmostLoc(int loc1, int loc2)
{
	if (loc1 < 0)
		return loc2;
	else if (loc2 < 0)
		return loc1;
	else
		return Min(loc1, loc2);
}


/*
 * fix_opfuncids
 *	  Calculate opfuncid field from opno for each OpExpr node in given tree.
 *	  The given tree can be anything expression_tree_walker handles.
 *
 * The argument is modified in-place.  (This is OK since we'd want the
 * same change for any node, even if it gets visited more than once due to
 * shared structure.)
 */




/*
 * set_opfuncid
 *		Set the opfuncid (procedure OID) in an OpExpr node,
 *		if it hasn't been set already.
 *
 * Because of struct equivalence, this can also be used for
 * DistinctExpr and NullIfExpr nodes.
 */


/*
 * set_sa_opfuncid
 *		As above, for ScalarArrayOpExpr nodes.
 */



/*
 *	check_functions_in_node -
 *	  apply checker() to each function OID contained in given expression node
 *
 * Returns true if the checker() function does; for nodes representing more
 * than one function call, returns true if the checker() function does so
 * for any of those functions.  Returns false if node does not invoke any
 * SQL-visible function.  Caller must not pass node == NULL.
 *
 * This function examines only the given node; it does not recurse into any
 * sub-expressions.  Callers typically prefer to keep control of the recursion
 * for themselves, in case additional checks should be made, or because they
 * have special rules about which parts of the tree need to be visited.
 *
 * Note: we ignore MinMaxExpr, SQLValueFunction, XmlExpr, CoerceToDomain,
 * and NextValueExpr nodes, because they do not contain SQL function OIDs.
 * However, they can invoke SQL-visible functions, so callers should take
 * thought about how to treat them.
 */



/*
 * Standard expression-tree walking support
 *
 * We used to have near-duplicate code in many different routines that
 * understood how to recurse through an expression node tree.  That was
 * a pain to maintain, and we frequently had bugs due to some particular
 * routine neglecting to support a particular node type.  In most cases,
 * these routines only actually care about certain node types, and don't
 * care about other types except insofar as they have to recurse through
 * non-primitive node types.  Therefore, we now provide generic tree-walking
 * logic to consolidate the redundant "boilerplate" code.  There are
 * two versions: expression_tree_walker() and expression_tree_mutator().
 */

/*
 * expression_tree_walker() is designed to support routines that traverse
 * a tree in a read-only fashion (although it will also work for routines
 * that modify nodes in-place but never add/delete/replace nodes).
 * A walker routine should look like this:
 *
 * bool my_walker (Node *node, my_struct *context)
 * {
 *		if (node == NULL)
 *			return false;
 *		// check for nodes that special work is required for, eg:
 *		if (IsA(node, Var))
 *		{
 *			... do special actions for Var nodes
 *		}
 *		else if (IsA(node, ...))
 *		{
 *			... do special actions for other node types
 *		}
 *		// for any node type not specially processed, do:
 *		return expression_tree_walker(node, my_walker, (void *) context);
 * }
 *
 * The "context" argument points to a struct that holds whatever context
 * information the walker routine needs --- it can be used to return data
 * gathered by the walker, too.  This argument is not touched by
 * expression_tree_walker, but it is passed down to recursive sub-invocations
 * of my_walker.  The tree walk is started from a setup routine that
 * fills in the appropriate context struct, calls my_walker with the top-level
 * node of the tree, and then examines the results.
 *
 * The walker routine should return "false" to continue the tree walk, or
 * "true" to abort the walk and immediately return "true" to the top-level
 * caller.  This can be used to short-circuit the traversal if the walker
 * has found what it came for.  "false" is returned to the top-level caller
 * iff no invocation of the walker returned "true".
 *
 * The node types handled by expression_tree_walker include all those
 * normally found in target lists and qualifier clauses during the planning
 * stage.  In particular, it handles List nodes since a cnf-ified qual clause
 * will have List structure at the top level, and it handles TargetEntry nodes
 * so that a scan of a target list can be handled without additional code.
 * Also, RangeTblRef, FromExpr, JoinExpr, and SetOperationStmt nodes are
 * handled, so that query jointrees and setOperation trees can be processed
 * without additional code.
 *
 * expression_tree_walker will handle SubLink nodes by recursing normally
 * into the "testexpr" subtree (which is an expression belonging to the outer
 * plan).  It will also call the walker on the sub-Query node; however, when
 * expression_tree_walker itself is called on a Query node, it does nothing
 * and returns "false".  The net effect is that unless the walker does
 * something special at a Query node, sub-selects will not be visited during
 * an expression tree walk. This is exactly the behavior wanted in many cases
 * --- and for those walkers that do want to recurse into sub-selects, special
 * behavior is typically needed anyway at the entry to a sub-select (such as
 * incrementing a depth counter). A walker that wants to examine sub-selects
 * should include code along the lines of:
 *
 *		if (IsA(node, Query))
 *		{
 *			adjust context for subquery;
 *			result = query_tree_walker((Query *) node, my_walker, context,
 *									   0); // adjust flags as needed
 *			restore context if needed;
 *			return result;
 *		}
 *
 * query_tree_walker is a convenience routine (see below) that calls the
 * walker on all the expression subtrees of the given Query node.
 *
 * expression_tree_walker will handle SubPlan nodes by recursing normally
 * into the "testexpr" and the "args" list (which are expressions belonging to
 * the outer plan).  It will not touch the completed subplan, however.  Since
 * there is no link to the original Query, it is not possible to recurse into
 * subselects of an already-planned expression tree.  This is OK for current
 * uses, but may need to be revisited in future.
 */

#define WALK(n) walker((Node *) (n), context)
#define LIST_WALK(l) expression_tree_walker_impl((Node *) (l), walker, context)
#undef LIST_WALK

/*
 * query_tree_walker --- initiate a walk of a Query's expressions
 *
 * This routine exists just to reduce the number of places that need to know
 * where all the expression subtrees of a Query are.  Note it can be used
 * for starting a walk at top level of a Query regardless of whether the
 * walker intends to descend into subqueries.  It is also useful for
 * descending into subqueries within a walker.
 *
 * Some callers want to suppress visitation of certain items in the sub-Query,
 * typically because they need to process them specially, or don't actually
 * want to recurse into subqueries.  This is supported by the flags argument,
 * which is the bitwise OR of flag values to add or suppress visitation of
 * indicated items.  (More flag bits may be added as needed.)
 */


/*
 * range_table_walker is just the part of query_tree_walker that scans
 * a query's rangetable.  This is split out since it can be useful on
 * its own.
 */


/*
 * Some callers even want to scan the expressions in individual RTEs.
 */



/*
 * expression_tree_mutator() is designed to support routines that make a
 * modified copy of an expression tree, with some nodes being added,
 * removed, or replaced by new subtrees.  The original tree is (normally)
 * not changed.  Each recursion level is responsible for returning a copy of
 * (or appropriately modified substitute for) the subtree it is handed.
 * A mutator routine should look like this:
 *
 * Node * my_mutator (Node *node, my_struct *context)
 * {
 *		if (node == NULL)
 *			return NULL;
 *		// check for nodes that special work is required for, eg:
 *		if (IsA(node, Var))
 *		{
 *			... create and return modified copy of Var node
 *		}
 *		else if (IsA(node, ...))
 *		{
 *			... do special transformations of other node types
 *		}
 *		// for any node type not specially processed, do:
 *		return expression_tree_mutator(node, my_mutator, (void *) context);
 * }
 *
 * The "context" argument points to a struct that holds whatever context
 * information the mutator routine needs --- it can be used to return extra
 * data gathered by the mutator, too.  This argument is not touched by
 * expression_tree_mutator, but it is passed down to recursive sub-invocations
 * of my_mutator.  The tree walk is started from a setup routine that
 * fills in the appropriate context struct, calls my_mutator with the
 * top-level node of the tree, and does any required post-processing.
 *
 * Each level of recursion must return an appropriately modified Node.
 * If expression_tree_mutator() is called, it will make an exact copy
 * of the given Node, but invoke my_mutator() to copy the sub-node(s)
 * of that Node.  In this way, my_mutator() has full control over the
 * copying process but need not directly deal with expression trees
 * that it has no interest in.
 *
 * Just as for expression_tree_walker, the node types handled by
 * expression_tree_mutator include all those normally found in target lists
 * and qualifier clauses during the planning stage.
 *
 * expression_tree_mutator will handle SubLink nodes by recursing normally
 * into the "testexpr" subtree (which is an expression belonging to the outer
 * plan).  It will also call the mutator on the sub-Query node; however, when
 * expression_tree_mutator itself is called on a Query node, it does nothing
 * and returns the unmodified Query node.  The net effect is that unless the
 * mutator does something special at a Query node, sub-selects will not be
 * visited or modified; the original sub-select will be linked to by the new
 * SubLink node.  Mutators that want to descend into sub-selects will usually
 * do so by recognizing Query nodes and calling query_tree_mutator (below).
 *
 * expression_tree_mutator will handle a SubPlan node by recursing into the
 * "testexpr" and the "args" list (which belong to the outer plan), but it
 * will simply copy the link to the inner plan, since that's typically what
 * expression tree mutators want.  A mutator that wants to modify the subplan
 * can force appropriate behavior by recognizing SubPlan expression nodes
 * and doing the right thing.
 */

#define FLATCOPY(newnode, node, nodetype)  \
	( (newnode) = (nodetype *) palloc(sizeof(nodetype)), \
	  memcpy((newnode), (node), sizeof(nodetype)) )
#define MUTATE(newfield, oldfield, fieldtype)  \
		( (newfield) = (fieldtype) mutator((Node *) (oldfield), context) )


/*
 * query_tree_mutator --- initiate modification of a Query's expressions
 *
 * This routine exists just to reduce the number of places that need to know
 * where all the expression subtrees of a Query are.  Note it can be used
 * for starting a walk at top level of a Query regardless of whether the
 * mutator intends to descend into subqueries.  It is also useful for
 * descending into subqueries within a mutator.
 *
 * Some callers want to suppress mutating of certain items in the Query,
 * typically because they need to process them specially, or don't actually
 * want to recurse into subqueries.  This is supported by the flags argument,
 * which is the bitwise OR of flag values to suppress mutating of
 * indicated items.  (More flag bits may be added as needed.)
 *
 * Normally the top-level Query node itself is copied, but some callers want
 * it to be modified in-place; they must pass QTW_DONT_COPY_QUERY in flags.
 * All modified substructure is safely copied in any case.
 */


/*
 * range_table_mutator is just the part of query_tree_mutator that processes
 * a query's rangetable.  This is split out since it can be useful on
 * its own.
 */


/*
 * query_or_expression_tree_walker --- hybrid form
 *
 * This routine will invoke query_tree_walker if called on a Query node,
 * else will invoke the walker directly.  This is a useful way of starting
 * the recursion when the walker's normal change of state is not appropriate
 * for the outermost Query node.
 */


/*
 * query_or_expression_tree_mutator --- hybrid form
 *
 * This routine will invoke query_tree_mutator if called on a Query node,
 * else will invoke the mutator directly.  This is a useful way of starting
 * the recursion when the mutator's normal change of state is not appropriate
 * for the outermost Query node.
 */



/*
 * raw_expression_tree_walker --- walk raw parse trees
 *
 * This has exactly the same API as expression_tree_walker, but instead of
 * walking post-analysis parse trees, it knows how to walk the node types
 * found in raw grammar output.  (There is not currently any need for a
 * combined walker, so we keep them separate in the name of efficiency.)
 * Unlike expression_tree_walker, there is no special rule about query
 * boundaries: we descend to everything that's possibly interesting.
 *
 * Currently, the node type coverage here extends only to DML statements
 * (SELECT/INSERT/UPDATE/DELETE/MERGE) and nodes that can appear in them,
 * because this is used mainly during analysis of CTEs, and only DML
 * statements can appear in CTEs.
 */
bool
raw_expression_tree_walker_impl(Node *node,
								tree_walker_callback walker,
								void *context)
{
	ListCell   *temp;

	/*
	 * The walker has already visited the current node, and so we need only
	 * recurse into any sub-nodes it has.
	 */
	if (node == NULL)
		return false;

	/* Guard against stack overflow due to overly complex expressions */
	check_stack_depth();

	switch (nodeTag(node))
	{
		case T_JsonFormat:
		case T_SetToDefault:
		case T_CurrentOfExpr:
		case T_SQLValueFunction:
		case T_Integer:
		case T_Float:
		case T_Boolean:
		case T_String:
		case T_BitString:
		case T_ParamRef:
		case T_A_Const:
		case T_A_Star:
			/* primitive node types with no subnodes */
			break;
		case T_Alias:
			/* we assume the colnames list isn't interesting */
			break;
		case T_RangeVar:
			return WALK(((RangeVar *) node)->alias);
		case T_GroupingFunc:
			return WALK(((GroupingFunc *) node)->args);
		case T_SubLink:
			{
				SubLink    *sublink = (SubLink *) node;

				if (WALK(sublink->testexpr))
					return true;
				/* we assume the operName is not interesting */
				if (WALK(sublink->subselect))
					return true;
			}
			break;
		case T_CaseExpr:
			{
				CaseExpr   *caseexpr = (CaseExpr *) node;

				if (WALK(caseexpr->arg))
					return true;
				/* we assume walker doesn't care about CaseWhens, either */
				foreach(temp, caseexpr->args)
				{
					CaseWhen   *when = lfirst_node(CaseWhen, temp);

					if (WALK(when->expr))
						return true;
					if (WALK(when->result))
						return true;
				}
				if (WALK(caseexpr->defresult))
					return true;
			}
			break;
		case T_RowExpr:
			/* Assume colnames isn't interesting */
			return WALK(((RowExpr *) node)->args);
		case T_CoalesceExpr:
			return WALK(((CoalesceExpr *) node)->args);
		case T_MinMaxExpr:
			return WALK(((MinMaxExpr *) node)->args);
		case T_XmlExpr:
			{
				XmlExpr    *xexpr = (XmlExpr *) node;

				if (WALK(xexpr->named_args))
					return true;
				/* we assume walker doesn't care about arg_names */
				if (WALK(xexpr->args))
					return true;
			}
			break;
		case T_JsonReturning:
			return WALK(((JsonReturning *) node)->format);
		case T_JsonValueExpr:
			{
				JsonValueExpr *jve = (JsonValueExpr *) node;

				if (WALK(jve->raw_expr))
					return true;
				if (WALK(jve->formatted_expr))
					return true;
				if (WALK(jve->format))
					return true;
			}
			break;
		case T_JsonConstructorExpr:
			{
				JsonConstructorExpr *ctor = (JsonConstructorExpr *) node;

				if (WALK(ctor->args))
					return true;
				if (WALK(ctor->func))
					return true;
				if (WALK(ctor->coercion))
					return true;
				if (WALK(ctor->returning))
					return true;
			}
			break;
		case T_JsonIsPredicate:
			return WALK(((JsonIsPredicate *) node)->expr);
		case T_NullTest:
			return WALK(((NullTest *) node)->arg);
		case T_BooleanTest:
			return WALK(((BooleanTest *) node)->arg);
		case T_JoinExpr:
			{
				JoinExpr   *join = (JoinExpr *) node;

				if (WALK(join->larg))
					return true;
				if (WALK(join->rarg))
					return true;
				if (WALK(join->quals))
					return true;
				if (WALK(join->alias))
					return true;
				/* using list is deemed uninteresting */
			}
			break;
		case T_IntoClause:
			{
				IntoClause *into = (IntoClause *) node;

				if (WALK(into->rel))
					return true;
				/* colNames, options are deemed uninteresting */
				/* viewQuery should be null in raw parsetree, but check it */
				if (WALK(into->viewQuery))
					return true;
			}
			break;
		case T_List:
			foreach(temp, (List *) node)
			{
				if (WALK((Node *) lfirst(temp)))
					return true;
			}
			break;
		case T_InsertStmt:
			{
				InsertStmt *stmt = (InsertStmt *) node;

				if (WALK(stmt->relation))
					return true;
				if (WALK(stmt->cols))
					return true;
				if (WALK(stmt->selectStmt))
					return true;
				if (WALK(stmt->onConflictClause))
					return true;
				if (WALK(stmt->returningList))
					return true;
				if (WALK(stmt->withClause))
					return true;
			}
			break;
		case T_DeleteStmt:
			{
				DeleteStmt *stmt = (DeleteStmt *) node;

				if (WALK(stmt->relation))
					return true;
				if (WALK(stmt->usingClause))
					return true;
				if (WALK(stmt->whereClause))
					return true;
				if (WALK(stmt->returningList))
					return true;
				if (WALK(stmt->withClause))
					return true;
			}
			break;
		case T_UpdateStmt:
			{
				UpdateStmt *stmt = (UpdateStmt *) node;

				if (WALK(stmt->relation))
					return true;
				if (WALK(stmt->targetList))
					return true;
				if (WALK(stmt->whereClause))
					return true;
				if (WALK(stmt->fromClause))
					return true;
				if (WALK(stmt->returningList))
					return true;
				if (WALK(stmt->withClause))
					return true;
			}
			break;
		case T_MergeStmt:
			{
				MergeStmt  *stmt = (MergeStmt *) node;

				if (WALK(stmt->relation))
					return true;
				if (WALK(stmt->sourceRelation))
					return true;
				if (WALK(stmt->joinCondition))
					return true;
				if (WALK(stmt->mergeWhenClauses))
					return true;
				if (WALK(stmt->withClause))
					return true;
			}
			break;
		case T_MergeWhenClause:
			{
				MergeWhenClause *mergeWhenClause = (MergeWhenClause *) node;

				if (WALK(mergeWhenClause->condition))
					return true;
				if (WALK(mergeWhenClause->targetList))
					return true;
				if (WALK(mergeWhenClause->values))
					return true;
			}
			break;
		case T_SelectStmt:
			{
				SelectStmt *stmt = (SelectStmt *) node;

				if (WALK(stmt->distinctClause))
					return true;
				if (WALK(stmt->intoClause))
					return true;
				if (WALK(stmt->targetList))
					return true;
				if (WALK(stmt->fromClause))
					return true;
				if (WALK(stmt->whereClause))
					return true;
				if (WALK(stmt->groupClause))
					return true;
				if (WALK(stmt->havingClause))
					return true;
				if (WALK(stmt->windowClause))
					return true;
				if (WALK(stmt->valuesLists))
					return true;
				if (WALK(stmt->sortClause))
					return true;
				if (WALK(stmt->limitOffset))
					return true;
				if (WALK(stmt->limitCount))
					return true;
				if (WALK(stmt->lockingClause))
					return true;
				if (WALK(stmt->withClause))
					return true;
				if (WALK(stmt->larg))
					return true;
				if (WALK(stmt->rarg))
					return true;
			}
			break;
		case T_PLAssignStmt:
			{
				PLAssignStmt *stmt = (PLAssignStmt *) node;

				if (WALK(stmt->indirection))
					return true;
				if (WALK(stmt->val))
					return true;
			}
			break;
		case T_A_Expr:
			{
				A_Expr	   *expr = (A_Expr *) node;

				if (WALK(expr->lexpr))
					return true;
				if (WALK(expr->rexpr))
					return true;
				/* operator name is deemed uninteresting */
			}
			break;
		case T_BoolExpr:
			{
				BoolExpr   *expr = (BoolExpr *) node;

				if (WALK(expr->args))
					return true;
			}
			break;
		case T_ColumnRef:
			/* we assume the fields contain nothing interesting */
			break;
		case T_FuncCall:
			{
				FuncCall   *fcall = (FuncCall *) node;

				if (WALK(fcall->args))
					return true;
				if (WALK(fcall->agg_order))
					return true;
				if (WALK(fcall->agg_filter))
					return true;
				if (WALK(fcall->over))
					return true;
				/* function name is deemed uninteresting */
			}
			break;
		case T_NamedArgExpr:
			return WALK(((NamedArgExpr *) node)->arg);
		case T_A_Indices:
			{
				A_Indices  *indices = (A_Indices *) node;

				if (WALK(indices->lidx))
					return true;
				if (WALK(indices->uidx))
					return true;
			}
			break;
		case T_A_Indirection:
			{
				A_Indirection *indir = (A_Indirection *) node;

				if (WALK(indir->arg))
					return true;
				if (WALK(indir->indirection))
					return true;
			}
			break;
		case T_A_ArrayExpr:
			return WALK(((A_ArrayExpr *) node)->elements);
		case T_ResTarget:
			{
				ResTarget  *rt = (ResTarget *) node;

				if (WALK(rt->indirection))
					return true;
				if (WALK(rt->val))
					return true;
			}
			break;
		case T_MultiAssignRef:
			return WALK(((MultiAssignRef *) node)->source);
		case T_TypeCast:
			{
				TypeCast   *tc = (TypeCast *) node;

				if (WALK(tc->arg))
					return true;
				if (WALK(tc->typeName))
					return true;
			}
			break;
		case T_CollateClause:
			return WALK(((CollateClause *) node)->arg);
		case T_SortBy:
			return WALK(((SortBy *) node)->node);
		case T_WindowDef:
			{
				WindowDef  *wd = (WindowDef *) node;

				if (WALK(wd->partitionClause))
					return true;
				if (WALK(wd->orderClause))
					return true;
				if (WALK(wd->startOffset))
					return true;
				if (WALK(wd->endOffset))
					return true;
			}
			break;
		case T_RangeSubselect:
			{
				RangeSubselect *rs = (RangeSubselect *) node;

				if (WALK(rs->subquery))
					return true;
				if (WALK(rs->alias))
					return true;
			}
			break;
		case T_RangeFunction:
			{
				RangeFunction *rf = (RangeFunction *) node;

				if (WALK(rf->functions))
					return true;
				if (WALK(rf->alias))
					return true;
				if (WALK(rf->coldeflist))
					return true;
			}
			break;
		case T_RangeTableSample:
			{
				RangeTableSample *rts = (RangeTableSample *) node;

				if (WALK(rts->relation))
					return true;
				/* method name is deemed uninteresting */
				if (WALK(rts->args))
					return true;
				if (WALK(rts->repeatable))
					return true;
			}
			break;
		case T_RangeTableFunc:
			{
				RangeTableFunc *rtf = (RangeTableFunc *) node;

				if (WALK(rtf->docexpr))
					return true;
				if (WALK(rtf->rowexpr))
					return true;
				if (WALK(rtf->namespaces))
					return true;
				if (WALK(rtf->columns))
					return true;
				if (WALK(rtf->alias))
					return true;
			}
			break;
		case T_RangeTableFuncCol:
			{
				RangeTableFuncCol *rtfc = (RangeTableFuncCol *) node;

				if (WALK(rtfc->colexpr))
					return true;
				if (WALK(rtfc->coldefexpr))
					return true;
			}
			break;
		case T_TypeName:
			{
				TypeName   *tn = (TypeName *) node;

				if (WALK(tn->typmods))
					return true;
				if (WALK(tn->arrayBounds))
					return true;
				/* type name itself is deemed uninteresting */
			}
			break;
		case T_ColumnDef:
			{
				ColumnDef  *coldef = (ColumnDef *) node;

				if (WALK(coldef->typeName))
					return true;
				if (WALK(coldef->raw_default))
					return true;
				if (WALK(coldef->collClause))
					return true;
				/* for now, constraints are ignored */
			}
			break;
		case T_IndexElem:
			{
				IndexElem  *indelem = (IndexElem *) node;

				if (WALK(indelem->expr))
					return true;
				/* collation and opclass names are deemed uninteresting */
			}
			break;
		case T_GroupingSet:
			return WALK(((GroupingSet *) node)->content);
		case T_LockingClause:
			return WALK(((LockingClause *) node)->lockedRels);
		case T_XmlSerialize:
			{
				XmlSerialize *xs = (XmlSerialize *) node;

				if (WALK(xs->expr))
					return true;
				if (WALK(xs->typeName))
					return true;
			}
			break;
		case T_WithClause:
			return WALK(((WithClause *) node)->ctes);
		case T_InferClause:
			{
				InferClause *stmt = (InferClause *) node;

				if (WALK(stmt->indexElems))
					return true;
				if (WALK(stmt->whereClause))
					return true;
			}
			break;
		case T_OnConflictClause:
			{
				OnConflictClause *stmt = (OnConflictClause *) node;

				if (WALK(stmt->infer))
					return true;
				if (WALK(stmt->targetList))
					return true;
				if (WALK(stmt->whereClause))
					return true;
			}
			break;
		case T_CommonTableExpr:
			/* search_clause and cycle_clause are not interesting here */
			return WALK(((CommonTableExpr *) node)->ctequery);
		case T_JsonOutput:
			{
				JsonOutput *out = (JsonOutput *) node;

				if (WALK(out->typeName))
					return true;
				if (WALK(out->returning))
					return true;
			}
			break;
		case T_JsonKeyValue:
			{
				JsonKeyValue *jkv = (JsonKeyValue *) node;

				if (WALK(jkv->key))
					return true;
				if (WALK(jkv->value))
					return true;
			}
			break;
		case T_JsonObjectConstructor:
			{
				JsonObjectConstructor *joc = (JsonObjectConstructor *) node;

				if (WALK(joc->output))
					return true;
				if (WALK(joc->exprs))
					return true;
			}
			break;
		case T_JsonArrayConstructor:
			{
				JsonArrayConstructor *jac = (JsonArrayConstructor *) node;

				if (WALK(jac->output))
					return true;
				if (WALK(jac->exprs))
					return true;
			}
			break;
		case T_JsonAggConstructor:
			{
				JsonAggConstructor *ctor = (JsonAggConstructor *) node;

				if (WALK(ctor->output))
					return true;
				if (WALK(ctor->agg_order))
					return true;
				if (WALK(ctor->agg_filter))
					return true;
				if (WALK(ctor->over))
					return true;
			}
			break;
		case T_JsonObjectAgg:
			{
				JsonObjectAgg *joa = (JsonObjectAgg *) node;

				if (WALK(joa->constructor))
					return true;
				if (WALK(joa->arg))
					return true;
			}
			break;
		case T_JsonArrayAgg:
			{
				JsonArrayAgg *jaa = (JsonArrayAgg *) node;

				if (WALK(jaa->constructor))
					return true;
				if (WALK(jaa->arg))
					return true;
			}
			break;
		case T_JsonArrayQueryConstructor:
			{
				JsonArrayQueryConstructor *jaqc = (JsonArrayQueryConstructor *) node;

				if (WALK(jaqc->output))
					return true;
				if (WALK(jaqc->query))
					return true;
			}
			break;
		default:
            // NOTE: this does not handle DDL statements
            // unmodified it would lead to exit(1) being called
            // modified to change log level.
			elog(NOTICE, "unrecognized node type: %d",
				 (int) nodeTag(node));
			break;
	}
	return false;
}

/*
 * planstate_tree_walker --- walk plan state trees
 *
 * The walker has already visited the current node, and so we need only
 * recurse into any sub-nodes it has.
 */
#define PSWALK(n) walker(n, context)

/*
 * Walk a list of SubPlans (or initPlans, which also use SubPlan nodes).
 */


/*
 * Walk the constituent plans of a ModifyTable, Append, MergeAppend,
 * BitmapAnd, or BitmapOr node.
 */

