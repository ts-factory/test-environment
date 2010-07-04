/** @file
 * @brief Testing Results Comparator
 *
 * Definition of data types and API to use TRC.
 *
 *
 * Copyright (C) 2006 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Andrew Rybchenko <Andrew.Rybchenko@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef __TE_TRC_H__
#define __TE_TRC_H__

#include "te_errno.h"
#include "te_queue.h"
#include "tq_string.h"
#include "te_test_result.h"
#include "logic_expr.h"

#ifdef __cplusplus
extern "C" {
#endif


/** TE log test argument */
typedef struct trc_report_argument {
    char    *name;              /**< Argument name */
    char    *value;             /**< Argument value */
    te_bool  variable;          /**< Is this argument in fact a variable */
} trc_report_argument;


/**
 * Single test result with auxiliary information for TRC.
 */
typedef struct trc_exp_result_entry {
    TAILQ_ENTRY(trc_exp_result_entry)   links;  /**< List links */

    te_test_result  result; /**< Test result */

    /* Auxiliary information */
    char       *key;        /**< BugID-like information */
    char       *notes;      /**< Any kind of notes */
} trc_exp_result_entry;

/**
 * Expected test result.
 */
typedef struct trc_exp_result {
    SLIST_ENTRY(trc_exp_result) links;  /**< List links */

    char       *tags_str;   /**< String representation of tags logical
                                 expression */
    logic_expr *tags_expr;  /**< Tags logical expression */

    /** Results expected for such tags logical expression */
    TAILQ_HEAD(, trc_exp_result_entry)  results;

    /* Auxiliary information common for expected results */
    char       *key;        /**< BugID-like information */
    char       *notes;      /**< Any kind of notes */
} trc_exp_result;


/*
 * Testing Results Comparator database interface
 */

/* Forward declaration of the TRC database */
struct te_trc_db;
/** Short alias for TRC database type */
typedef struct te_trc_db te_trc_db;


/**
 * Initialize a new TRC database.
 *
 * @param p_trc_db      Location for TRC database instance handle
 *
 * @return Status code.
 *
 * @sa trc_db_save, trc_db_close
 */
extern te_errno trc_db_init(te_trc_db **p_trc_db);

/**
 * Open TRC database.
 *
 * @param location      Location of the database
 * @param p_trc_db      Location for TRC database instance handle
 *
 * @return Status code.
 *
 * @sa trc_db_save, trc_db_close
 */
extern te_errno trc_db_open(const char *location, te_trc_db **p_trc_db);

/**
 * Close TRC database.
 *
 * @param trc_db        TRC database instance handle
 */
extern void trc_db_close(te_trc_db *trc_db);


/*
 * Testing Results Comparator database walker 
 */

/* Forward declaration of the position in TRC database tree */
struct te_trc_db_walker;
/** Short alias for type to represent position in TRC database tree */
typedef struct te_trc_db_walker te_trc_db_walker;

/**
 * Allocate a new walker to traverse TRC database tree.
 *
 * @param trc_db        TRC database instance
 *
 * @return Pointer to allocated walker.
 */
extern te_trc_db_walker *trc_db_new_walker(te_trc_db *trc_db);

/**
 * Release resources allocated for TRC database tree walker.
 *
 * @param walker        TRC database tree walker
 */
extern void trc_db_free_walker(te_trc_db_walker *walker);

/**
 * Is walker located on a test iteration (or test itself)?
 * 
 * @param walker        Current walker position
 */
extern te_bool trc_db_walker_is_iter(const te_trc_db_walker *walker);

/**
 * Move walker from the current position to the child test with
 * specified name.
 *
 * @attention The function has to be called from initial walker
 *            position or iteration of any test.
 *
 * @param walker        Current walker position
 * @param test_name     Name of the test
 * @param force         Force to create DB entry, if it does not
 *                      exist (if resources allocation fails, FALSE is
 *                      returned, else TRUE is returned)
 *
 * @return Is walker in a known place in TRC database tree?
 */
extern te_bool trc_db_walker_step_test(te_trc_db_walker *walker,
                                       const char       *test_name,
                                       te_bool           force);

/**
 * Move walker from the current position to the test iteration with
 * specified arguments.
 *
 * @attention The function has to be called from the test entry
 *            position only.
 *
 * @param walker        Current walker position
 * @param n_args        Number of arguments
 * @param args          Test arguments
 * @param force         Force to create DB entry, if it does not
 *                      exist (if resources allocation fails, FALSE is
 *                      returned, else TRUE is returned)
 *
 * @note If @a force is @c TRUE and iteration is not found, @a names[]
 *       and @a values[] are owned by the function and corresponding
 *       pointers are substituted by @c NULL. It is assumed that
 *       @a names[] and @a values[] are dynamically allocated from heap.
 *
 * @return Is walker in a known place in TRC database tree?
 */
extern te_bool trc_db_walker_step_iter(te_trc_db_walker  *walker,
                                       unsigned int       n_args,
                                       trc_report_argument *args,
                                       te_bool            force);

/**
 * Move walker one step back.
 *
 * @param walker        Current walker position
 */
extern void trc_db_walker_step_back(te_trc_db_walker *walker);


/** Types of motion of the TRC database walker */
typedef enum trc_db_walker_motion {
    TRC_DB_WALKER_SON,      /**< To son */
    TRC_DB_WALKER_BROTHER,  /**< To brother */
    TRC_DB_WALKER_FATHER,   /**< To father */
    TRC_DB_WALKER_ROOT,     /**< Nowhere */
} trc_db_walker_motion;

/**
 * If previous motion is to nowhere, move to the root.
 * If previous motion is not to the parent, try to move to the first son.
 * Otherwise, move to the next brother, or to the parent, or nowhere.
 *
 * @param walker        Current walker position
 *
 * @return Made motion.
 */
extern trc_db_walker_motion trc_db_walker_move(te_trc_db_walker *walker);


/**
 * Get test iteration expected result at the current TRC database
 * walker position.
 *
 * @param walker        Current walker position
 * @param tags          Tags which identify IUT
 *
 * @return Expected result.
 * @return NULL         The test/iteration is unknown for TRC database.
 */
extern const trc_exp_result *trc_db_walker_get_exp_result(
                                 const te_trc_db_walker *walker,
                                 const tqh_strings      *tags);


/*
 * Comparison routines.
 */

/**
 * Is obtained result equal to one of expected?
 *
 * @param expected      Expected results
 * @param obtained      Obtained result
 *
 * @return Pointer to the entry in expected result which is equal
 *         to obtained result.
 */
extern const trc_exp_result_entry *trc_is_result_expected(
                                       const trc_exp_result *expected,
                                       const te_test_result *obtained);

/**
 * Is expected result equal to skipped (without any verdicts).
 *
 * @param result        Expected result to check
 */
extern te_bool trc_is_exp_result_skipped(const trc_exp_result *result);


/*
 * Attached user data management.
 */

/**
 * Allocate a new user ID.
 *
 * @param db            TRC database handle
 *
 * @return Allocated user ID.
 */
extern unsigned int trc_db_new_user(te_trc_db *db);

/**
 * Free user ID.
 *
 * @param db            TRC database handle
 * @param user_id       User ID to free
 */
extern void trc_db_free_user(te_trc_db *db, unsigned int user_id);

/**
 * Get data associated by user with current position in TRC database.
 *
 * @param walker        TRC database walker
 * @param user_id       User ID
 *
 * @return Data associated by user or NULL.
 */
extern void *trc_db_walker_get_user_data(const te_trc_db_walker *walker,
                                         unsigned int            user_id);

/**
 * Set data associated by user with current position in TRC database.
 *
 * @param walker        TRC database walker
 * @param user_id       User ID
 * @param user_data     User data to associate
 *
 * @return Status code.
 */
extern te_errno trc_db_walker_set_user_data(
                    const te_trc_db_walker *walker,
                    unsigned int            user_id,
                    void                   *user_data);

/**
 * Free user data associated by user with current position in TRC
 * database.
 *
 * @param walker        TRC database walker
 * @param user_id       User ID
 * @param user_free     Function to be used to free user data or NULL
 */
extern void trc_db_walker_free_user_data(te_trc_db_walker *walker,
                                         unsigned int user_id,
                                         void (*user_free)(void *));

/**
 * Free all data of specified user associated with elements of TRC
 * database.
 *
 * @param db            TRC database handle
 * @param user_id       User ID
 * @param test_free     Function to be used to free user data 
 *                      associated with test entries or NULL
 * @param iter_free     Function to be used to free user data 
 *                      associated with test iterations or NULL
 *
 * @return Status code.
 */
extern te_errno trc_db_free_user_data(te_trc_db *db, 
                                      unsigned int user_id,
                                      void (*test_free)(void *),
                                      void (*iter_free)(void *));

extern int (*trc_db_compare_values)(const char *s1, const char *s2);

extern int trc_db_strcmp_tokens(const char *s1, const char *s2);
extern int trc_db_strcmp_normspace(const char *s1, const char *s2);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TRC_H__ */
