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

#ifdef _KERNEL
	#define LOG_DEBUG(fmt, args...) printk(KERN_DEBUG "debug: %s: %s %d: " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ## args)
	#define LOG_ERROR(error, fmt, args...) printk(KERN_ERR "error: %s: %s %d: " fmt ": %d\n",  __FILE__, __FUNCTION__, __LINE__, ## args, error)
	#define PRINT_ZKEY(key, fmt, args...) printk(KERN_DEBUG "debug: " fmt ": crypt = %d, tmpl = %p, refcount = %d\n", ## args, (int)key->zk_crypt, key->zk_ctx_tmpl, (int)refcount_count(&key->zk_refcnt))
	#define dump_stack() spl_dumpstack()
#else
	#define LOG_DEBUG(fmt, args...)
	#define LOG_ERROR(error, fmt, args...)
	#define PRINT_ZKEY(key, fmt, args...)
	#define dump_stack()
#endif

//utility macros
#define BITS_TO_BYTES(x) (((x) + 7) >> 3)
#define BYTES_TO_BITS(x) (x << 3)

typedef enum zfs_ioc_crypto_cmd {
	ZFS_IOC_CRYPTO_CMD_NONE = 0,
	ZFS_IOC_CRYPTO_LOAD_KEY,
	ZFS_IOC_CRYPTO_UNLOAD_KEY,
	ZFS_IOC_CRYPTO_ADD_KEY,
	ZFS_IOC_CRYPTO_REWRAP,
} zfs_ioc_crypto_cmd_t;

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

typedef enum zio_crypt_type {
	ZIO_CRYPT_TYPE_NONE = 0,
	ZIO_CRYPT_TYPE_CCM,
	ZIO_CRYPT_TYPE_GCM
} zio_encrypt_type_t;

//table of supported crypto algorithms, modes and keylengths.
typedef struct zio_crypt_info {
	crypto_mech_name_t	ci_mechname;
	zio_encrypt_type_t	ci_crypt_type;
	size_t			ci_keylen;
	size_t			ci_ivlen;
	size_t			ci_maclen;
	size_t			ci_zil_maclen;
	char			*ci_name;
} zio_crypt_info_t;

extern zio_crypt_info_t zio_crypt_table[ZIO_CRYPT_FUNCTIONS];

//in memory representation of an unwrapped key that is loaded into memory
typedef struct zio_crypt_key {
	enum zio_encrypt zk_crypt; //encryption algorithm
	crypto_key_t zk_key; //illumos crypto api key representation
	crypto_ctx_template_t zk_ctx_tmpl; //private data for illumos crypto api
	refcount_t zk_refcnt; //refcount
} zio_crypt_key_t;

void zio_crypt_key_hold(zio_crypt_key_t *key, void *tag);
void zio_crypt_key_rele(zio_crypt_key_t *key, void *tag);
int zio_crypt_key_create(uint64_t crypt, uint8_t *keydata, void *tag, zio_crypt_key_t **key_out);
int zio_crypt_wkey_create_nvlist(nvlist_t *props, void *tag, zio_crypt_key_t **key_out);

int zio_do_crypt(boolean_t encrypt, zio_crypt_key_t *key, uint8_t *ivbuf, uint_t ivlen, uint_t maclen, uint8_t *plainbuf, uint8_t *cipherbuf, uint_t datalen);
#define zio_encrypt(wkey, iv, ivlen, maclen, plaindata, cipherdata, keylen) zio_do_crypt(B_TRUE, wkey, iv, ivlen, maclen, plaindata, cipherdata, keylen)
#define zio_decrypt(wkey, iv, ivlen, maclen, plaindata, cipherdata, keylen) zio_do_crypt(B_FALSE, wkey, iv, ivlen, maclen, plaindata, cipherdata, keylen)

#endif
