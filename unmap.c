#include <stdio.h>
#include <string.h>

#include "unmap.h"
#include "crc32.h"

/* プロトタイプ宣言 */
static void *unmap_malloc(size_t size);
static void *unmap_realloc(void *p, size_t size, size_t len);
static unmap_storage_t *unmap_storage_init(unmap_t *list, size_t tsize, size_t hsize);
static void *unmap_storage_alloc(unmap_storage_t *st);
static void unmap_storage_free(unmap_storage_t *st);
static void unmap_storage_data_free(unmap_storage_t *st, void (*free_func)(void *));
static unmap_data_t *unmap_area_get(unmap_t *list, unmap_tree_t *tree, unmap_box_t *box, int level);
static unmap_data_t *unmap_area_find(unmap_t *list, unmap_tree_t *tree, unmap_box_t *box, int level);
static unmap_data_t *unmap_data_next(unmap_t *list, unmap_data_t *data, unmap_box_t *box);
static unmap_data_t *unmap_data_next_find(unmap_t *list, unmap_data_t *data, unmap_box_t *box);
static unmap_data_t *unmap_data_get(unmap_t *list, const char *key, size_t key_size);

/* unmap_tオブジェクト生成・初期化 */
unmap_t *unmap_init(int max_level)
{
	unmap_t *list = 0;
	/* 初期値設定 */
	size_t tree_heap_size = 0x40;
	size_t data_heap_size = 0x100;

	if((max_level > sizeof(int) * 8) || (max_level < 4)){
		/* 33bit以上3bit以下の場合 */
		return NULL;
	}
	/* unmap_tオブジェクト確保 */
	list = unmap_malloc(sizeof(unmap_t));
	/* tree領域確保 */
	list->tree_heap = unmap_storage_init(list, sizeof(unmap_tree_t), tree_heap_size);
	list->tree = (unmap_tree_t *)(list->tree_heap->heap[0]);
	list->tree_heap->data_index = 1;
	/* data領域確保 */
	list->data_heap = unmap_storage_init(list, sizeof(unmap_data_t), data_heap_size);
	/* 最大深度セット */
	list->max_level = max_level;
	return list;
}

/* unmap_tオブジェクト開放 */
void unmap_free_func(unmap_t *list, void (*free_func)(void *))
{
	if((list == NULL) || (list->tree == NULL)) return;
	/* tree開放 */
	unmap_storage_free(list->tree_heap);
	list->tree_heap = NULL;
	/* data開放 */
	unmap_storage_data_free(list->data_heap, free_func);
	unmap_storage_free(list->data_heap);
	list->data_heap = NULL;
	/* unmap_t開放 */
	free(list);
}

/* unmap_tオブジェクトに値をセットする */
int unmap_set(unmap_t *list, const char *key, size_t key_size, void *data)
{
	unmap_data_t *tmp = unmap_data_get(list, key, key_size);
	if(tmp == NULL){
		return -1;
	}
	tmp->data = data;
	return 0;
}

/* unmap_tオブジェクトから値を取得 */
void *unmap_get(unmap_t *list, const char *key, size_t key_size)
{
	unmap_data_t *tmp = unmap_data_get(list, key, key_size);
	if(tmp == NULL){
		return NULL;
	}
	return tmp->data;
}

/* unmap_tオブジェクトから値を取得 */
void *unmap_find(unmap_t *list, const char *key, size_t key_size)
{
	unmap_data_t *tmp = 0;
	void *data = 0;
	unmap_box_t box = {0};
	if((list == NULL) || (key == NULL) || (key_size == 0)){
		return NULL;
	}
	/* hashを計算 */
	box = unmap_hash_create(key, key_size, list->max_level);
	/* 検索 */
	tmp = unmap_area_find(list, list->tree, &box, list->max_level);
	if(tmp != NULL){
		if(tmp->box.hash != box.hash){
			/* 連結リストをたどる */
			tmp = unmap_data_next_find(list, tmp, &box);
			if(tmp != NULL){
				data = tmp->data;
			}
		} else {
			data = tmp->data;
		}
	}
	return data;
}

/* 格納しているデータ数を返す */
size_t unmap_size(unmap_t *list)
{
	unmap_storage_t *st = 0;
	size_t size = 0;
	if((list == NULL) || (list->data_heap == NULL)){
		return 0;
	}
	st = list->data_heap;
	size = (st->heap_size * st->list_index) + st->data_index;
	return size;
}

/* 要素数指定取得 */
void *unmap_at(unmap_t *list, size_t at)
{
	unmap_storage_t *st = 0;
	unmap_data_t *data = 0;
	size_t size = unmap_size(list);
	char *p = 0;
	void *ret = 0;

	if(at < size){
		/* 格納数よりも少ない場合 */
		st = list->data_heap;
		p = st->heap[at / st->heap_size];
		data = (unmap_data_t *)(p + (st->type_size * (at % st->heap_size)));
		ret = data->data;
	}
	return ret;
}

/* エラー処理付きmalloc */
static void *unmap_malloc(size_t size)
{
	void *p = malloc(size);
	if(p == NULL){
		perror("unmap_malloc");
	} else {
		memset(p, 0, size);
	}
	return p;
}

/* エラー処理付きrealloc */
static void *unmap_realloc(void *p, size_t size, size_t len)
{
	p = realloc(p, size);
	if(p == NULL){
		perror("unmap_realloc");
	} else {
		len++;
		if(size > len){
			memset(((char *)p) + len, 0, size - len);
		}
	}
	return p;
}

/* ストレージ初期化 */
static unmap_storage_t *unmap_storage_init(unmap_t *list, size_t tsize, size_t hsize)
{
	unmap_storage_t *st = unmap_malloc(sizeof(unmap_storage_t));
	char *p = 0;
	size_t i = 0;
	size_t block = 0;
	st->list_size = UNMAP_HEAP_ARRAY_SIZE;	/* 一列の長さ */
	st->heap_size = hsize;	/* 一行の長さ(不変) */
	st->type_size = tsize;	/* 型の大きさ */
	block = st->type_size * st->heap_size;
	st->heap = unmap_malloc(sizeof(void *) * st->list_size);
	st->heap[0] = unmap_malloc(block * st->list_size);
	p = st->heap[0];
	for(i = 1; i < st->list_size; i++){
		st->heap[i] = p + (block * i);
	}
	return st;
}

/* ストレージの切り分け */
static void *unmap_storage_alloc(unmap_storage_t *st)
{
	char *p = 0;
	if(st->data_index >= st->heap_size){
		/* 用意した領域を使い切った */
		st->data_index = 0;	/* 先頭を指し直す */
		st->list_index++;	/* 次の領域へ */
		if(st->list_index >= st->list_size){
			size_t i = 0;
			size_t len = 0;
			size_t size = st->type_size * st->heap_size;	/* 一塊の大きさ */
			/* 領域管理配列を拡張する */
			st->list_size = UNMAP_HEAP_EXTENSION_SIZE(st->list_size);
			st->heap = unmap_realloc(st->heap, sizeof(void *) * st->list_size, sizeof(void *) * st->list_index);
			/* 新しい領域を確保する */
			len = st->list_size - st->list_index;
			st->heap[st->list_index] = unmap_malloc(size * len);
			p = st->heap[st->list_index];
			for(i = 1; i < len; i++){
				st->heap[st->list_index + i] = p + (size * i);
			}
		}
	}
	p = st->heap[st->list_index];
	/* 使用していない領域を返す */
	return p + (st->type_size * st->data_index++);
}

/* unmap_storage_tの解放 */
static void unmap_storage_free(unmap_storage_t *st)
{
	size_t i = 0;
	size_t list_size = st->list_size;
	free(st->heap[0]);
	for(i = UNMAP_HEAP_ARRAY_SIZE; i < list_size; i = UNMAP_HEAP_EXTENSION_SIZE(i)){
		free(st->heap[i]);
	}
	free(st->heap);
	free(st);
}

/* unmap_storage_t (unmap_data_t) の解放 */
static void unmap_storage_data_free(unmap_storage_t *st, void (*free_func)(void *))
{
	size_t i = 0;
	size_t j = 0;
	size_t list_size = 0;
	size_t heap_size = 0;
	size_t type_size = 0;
	void **p = 0;
	unmap_data_t *data = 0;
	if(st == NULL){
		return;
	}
	if(free_func != NULL){
		/* 解放用関数がある場合 */
		list_size = st->list_size;
		heap_size = st->heap_size;
		type_size = st->type_size;
		p = st->heap;
		for(j = 0; j < list_size; j++){
			for(i = 0; i < heap_size; i++){
				data = (unmap_data_t *)((char *)p[j] + (type_size * i));
				if(data->data != NULL){
					/* データが格納されている */
					free_func(data->data);
				}
				data->data = NULL;
			}
		}
	}
}

/* unmap_tツリーを生成、値の格納場所を用意 */
static unmap_data_t *unmap_area_get(unmap_t *list, unmap_tree_t *tree, unmap_box_t *box, int level)
{
	size_t rl = 0;
	size_t rl2 = 0;
	size_t node = box->node;
	unmap_data_t *data = 0;
	for(UNMAP_TREE_NEXT(level); level > 0; UNMAP_TREE_NEXT(level)){
		rl = (node >> level) & UNMAP_TREE_FILTER;	/* 方向選択 */
		switch(UNMAP_TYPE_GET(tree, rl)){
		case UNMAP_TYPE_TREE:
			tree = tree->tree[rl];
			break;
		case UNMAP_TYPE_DATA:
			data = tree->tree[rl];
			if(node == data->box.node){
				return data;
			} else {
				/* unmap_tree_tオブジェクト生成 */
				tree->tree[rl] = unmap_storage_alloc(list->tree_heap);
				UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_TREE);
				tree = tree->tree[rl];
			}
			break;
		default:
			if(data != NULL){
				rl2 = (data->box.node >> level) & UNMAP_TREE_FILTER;
				if(rl == rl2){
					/* unmap_tree_tオブジェクト生成 */
					tree->tree[rl] = unmap_storage_alloc(list->tree_heap);
					UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_TREE);
					tree = tree->tree[rl];
					break;
				} else {
					/* 新しいtree上でのみ通過する */
					tree->tree[rl2] = data;
					UNMAP_TYPE_SET(tree, rl2, UNMAP_TYPE_DATA);
				}
			}
			/* unmap_data_tオブジェクト生成 */
			data = unmap_storage_alloc(list->data_heap);
			data->box = *box;
			tree->tree[rl] = data;
			UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_DATA);
			return data;
		}
	}
	rl = node & UNMAP_TREE_FILTER;	/* 方向選択 */
	/* 最深部 */
	if(data != NULL){
		rl2 = data->box.node & UNMAP_TREE_FILTER;
		if(rl == rl2){
			tree->tree[rl] = data;
			UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_DATA);
		} else {
			tree->tree[rl2] = data;
			UNMAP_TYPE_SET(tree, rl2, UNMAP_TYPE_DATA);
			/* unmap_data_tオブジェクト生成 */
			data = unmap_storage_alloc(list->data_heap);
			data->box = *box;
			tree->tree[rl] = data;
			UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_DATA);
		}
	} else {
		if(UNMAP_TYPE_GET(tree, rl) == UNMAP_TYPE_DATA){
			data = tree->tree[rl];
		} else {
			/* unmap_data_tオブジェクト生成 */
			data = unmap_storage_alloc(list->data_heap);
			data->box = *box;
			tree->tree[rl] = data;
			UNMAP_TYPE_SET(tree, rl, UNMAP_TYPE_DATA);
		}
	}
	/* unmap_data_tオブジェクトを返す */
	return data;
}

/* unmap_tツリーを探索 */
static unmap_data_t *unmap_area_find(unmap_t *list, unmap_tree_t *tree, unmap_box_t *box, int level)
{
	size_t rl = 0;
	size_t rl2 = 0;
	size_t node = box->node;
	unmap_data_t *data = 0;
	for(UNMAP_TREE_NEXT(level); level > 0; UNMAP_TREE_NEXT(level)){
		rl = (node >> level) & UNMAP_TREE_FILTER;	/* 方向選択 */
		switch(UNMAP_TYPE_GET(tree, rl)){
		case UNMAP_TYPE_TREE:
			tree = tree->tree[rl];
			break;
		case UNMAP_TYPE_DATA:
			data = tree->tree[rl];
			if(node != data->box.node){
				data = NULL;
			}
			return data;
		default:
			return NULL;
		}
	}
	rl = node & UNMAP_TREE_FILTER;	/* 方向選択 */
	/* 最深部 */
	if(UNMAP_TYPE_GET(tree, rl) == UNMAP_TYPE_DATA){
		data = tree->tree[rl];
	}
	/* unmap_data_tオブジェクトを返す */
	return data;
}

/* 連結リストをたどる */
static unmap_data_t *unmap_data_next(unmap_t *list, unmap_data_t *data, unmap_box_t *box)
{
	unmap_data_t *tmp = data->next;
	unmap_data_t *back = data;
	while(tmp != NULL){
		if(tmp->box.hash == box->hash){
			/* それっぽいものを発見 */
			return tmp;
		} else {
			/* 次へ */
			back = tmp;
			tmp = tmp->next;
		}
	}
	/* unmap_data_tオブジェクト生成 */
	tmp = (unmap_data_t *)unmap_storage_alloc(list->data_heap);
	tmp->box = *box;
	back->next = tmp;
	return tmp;
}

/* 連結リストをたどる */
static unmap_data_t *unmap_data_next_find(unmap_t *list, unmap_data_t *data, unmap_box_t *box)
{
	unmap_data_t *tmp = data->next;
	while(tmp != NULL){
		if(tmp->box.hash == box->hash){
			/* それっぽいものを発見 */
			return tmp;
		} else {
			/* 次へ */
			tmp = tmp->next;
		}
	}
	return tmp;
}

/* 共通処理の関数化 */
static unmap_data_t *unmap_data_get(unmap_t *list, const char *key, size_t key_size)
{
	unmap_data_t *tmp = 0;
	unmap_box_t box = {0};
	if((list == NULL) || (key == NULL) || (key_size == 0)){
		return NULL;
	}
	/* hashを計算する */
	box = unmap_hash_create(key, key_size, list->max_level);
	tmp = unmap_area_get(list, list->tree, &box, list->max_level);
	if(tmp->box.hash != box.hash){
		/* 連結リストをたどる */
		tmp = unmap_data_next(list, tmp, &box);
	}
	return tmp;
}

