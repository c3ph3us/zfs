/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2016, Datto, Inc. All rights reserved.
 */

#ifndef	_SYS_DSL_KEYCHAIN_H
#define	_SYS_DSL_KEYCHAIN_H

#include <sys/dmu_tx.h>
#include <sys/dmu.h>
#include <sys/zio_crypt.h>
#include <sys/spa.h>
#include <sys/dsl_dataset.h>

typedef enum zfs_keystatus {
	ZFS_KEYSTATUS_NONE = 0,
	ZFS_KEYSTATUS_UNAVAILABLE,
	ZFS_KEYSTATUS_AVAILABLE,
} zfs_keystatus_t;

//physical representation of a wrapped key in the DSL Keychain
typedef struct dsl_crypto_key_phys {
	uint64_t dk_crypt_alg; //encryption algorithm (see zio_encrypt enum)
	uint8_t dk_iv[13]; //iv / nonce for unwrapping the key
	uint8_t dk_padding[3];
	uint8_t dk_keybuf[48]; //wrapped key data
} dsl_crypto_key_phys_t;

//in memory representation of an entry in the DSL Keychain
typedef struct dsl_keychain_entry {
	list_node_t ke_link; //link into the keychain
	uint64_t ke_txgid; //first txg id that this key should be applied to
	zio_crypt_key_t *ke_key; //the actual key that this entry represents 
} dsl_keychain_entry_t;

//in memory representation of a DSL keychain
typedef struct dsl_keychain {
	avl_node_t kc_avl_link; //avl node for linking into the keystore
	refcount_t kc_refcnt; //refcount of datasets holding this keychain (via dsl_keychain_record_t)
	krwlock_t kc_lock; //lock for protecting entry manipulations
	list_t kc_entries; //list of keychain entries
	zio_crypt_key_t *kc_wkey; //wrapping key for all entries
	uint64_t kc_obj; //keychain object id
} dsl_keychain_t;

//in memory mapping of a dataset to a DSL keychain
typedef struct dsl_keychain_record {
	avl_node_t kr_avl_link; //avl node for linking into the keystore dataset index
	uint64_t kr_dsobj; //dataset this keychain belongs to (index)
	dsl_keychain_t *kr_keychain; //keychain value of this record
} dsl_keychain_record_t;

//in memory structure for holding all keychains loaded into memory
typedef struct spa_keystore {
	krwlock_t sk_lock; //lock for protecting this trees below 
	avl_tree_t sk_keychains; //tree of all keychains, indexed by keychain obj
	avl_tree_t sk_keychain_index; //index of all keychains, by dataset obj
} spa_keystore_t;

void dsl_keychain_free(dsl_keychain_t *kc);
int dsl_keychain_alloc(dsl_keychain_t **kc_out);

int dsl_keychain_rewrap_nvlist(const char *dsname, nvlist_t *props);
int dsl_keychain_add_key(const char *dsname);
int dsl_keychain_lookup_key(dsl_keychain_t *kc, uint64_t txgid, zio_crypt_key_t **key_out);
void dsl_keychain_destroy(uint64_t kcobj, dmu_tx_t *tx);
int dsl_keychain_create_sync(zio_crypt_key_t *wkey, dmu_tx_t *tx, uint64_t *kcobj_out);
int dsl_keychain_clone_sync(uint64_t orig_kcobj, dmu_tx_t *tx, uint64_t *kcobj_out);
int dsl_keychain_open(objset_t *mos, uint64_t kcobj, uint8_t *wkeydata, uint_t wkeydata_len, dsl_keychain_t **kc_out);
zfs_keystatus_t dsl_keychain_keystatus(dsl_dataset_t *ds);

void spa_keystore_init(spa_keystore_t *sk);
void spa_keystore_fini(spa_keystore_t *sk);

int spa_keystore_lookup(spa_t *spa, uint64_t kcobj, dsl_keychain_t **kc_out);
int spa_keystore_insert(spa_t *spa, dsl_keychain_t *kc);
int spa_keystore_load(spa_t *spa, uint64_t kcobj, uint8_t *wkeydata, uint_t wkeydata_len);
int spa_keystore_unload(spa_t *spa, uint64_t kcobj);
int spa_keystore_lookup_index(spa_t *spa, uint64_t dsobj, dsl_keychain_t **kc_out);
int spa_keystore_create_index(spa_t *spa, uint64_t dsobj, uint64_t kcobj);
int spa_keystore_remove_index(spa_t *spa, uint64_t dsobj);

#endif