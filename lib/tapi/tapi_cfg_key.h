/** @file
 * @brief Test API to manage agent keys.
 *
 * API definitions to manage agent keys.
 *
 *
 * Copyright (C) 2004-2022 OKTET Labs. All rights reserved.
 *
 *
 * @author Artem Andreev <Artem.Andreev@oktetlabs.ru>
 */

#ifndef __TE_TAPI_CFG_KEY_H__
#define __TE_TAPI_CFG_KEY_H__

#include "te_config.h"

#include "te_defs.h"
#include "te_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup tapi_conf_key Agent keys subtree
 * @ingroup tapi_conf
 * @{
 */

/** Support key managers */
typedef enum tapi_cfg_key_manager {
    TAPI_CFG_KEY_MANAGER_SSH, /**< Keys by ssh-keygen */
} tapi_cfg_key_manager;

/**
 * Key types.
 *
 * Different key managers may have different sets of key types.
 */
typedef enum tapi_cfg_key_type {
    TAPI_CFG_KEY_TYPE_SSH_RSA,     /**< RSA key */
    TAPI_CFG_KEY_TYPE_SSH_DSA,     /**< DSA key */
    TAPI_CFG_KEY_TYPE_SSH_ECDSA,   /**< ECDSA key */
    TAPI_CFG_KEY_TYPE_SSH_ED25519, /**< Ed25519 */
} tapi_cfg_key_type;

/**
 * Key sizes.
 *
 * The values are abstract, not exact bit sizes, because
 * different key type may have totally different semantics of
 * a key size, therefore requesting a exact size rarely makes
 * any sense
 */
typedef enum tapi_cfg_key_size {
    /** Minimum allowed key size for a given type */
    TAPI_CFG_KEY_SIZE_MIN,
    /** Recommended key size for a given type */
    TAPI_CFG_KEY_SIZE_RECOMMENDED,
    /** Maximum allowed or reasonable key size for a given type */
    TAPI_CFG_KEY_SIZE_MAX,
} tapi_cfg_key_size;

/**
 * Key replacement modes.
 *
 * If a key does not exist, it is always created.
 */
typedef enum tapi_cfg_key_mode {
    /** Add a new key, fail if a key exists */
    TAPI_CFG_KEY_MODE_NEW,
    /** Reuse an exisiting key if parameters are the same */
    TAPI_CFG_KEY_MODE_REUSE,
    /** Replace an existing key if necessary */
    TAPI_CFG_KEY_MODE_REPLACE,
} tapi_cfg_key_mode;

/**
 * Check whether a key exists
 *
 * @param ta         Agent name
 * @param key_name   Key name
 *
 * @return @c TRUE if the key exists
 */
extern te_bool tapi_cfg_key_exists(const char *ta, const char *key_name);

/**
 * Add or replace a key with given parameters.
 *
 * @param ta          Agent name
 * @param key_name    Key name
 * @param manager     Key manager
 * @param type        Key type
 * @param size        Key size
 * @param mode        Key replacement mode.
 *                    If a key does not exist, it is always created
 *                    in any mode.
 *
 * @return                       Status code
 * @retval TE_EEXIST             New key has been requested, but a key
 *                               already exists
 * @retval TE_EBADSLT            A key cannot be reused due to
 *                               different parameters
 * @retval TE_TE_EPROTONOSUPPORT The agent does not support a requested manager
 *
 * @note Because keys may be generated by an external tool at the agent,
 *       there may not be simple diagnostics if something goes wrong there.
 *       An assortment of error codes may be returned, such as @c TE_ESHCMD,
 *       @c TE_EIO and others.
 */
extern te_errno tapi_cfg_key_add(const char *ta, const char *key_name,
                                 tapi_cfg_key_manager manager,
                                 tapi_cfg_key_type type,
                                 tapi_cfg_key_size size,
                                 tapi_cfg_key_mode mode);

/**
 * Get the real bit size of a generated key.
 *
 * @param ta        Agent name
 * @param key_name  Key_name
 *
 * @return The real bit size of a key
 * @retval 0        There is an error
 * @retval 1        This may be returned instead of a real size
 *                  for some key types which do not have a sensible
 *                  notion of a key bit size
 */
extern unsigned tapi_cfg_key_get_bitsize(const char *ta, const char *key_name);

/**
 * Get the private key file path at the agent side.
 *
 * The name shall not change if a key is re-generated.
 *
 * @param ta        Agent name
 * @param key_name  Key name
 *
 * @return Private key path (should be free()'d)
 * @retval @c NULL  An error happened
 */
extern char *tapi_cfg_key_get_private_key_path(const char *ta,
                                               const char *key_name);

/**
 * Get the public key.
 *
 * This is the real encoded public key string, _not_ a file name.
 * The string is guaranteed not to have any embedded zeroes.
 *
 * @param ta        Agent name
 * @param key_name  Key name
 *
 * @return Public key string (should be free()'d)
 * @retval @c NULL  An error happened
 */
extern char *tapi_cfg_key_get_public_key(const char *ta, const char *key_name);

/**
 * Delete a key from the agent.
 *
 * @param ta        Agent name
 * @param key_name  Key name
 *
 * @return Status code
 */
extern te_errno tapi_cfg_key_del(const char *ta, const char *key_name);


/**
 * Append a public key to a list of keys.
 *
 * Append the public key of @p key_name from @p ta to a file @p list_name
 * on @p dst_ta. If the file does not exist, it is created.
 * If @p list_name is relative, it is relative to @c /agent:dst_ta/tmp_dir
 *
 * @param ta        Source agent name
 * @param key_name  Key name
 * @param dst_ta    Destination agent name
 * @param list_name Key list file name
 *
 * @return Status code
 */
extern te_errno tapi_cfg_key_append_public(const char *ta, const char *key_name,
                                           const char *dst_ta,
                                           const char *list_name);

/**@} <!-- END tapi_conf_key --> */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_CFG_KEY_H__ */
