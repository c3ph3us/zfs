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

#ifndef	_SYS_ZIO_CRYPT_H
#define	_SYS_ZIO_CRYPT_H

#include <sys/refcount.h>
#include <sys/crypto/api.h>
#include <sys/nvpair.h>
#include <sys/avl.h>

#ifdef _KERNEL

#define	LOG_DEBUG(fmt, args...) \
	printk(KERN_DEBUG "debug: %s: %s %d: " \
	fmt "\n", __FILE__, __FUNCTION__, __LINE__, ## args)

#define	LOG_ERROR(error, fmt, args...) \
	printk(KERN_ERR "error: %s: %s %d: " \
	fmt ": %d\n",  __FILE__, __FUNCTION__, __LINE__, ## args, error)

#define	LOG_CRYPTO_PARAMS(dcp) \
	LOG_DEBUG("cp: crypt = %llu, wkey = 0x%p", \
	(unsigned long long)(dcp)->cp_crypt, (dcp)->cp_wkey)

#define	dump_stack() spl_dumpstack()

#else

#define	LOG_DEBUG(fmt, args...)
#define	LOG_ERROR(error, fmt, args...)
#define	LOG_CRYPTO_PARAMS(dcp)
#define	dump_stack()

#endif

/* macros defining key encryption lengths */
#define	MAX_KEY_LEN 32
#define	ZIO_CRYPT_WRAPKEY_IVLEN 13
#define	WRAPPING_MAC_LEN 16
#define	DATA_MAC_LEN 12

/* utility macros */
#define	BITS_TO_BYTES(x) (((x) + 7) >> 3)
#define	BYTES_TO_BITS(x) (x << 3)

/* supported commands for zfs_ioc_crypto() */
typedef enum zfs_ioc_crypto_cmd {
	ZFS_IOC_CRYPTO_CMD_NONE = 0,
	ZFS_IOC_CRYPTO_LOAD_KEY,
	ZFS_IOC_CRYPTO_UNLOAD_KEY,
	ZFS_IOC_CRYPTO_ADD_KEY,
	ZFS_IOC_CRYPTO_REWRAP,
} zfs_ioc_crypto_cmd_t;

/* supported encryption algorithms */
typedef enum zio_encrypt {
	ZIO_CRYPT_INHERIT = 0,
	ZIO_CRYPT_ON,
	ZIO_CRYPT_OFF,
	ZIO_CRYPT_AES_128_CCM,
	ZIO_CRYPT_AES_192_CCM,
	ZIO_CRYPT_AES_256_CCM,
	ZIO_CRYPT_AES_128_GCM,
	ZIO_CRYPT_AES_192_GCM,
	ZIO_CRYPT_AES_256_GCM,
	ZIO_CRYPT_FUNCTIONS
} zio_encrypt_t;

#define	ZIO_CRYPT_ON_VALUE	ZIO_CRYPT_AES_256_CCM
#define	ZIO_CRYPT_DEFAULT	ZIO_CRYPT_OFF

#define	WRAPPING_KEY_LEN 32

typedef enum zio_crypt_type
{
	ZC_TYPE_NONE = 0,
	ZC_TYPE_CCM,
	ZC_TYPE_GCM
} zio_crypt_type_t;

/* table of supported crypto algorithms, modes and keylengths. */
typedef struct zio_crypt_info
{
	/* mechanism name, needed by ICP */
	crypto_mech_name_t ci_mechname;

	/* cipher mode type (GCM, CCM) */
	zio_crypt_type_t ci_crypt_type;

	/* length of the encryption key */
	size_t ci_keylen;

	/* length of the IV paramter */
	size_t ci_ivlen;

	/* length of the output MAC parameter */
	size_t ci_maclen;

	/* human-readable name of the encryption alforithm */
	char *ci_name;
} zio_crypt_info_t;

extern zio_crypt_info_t zio_crypt_table[ZIO_CRYPT_FUNCTIONS];

/* in memory representation of an unwrapped key that is loaded into memory */
typedef struct zio_crypt_key
{
	/* encryption algorithm */
	zio_encrypt_t zk_crypt;

	/* illumos crypto api key representation */
	crypto_key_t zk_key;

	/* private data for illumos crypto api */
	crypto_ctx_template_t zk_ctx_tmpl;
} zio_crypt_key_t;

/* in memory representation of a wrapping key */
typedef struct dsl_wrapping_key
{
	/* link into the keystore's tree of wrapping keys */
	avl_node_t wk_avl_link;

	/* actual wrapping key */
	crypto_key_t wk_key;

	/* refcount of number of keychains holding this struct */
	refcount_t wk_refcnt;

	/* dsl directory object that owns this wrapping key */
	uint64_t wk_ddobj;
} dsl_wrapping_key_t;

/* structure for passing around encryption params from userspace */
typedef struct dsl_crypto_params
{
	/* command to be executed */
	zfs_ioc_crypto_cmd_t cp_cmd;

	/* the encryption algorithm */
	uint64_t cp_crypt;

	/* the salt, if the keysource is of type passphrase */
	uint64_t cp_salt;

	/* keysource property string */
	const char *cp_keysource;

	/* the wrapping key */
	dsl_wrapping_key_t *cp_wkey;
} dsl_crypto_params_t;

void zio_crypt_key_destroy(zio_crypt_key_t *key);
int zio_crypt_key_init(uint64_t crypt, uint8_t *keydata, zio_crypt_key_t *key);
void dsl_wrapping_key_hold(dsl_wrapping_key_t *wkey, void *tag);
void dsl_wrapping_key_rele(dsl_wrapping_key_t *wkey, void *tag);
void dsl_wrapping_key_free(dsl_wrapping_key_t *wkey);
int dsl_wrapping_key_create(uint8_t *wkeydata, dsl_wrapping_key_t **wkey_out);
int dsl_crypto_params_init_nvlist(nvlist_t *props, dsl_crypto_params_t *dcp);
void dsl_crypto_params_destroy(dsl_crypto_params_t *dcp);

int zio_do_crypt_raw(boolean_t encrypt, uint64_t crypt, crypto_key_t *key,
	crypto_ctx_template_t tmpl, uint8_t *ivbuf,	uint8_t *plainbuf,
	uint8_t *cipherbuf, uint_t datalen);
#define	zio_encrypt_raw(crypt, key, tmpl, iv, pd, cd, datalen) \
	zio_do_crypt_raw(B_TRUE, crypt, key, tmpl, iv, pd, cd, datalen)
#define	zio_decrypt_raw(crypt, key, tmpl, iv, pd, cd, datalen) \
	zio_do_crypt_raw(B_FALSE, crypt, key, tmpl, iv, pd, cd, datalen)

int zio_do_crypt_uio(boolean_t encrypt, uint64_t crypt, crypto_key_t *key,
	crypto_ctx_template_t tmpl, uint8_t *ivbuf, uint_t datalen,
	uio_t *puio, uio_t *cuio);
#define	zio_encrypt_uio(crypt, key, tmpl, ivbuf, datalen, pu, cu) \
	zio_do_crypt_uio(B_TRUE, crypt, key, tmpl, ivbuf, datalen, pu, cu)
#define	zio_decrypt_uio(crypt, key, tmpl, ivbuf, datalen, pu, cu) \
	zio_do_crypt_uio(B_FALSE, crypt, key, tmpl, ivbuf, datalen, pu, cu)

#endif
