#ifndef INCLUDE_UNMAP_H
#define INCLUDE_UNMAP_H

#include <stdlib.h>

#define UNMAP_HEAP_ARRAY_SIZE			(2)
#define UNMAP_HEAP_EXTENSION_SIZE(n)	((n) * 2)
#define UNMAP_TREE_BRANCH				(0x10)
#define UNMAP_TREE_FILTER				(UNMAP_TREE_BRANCH - 1)
#define UNMAP_TREE_NEXT(n)				((n) -= 4)

/* 型の種類 */
#define UNMAP_TYPE_NONE					(0x00)
#define UNMAP_TYPE_DATA					(0x01)
#define UNMAP_TYPE_TREE					(0x02)

/* 型情報を取得 */
#define UNMAP_TYPE_GET(tree, level)					\
	(((tree)->type >> ((level) * 2)) & 0x03)

/* 型情報を設定 */
#define UNMAP_TYPE_SET(tree, level, t)				\
	do {											\
		(tree)->type &= (~(0x03 << ((level) * 2)));	\
		(tree)->type |= ((t) << ((level) * 2));		\
	} while(0)

#define unmap_free(unmap, free_func)				\
	do { unmap_free_func((unmap), (free_func)); (unmap) = NULL; } while(0)

/* 32(64)bitハッシュ値 */
typedef unsigned long unmap_hash_t;

/* 木構造 */
typedef struct unmap_tree_st {
	unsigned int type;				/* 枝の型を管理(2bitずつ) */
	void *tree[UNMAP_TREE_BRANCH];	/* 枝を管理 */
} unmap_tree_t;

/* 連結リスト */
typedef struct unmap_data_st {
	unmap_hash_t hash;			/* ハッシュ値 */
	void *data;					/* 保存データ */
} unmap_data_t;

/* メモリ空間管理 */
typedef struct unmap_storage_st {
	void **heap;				/* メモリ */
	size_t heap_size;			/* 一度に確保するサイズ */
	size_t list_size;			/* 確保する最大回数 */
	size_t list_index;			/* 参照しているメモリブロック */
	size_t data_index;			/* 参照しているメモリアドレス */
	size_t type_size;			/* 管理する型のサイズ */
} unmap_storage_t;

/* unmap情報管理 */
typedef struct unmap_st {
	unmap_storage_t *tree_heap;	/* メモリ空間ツリー用 */
	unmap_storage_t *data_heap;	/* メモリ空間データ用 */
	unmap_tree_t *tree;			/* 最初の枝 */
} unmap_t;

/* プロトタイプ宣言 */
extern unmap_t *unmap_init(void);
extern void unmap_free_func(unmap_t *list, void (*free_func)(void *));
extern int unmap_set(unmap_t *list, const char *key, size_t key_size, void *data);
extern void *unmap_get(unmap_t *list, const char *key, size_t key_size);
extern void *unmap_find(unmap_t *list, const char *key, size_t key_size);
extern size_t unmap_size(unmap_t *list);
extern void *unmap_at(unmap_t *list, size_t at);

#endif /* INCLUDE_UNMAP_H */
